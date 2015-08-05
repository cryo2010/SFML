////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2015 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/RenderTextureImplFBO.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/System/Err.hpp>

#ifdef SFML_OPENGL_ES

    #include <SFML/OpenGL.hpp>

#endif // SFML_OPENGL_ES

#define GLEXT_GL_FRAMEBUFFER                   0x8D40
#define GLEXT_GL_RENDERBUFFER                  0x8D41
#define GLEXT_GL_COLOR_ATTACHMENT0             0x8CE0
#define GLEXT_GL_DEPTH_ATTACHMENT              0x8D00
#define GLEXT_GL_FRAMEBUFFER_COMPLETE          0x8CD5
#define GLEXT_GL_FRAMEBUFFER_BINDING           0x8CA6
#define GLEXT_GL_INVALID_FRAMEBUFFER_OPERATION 0x0506


namespace
{
#ifndef SFML_OPENGL_ES

    bool GLEXT_framebuffer_object = false;

    void (GL_FUNCPTR *GLEXT_glBindFramebuffer)(GLenum, GLuint) = NULL;
    void (GL_FUNCPTR *GLEXT_glBindRenderbuffer)(GLenum, GLuint) = NULL;
    GLenum (GL_FUNCPTR *GLEXT_glCheckFramebufferStatus)(GLenum) = NULL;
    void (GL_FUNCPTR *GLEXT_glDeleteFramebuffers)(GLsizei, const GLuint *) = NULL;
    void (GL_FUNCPTR *GLEXT_glDeleteRenderbuffers)(GLsizei, const GLuint *) = NULL;
    void (GL_FUNCPTR *GLEXT_glFramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint) = NULL;
    void (GL_FUNCPTR *GLEXT_glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = NULL;
    void (GL_FUNCPTR *GLEXT_glGenFramebuffers)(GLsizei, GLuint *) = NULL;
    void (GL_FUNCPTR *GLEXT_glGenRenderbuffers)(GLsizei, GLuint *) = NULL;
    void (GL_FUNCPTR *GLEXT_glRenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei) = NULL;

#else

    #define GLEXT_framebuffer_object        true
    #define GLEXT_glBindRenderbuffer        glBindRenderbufferOES
    #define GLEXT_glDeleteRenderbuffers     glDeleteRenderbuffersOES
    #define GLEXT_glGenRenderbuffers        glGenRenderbuffersOES
    #define GLEXT_glRenderbufferStorage     glRenderbufferStorageOES
    #define GLEXT_glBindFramebuffer         glBindFramebufferOES
    #define GLEXT_glDeleteFramebuffers      glDeleteFramebuffersOES
    #define GLEXT_glGenFramebuffers         glGenFramebuffersOES
    #define GLEXT_glCheckFramebufferStatus  glCheckFramebufferStatusOES
    #define GLEXT_glFramebufferTexture2D    glFramebufferTexture2DOES
    #define GLEXT_glFramebufferRenderbuffer glFramebufferRenderbufferOES

#endif // SFML_OPENGL_ES

    void ensureExtensionInit()
    {
#ifndef SFML_OPENGL_ES

        static bool loaded = false;

        if (loaded)
            return;

        loaded = true;

        GLEXT_framebuffer_object = sf::Context::isExtensionAvailable("GL_EXT_framebuffer_object");

        if (!GLEXT_framebuffer_object)
            return;

        GLEXT_glBindFramebuffer = (void (GL_FUNCPTR *)(GLenum, GLuint))sf::Context::getFunction("glBindFramebufferEXT");
        GLEXT_glBindRenderbuffer = (void (GL_FUNCPTR *)(GLenum, GLuint))sf::Context::getFunction("glBindRenderbufferEXT");
        GLEXT_glCheckFramebufferStatus = (GLenum (GL_FUNCPTR *)(GLenum))sf::Context::getFunction("glCheckFramebufferStatusEXT");
        GLEXT_glDeleteFramebuffers = (void (GL_FUNCPTR *)(GLsizei, const GLuint *))sf::Context::getFunction("glDeleteFramebuffersEXT");
        GLEXT_glDeleteRenderbuffers = (void (GL_FUNCPTR *)(GLsizei, const GLuint *))sf::Context::getFunction("glDeleteRenderbuffersEXT");
        GLEXT_glFramebufferRenderbuffer = (void (GL_FUNCPTR *)(GLenum, GLenum, GLenum, GLuint))sf::Context::getFunction("glFramebufferRenderbufferEXT");
        GLEXT_glFramebufferTexture2D = (void (GL_FUNCPTR *)(GLenum, GLenum, GLenum, GLuint, GLint))sf::Context::getFunction("glFramebufferTexture2DEXT");
        GLEXT_glGenFramebuffers = (void (GL_FUNCPTR *)(GLsizei, GLuint *))sf::Context::getFunction("glGenFramebuffersEXT");
        GLEXT_glGenRenderbuffers = (void (GL_FUNCPTR *)(GLsizei, GLuint *))sf::Context::getFunction("glGenRenderbuffersEXT");
        GLEXT_glRenderbufferStorage = (void (GL_FUNCPTR *)(GLenum, GLenum, GLsizei, GLsizei))sf::Context::getFunction("glRenderbufferStorageEXT");

        if (!GLEXT_glBindFramebuffer ||
            !GLEXT_glBindRenderbuffer ||
            !GLEXT_glCheckFramebufferStatus ||
            !GLEXT_glDeleteFramebuffers ||
            !GLEXT_glDeleteRenderbuffers ||
            !GLEXT_glFramebufferRenderbuffer ||
            !GLEXT_glFramebufferTexture2D ||
            !GLEXT_glGenFramebuffers ||
            !GLEXT_glGenRenderbuffers ||
            !GLEXT_glRenderbufferStorage)
            GLEXT_framebuffer_object = false;

#endif // SFML_OPENGL_ES
    }
}

namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
RenderTextureImplFBO::RenderTextureImplFBO() :
m_frameBuffer(0),
m_depthBuffer(0)
{

}


////////////////////////////////////////////////////////////
RenderTextureImplFBO::~RenderTextureImplFBO()
{
    ensureGlContext();

    // Destroy the depth buffer
    if (m_depthBuffer)
    {
        GLuint depthBuffer = static_cast<GLuint>(m_depthBuffer);
        glCheck(GLEXT_glDeleteRenderbuffers(1, &depthBuffer));
    }

    // Destroy the frame buffer
    if (m_frameBuffer)
    {
        GLuint frameBuffer = static_cast<GLuint>(m_frameBuffer);
        glCheck(GLEXT_glDeleteFramebuffers(1, &frameBuffer));
    }

    // Delete the context
    delete m_context;
}


////////////////////////////////////////////////////////////
bool RenderTextureImplFBO::isAvailable()
{
    ensureGlContext();

    // Make sure that extensions are initialized
    ensureExtensionInit();

    return GLEXT_framebuffer_object != 0;
}


////////////////////////////////////////////////////////////
bool RenderTextureImplFBO::create(unsigned int width, unsigned int height, unsigned int textureId, bool depthBuffer)
{
    // Create the context
    m_context = new Context;

    // Create the framebuffer object
    GLuint frameBuffer = 0;
    glCheck(GLEXT_glGenFramebuffers(1, &frameBuffer));
    m_frameBuffer = static_cast<unsigned int>(frameBuffer);
    if (!m_frameBuffer)
    {
        err() << "Impossible to create render texture (failed to create the frame buffer object)" << std::endl;
        return false;
    }
    glCheck(GLEXT_glBindFramebuffer(GLEXT_GL_FRAMEBUFFER, m_frameBuffer));

    // Create the depth buffer if requested
    if (depthBuffer)
    {
        GLuint depth = 0;
        glCheck(GLEXT_glGenRenderbuffers(1, &depth));
        m_depthBuffer = static_cast<unsigned int>(depth);
        if (!m_depthBuffer)
        {
            err() << "Impossible to create render texture (failed to create the attached depth buffer)" << std::endl;
            return false;
        }
        glCheck(GLEXT_glBindRenderbuffer(GLEXT_GL_RENDERBUFFER, m_depthBuffer));
        glCheck(GLEXT_glRenderbufferStorage(GLEXT_GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height));
        glCheck(GLEXT_glFramebufferRenderbuffer(GLEXT_GL_FRAMEBUFFER, GLEXT_GL_DEPTH_ATTACHMENT, GLEXT_GL_RENDERBUFFER, m_depthBuffer));
    }

    // Link the texture to the frame buffer
    glCheck(GLEXT_glFramebufferTexture2D(GLEXT_GL_FRAMEBUFFER, GLEXT_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0));

    // A final check, just to be sure...
    GLenum status = glCheck(GLEXT_glCheckFramebufferStatus(GLEXT_GL_FRAMEBUFFER));
    if (status != GLEXT_GL_FRAMEBUFFER_COMPLETE)
    {
        glCheck(GLEXT_glBindFramebuffer(GLEXT_GL_FRAMEBUFFER, 0));
        err() << "Impossible to create render texture (failed to link the target texture to the frame buffer)" << std::endl;
        return false;
    }

    return true;
}


////////////////////////////////////////////////////////////
bool RenderTextureImplFBO::activate(bool active)
{
    return m_context->setActive(active);
}


////////////////////////////////////////////////////////////
void RenderTextureImplFBO::updateTexture(unsigned int)
{
    glCheck(glFlush());
}

} // namespace priv

} // namespace sf
