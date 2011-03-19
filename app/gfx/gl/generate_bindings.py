#!/usr/bin/python
#
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""code generator for GL/GLES extension wrangler."""

import os
import re
import sys

GL_FUNCTIONS = [
['void', ['glActiveTexture'], 'GLenum texture'],
['void', ['glAttachShader'], 'GLuint program, GLuint shader'],
['void', ['glBindAttribLocation'],
    'GLuint program, GLuint index, const char* name'],
['void', ['glBindBuffer'], 'GLenum target, GLuint buffer'],
['void', ['glBindFramebufferEXT', 'glBindFramebuffer'],
    'GLenum target, GLuint framebuffer'],
['void', ['glBindRenderbufferEXT', 'glBindRenderbuffer'],
    'GLenum target, GLuint renderbuffer'],
['void', ['glBindTexture'], 'GLenum target, GLuint texture'],
['void', ['glBlendColor'],
    'GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha'],
['void', ['glBlendEquation'], ' GLenum mode '],
['void', ['glBlendEquationSeparate'], 'GLenum modeRGB, GLenum modeAlpha'],
['void', ['glBlendFunc'], 'GLenum sfactor, GLenum dfactor'],
['void', ['glBlendFuncSeparate'],
    'GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha'],
['void', ['glBlitFramebufferEXT', 'BlitFramebuffer'],
    'GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, '
    'GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, '
    'GLbitfield mask, GLenum filter'],
['void', ['glBlitFramebufferANGLE', 'BlitFramebuffer'],
    'GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, '
    'GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, '
    'GLbitfield mask, GLenum filter'],
['void', ['glBufferData'],
    'GLenum target, GLsizei size, const void* data, GLenum usage'],
['void', ['glBufferSubData'],
    'GLenum target, GLint offset, GLsizei size, const void* data'],
['GLenum', ['glCheckFramebufferStatusEXT',
    'glCheckFramebufferStatus'], 'GLenum target'],
['void', ['glClear'], 'GLbitfield mask'],
['void', ['glClearColor'],
    'GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha'],
['void', ['glClearDepth'], 'GLclampd depth'],
['void', ['glClearDepthf'], 'GLclampf depth'],
['void', ['glClearStencil'], 'GLint s'],
['void', ['glColorMask'],
    'GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha'],
['void', ['glCompileShader'], 'GLuint shader'],
['void', ['glCompressedTexImage2D'],
    'GLenum target, GLint level, GLenum internalformat, GLsizei width, '
    'GLsizei height, GLint border, GLsizei imageSize, const void* data'],
['void', ['glCompressedTexSubImage2D'],
    'GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, '
    'GLsizei height, GLenum format, GLsizei imageSize, const void* data'],
['void', ['glCopyTexImage2D'],
    'GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, '
    'GLsizei width, GLsizei height, GLint border'],
['void', ['glCopyTexSubImage2D'], 'GLenum target, GLint level, GLint xoffset, '
    'GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height'],
['GLuint', ['glCreateProgram'], 'void'],
['GLuint', ['glCreateShader'], 'GLenum type'],
['void', ['glCullFace'], 'GLenum mode'],
['void', ['glDeleteBuffersARB', 'glDeleteBuffers'],
    'GLsizei n, const GLuint* buffers'],
['void', ['glDeleteFramebuffersEXT', 'glDeleteFramebuffers'],
    'GLsizei n, const GLuint* framebuffers'],
['void', ['glDeleteProgram'], 'GLuint program'],
['void', ['glDeleteRenderbuffersEXT', 'glDeleteRenderbuffers'],
    'GLsizei n, const GLuint* renderbuffers'],
['void', ['glDeleteShader'], 'GLuint shader'],
['void', ['glDeleteTextures'], 'GLsizei n, const GLuint* textures'],
['void', ['glDepthFunc'], 'GLenum func'],
['void', ['glDepthMask'], 'GLboolean flag'],
['void', ['glDepthRange'], 'GLclampd zNear, GLclampd zFar'],
['void', ['glDepthRangef'], 'GLclampf zNear, GLclampf zFar'],
['void', ['glDetachShader'], 'GLuint program, GLuint shader'],
['void', ['glDisable'], 'GLenum cap'],
['void', ['glDisableVertexAttribArray'], 'GLuint index'],
['void', ['glDrawArrays'], 'GLenum mode, GLint first, GLsizei count'],
['void', ['glDrawElements'],
    'GLenum mode, GLsizei count, GLenum type, const void* indices'],
['void', ['glEnable'], 'GLenum cap'],
['void', ['glEnableVertexAttribArray'], 'GLuint index'],
['void', ['glFinish'], 'void'],
['void', ['glFlush'], 'void'],
['void', ['glFramebufferRenderbufferEXT', 'glFramebufferRenderbuffer'],
    'GLenum target, GLenum attachment, GLenum renderbuffertarget, '
    'GLuint renderbuffer'],
['void', ['glFramebufferTexture2DEXT', 'glFramebufferTexture2D'],
    'GLenum target, GLenum attachment, GLenum textarget, GLuint texture, '
    'GLint level'],
['void', ['glFrontFace'], 'GLenum mode'],
['void', ['glGenBuffersARB', 'glGenBuffers'], 'GLsizei n, GLuint* buffers'],
['void', ['glGenerateMipmapEXT', 'glGenerateMipmap'], 'GLenum target'],
['void', ['glGenFramebuffersEXT', 'glGenFramebuffers'],
    'GLsizei n, GLuint* framebuffers'],
['void', ['glGenRenderbuffersEXT', 'glGenRenderbuffers'],
    'GLsizei n, GLuint* renderbuffers'],
['void', ['glGenTextures'], 'GLsizei n, GLuint* textures'],
['void', ['glGetActiveAttrib'],
    'GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, '
    'GLint* size, GLenum* type, char* name'],
['void', ['glGetActiveUniform'],
    'GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, '
    'GLint* size, GLenum* type, char* name'],
['void', ['glGetAttachedShaders'],
    'GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders'],
['GLint', ['glGetAttribLocation'], 'GLuint program, const char* name'],
['void', ['glGetBooleanv'], 'GLenum pname, GLboolean* params'],
['void', ['glGetBufferParameteriv'],
    'GLenum target, GLenum pname, GLint* params'],
['GLenum', ['glGetError'], 'void'],
['void', ['glGetFloatv'], 'GLenum pname, GLfloat* params'],
['void', ['glGetFramebufferAttachmentParameterivEXT',
    'glGetFramebufferAttachmentParameteriv'], 'GLenum target, '
    'GLenum attachment, GLenum pname, GLint* params'],
['void', ['glGetIntegerv'], 'GLenum pname, GLint* params'],
['void', ['glGetProgramiv'], 'GLuint program, GLenum pname, GLint* params'],
['void', ['glGetProgramInfoLog'],
    'GLuint program, GLsizei bufsize, GLsizei* length, char* infolog'],
['void', ['glGetRenderbufferParameterivEXT', 'glGetRenderbufferParameteriv'],
    'GLenum target, GLenum pname, GLint* params'],
['void', ['glGetShaderiv'], 'GLuint shader, GLenum pname, GLint* params'],
['void', ['glGetShaderInfoLog'],
    'GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog'],
['void', ['glGetShaderPrecisionFormat'],
    'GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision'],
['void', ['glGetShaderSource'],
    'GLuint shader, GLsizei bufsize, GLsizei* length, char* source'],
['const GLubyte*', ['glGetString'], 'GLenum name'],
['void', ['glGetTexLevelParameterfv'],
    'GLenum target, GLint level, GLenum pname, GLfloat* params'],
['void', ['glGetTexLevelParameteriv'],
    'GLenum target, GLint level, GLenum pname, GLint* params'],
['void', ['glGetTexParameterfv'],
    'GLenum target, GLenum pname, GLfloat* params'],
['void', ['glGetTexParameteriv'], 'GLenum target, GLenum pname, GLint* params'],
['void', ['glGetUniformfv'], 'GLuint program, GLint location, GLfloat* params'],
['void', ['glGetUniformiv'], 'GLuint program, GLint location, GLint* params'],
['GLint', ['glGetUniformLocation'], 'GLuint program, const char* name'],
['void', ['glGetVertexAttribfv'],
    'GLuint index, GLenum pname, GLfloat* params'],
['void', ['glGetVertexAttribiv'], 'GLuint index, GLenum pname, GLint* params'],
['void', ['glGetVertexAttribPointerv'],
    'GLuint index, GLenum pname, void** pointer'],
['void', ['glHint'], 'GLenum target, GLenum mode'],
['GLboolean', ['glIsBuffer'], 'GLuint buffer'],
['GLboolean', ['glIsEnabled'], 'GLenum cap'],
['GLboolean', ['glIsFramebufferEXT', 'glIsFramebuffer'],
    'GLuint framebuffer'],
['GLboolean', ['glIsProgram'], 'GLuint program'],
['GLboolean', ['glIsRenderbufferEXT', 'glIsRenderbuffer'],
    'GLuint renderbuffer'],
['GLboolean', ['glIsShader'], 'GLuint shader'],
['GLboolean', ['glIsTexture'], 'GLuint texture'],
['void', ['glLineWidth'], 'GLfloat width'],
['void', ['glLinkProgram'], 'GLuint program'],
['void', ['glPixelStorei'], 'GLenum pname, GLint param'],
['void', ['glPolygonOffset'], 'GLfloat factor, GLfloat units'],
['void', ['glReadPixels'],
    'GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, '
    'GLenum type, void* pixels'],
['void', ['glReleaseShaderCompiler'], 'void'],
['void', ['glRenderbufferStorageMultisampleEXT',
    'glRenderbufferStorageMultisample'],
    'GLenum target, GLsizei samples, GLenum internalformat, '
    'GLsizei width, GLsizei height'],
['void', ['glRenderbufferStorageMultisampleANGLE',
    'glRenderbufferStorageMultisample'],
    'GLenum target, GLsizei samples, GLenum internalformat, '
    'GLsizei width, GLsizei height'],
['void', ['glRenderbufferStorageEXT', 'glRenderbufferStorage'],
    'GLenum target, GLenum internalformat, GLsizei width, GLsizei height'],
['void', ['glSampleCoverage'], 'GLclampf value, GLboolean invert'],
['void', ['glScissor'], 'GLint x, GLint y, GLsizei width, GLsizei height'],
['void', ['glShaderBinary'],
    'GLsizei n, const GLuint* shaders, GLenum binaryformat, '
    'const void* binary, GLsizei length'],
['void', ['glShaderSource'],
    'GLuint shader, GLsizei count, const char** str, const GLint* length'],
['void', ['glStencilFunc'], 'GLenum func, GLint ref, GLuint mask'],
['void', ['glStencilFuncSeparate'],
    'GLenum face, GLenum func, GLint ref, GLuint mask'],
['void', ['glStencilMask'], 'GLuint mask'],
['void', ['glStencilMaskSeparate'], 'GLenum face, GLuint mask'],
['void', ['glStencilOp'], 'GLenum fail, GLenum zfail, GLenum zpass'],
['void', ['glStencilOpSeparate'],
    'GLenum face, GLenum fail, GLenum zfail, GLenum zpass'],
['void', ['glTexImage2D'],
    'GLenum target, GLint level, GLint internalformat, GLsizei width, '
    'GLsizei height, GLint border, GLenum format, GLenum type, '
    'const void* pixels'],
['void', ['glTexParameterf'], 'GLenum target, GLenum pname, GLfloat param'],
['void', ['glTexParameterfv'],
    'GLenum target, GLenum pname, const GLfloat* params'],
['void', ['glTexParameteri'], 'GLenum target, GLenum pname, GLint param'],
['void', ['glTexParameteriv'],
    'GLenum target, GLenum pname, const GLint* params'],
['void', ['glTexSubImage2D'],
    'GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, '
    'GLsizei height, GLenum format, GLenum type, const void* pixels'],
['void', ['glUniform1f'], 'GLint location, GLfloat x'],
['void', ['glUniform1fv'], 'GLint location, GLsizei count, const GLfloat* v'],
['void', ['glUniform1i'], 'GLint location, GLint x'],
['void', ['glUniform1iv'], 'GLint location, GLsizei count, const GLint* v'],
['void', ['glUniform2f'], 'GLint location, GLfloat x, GLfloat y'],
['void', ['glUniform2fv'], 'GLint location, GLsizei count, const GLfloat* v'],
['void', ['glUniform2i'], 'GLint location, GLint x, GLint y'],
['void', ['glUniform2iv'], 'GLint location, GLsizei count, const GLint* v'],
['void', ['glUniform3f'], 'GLint location, GLfloat x, GLfloat y, GLfloat z'],
['void', ['glUniform3fv'], 'GLint location, GLsizei count, const GLfloat* v'],
['void', ['glUniform3i'], 'GLint location, GLint x, GLint y, GLint z'],
['void', ['glUniform3iv'], 'GLint location, GLsizei count, const GLint* v'],
['void', ['glUniform4f'],
    'GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w'],
['void', ['glUniform4fv'], 'GLint location, GLsizei count, const GLfloat* v'],
['void', ['glUniform4i'], 'GLint location, GLint x, GLint y, GLint z, GLint w'],
['void', ['glUniform4iv'], 'GLint location, GLsizei count, const GLint* v'],
['void', ['glUniformMatrix2fv'],
    'GLint location, GLsizei count, GLboolean transpose, const GLfloat* value'],
['void', ['glUniformMatrix3fv'],
    'GLint location, GLsizei count, GLboolean transpose, const GLfloat* value'],
['void', ['glUniformMatrix4fv'],
    'GLint location, GLsizei count, GLboolean transpose, const GLfloat* value'],
['void', ['glUseProgram'], 'GLuint program'],
['void', ['glValidateProgram'], 'GLuint program'],
['void', ['glVertexAttrib1f'], 'GLuint indx, GLfloat x'],
['void', ['glVertexAttrib1fv'], 'GLuint indx, const GLfloat* values'],
['void', ['glVertexAttrib2f'], 'GLuint indx, GLfloat x, GLfloat y'],
['void', ['glVertexAttrib2fv'], 'GLuint indx, const GLfloat* values'],
['void', ['glVertexAttrib3f'], 'GLuint indx, GLfloat x, GLfloat y, GLfloat z'],
['void', ['glVertexAttrib3fv'], 'GLuint indx, const GLfloat* values'],
['void', ['glVertexAttrib4f'],
    'GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w'],
['void', ['glVertexAttrib4fv'], 'GLuint indx, const GLfloat* values'],
['void', ['glVertexAttribPointer'],
    'GLuint indx, GLint size, GLenum type, GLboolean normalized, '
    'GLsizei stride, const void* ptr'],
['void', ['glViewport'], 'GLint x, GLint y, GLsizei width, GLsizei height'],
['void', ['glGenFencesNV'], 'GLsizei n, GLuint* fences'],
['void', ['glDeleteFencesNV'], 'GLsizei n, const GLuint* fences'],
['void', ['glSetFenceNV'], 'GLuint fence, GLenum condition'],
['GLboolean', ['glTestFenceNV'], 'GLuint fence'],
['void', ['glFinishFenceNV'], 'GLuint fence'],
['GLboolean', ['glIsFenceNV'], 'GLuint fence'],
['void', ['glGetFenceivNV'], 'GLuint fence, GLenum pname, GLint* params'],
]

OSMESA_FUNCTIONS = [
['OSMesaContext', ['OSMesaCreateContext'],
    'GLenum format, OSMesaContext sharelist'],
['OSMesaContext', ['OSMesaCreateContextExt'],
    'GLenum format, GLint depthBits, GLint stencilBits, GLint accumBits, '
    'OSMesaContext sharelist'],
['void', ['OSMesaDestroyContext'], 'OSMesaContext ctx'],
['GLboolean', ['OSMesaMakeCurrent'],
    'OSMesaContext ctx, void* buffer, GLenum type, GLsizei width, '
    'GLsizei height'],
['OSMesaContext', ['OSMesaGetCurrentContext'], 'void'],
['void', ['OSMesaPixelStore'], 'GLint pname, GLint value'],
['void', ['OSMesaGetIntegerv'], 'GLint pname, GLint* value'],
['GLboolean', ['OSMesaGetDepthBuffer'],
    'OSMesaContext c, GLint* width, GLint* height, GLint* bytesPerValue, '
    'void** buffer'],
['GLboolean', ['OSMesaGetColorBuffer'],
    'OSMesaContext c, GLint* width, GLint* height, GLint* format, '
    'void** buffer'],
['OSMESAproc', ['OSMesaGetProcAddress'], 'const char* funcName'],
['void', ['OSMesaColorClamp'], 'GLboolean enable'],
]

EGL_FUNCTIONS = [
['EGLint', ['eglGetError'], 'void'],
['EGLDisplay', ['eglGetDisplay'], 'EGLNativeDisplayType display_id'],
['EGLBoolean', ['eglInitialize'],
    'EGLDisplay dpy, EGLint* major, EGLint* minor'],
['EGLBoolean', ['eglTerminate'], 'EGLDisplay dpy'],
['const char*', ['eglQueryString'], 'EGLDisplay dpy, EGLint name'],
['EGLBoolean', ['eglGetConfigs'],
    'EGLDisplay dpy, EGLConfig* configs, EGLint config_size, '
    'EGLint* num_config'],
['EGLBoolean', ['eglChooseConfig'],
    'EGLDisplay dpy, const EGLint* attrib_list, EGLConfig* configs, '
    'EGLint config_size, EGLint* num_config'],
['EGLBoolean', ['eglGetConfigAttrib'],
    'EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint* value'],
['EGLSurface', ['eglCreateWindowSurface'],
    'EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, '
    'const EGLint* attrib_list'],
['EGLSurface', ['eglCreatePbufferSurface'],
    'EGLDisplay dpy, EGLConfig config, const EGLint* attrib_list'],
['EGLSurface', ['eglCreatePixmapSurface'],
    'EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, '
    'const EGLint* attrib_list'],
['EGLBoolean', ['eglDestroySurface'], 'EGLDisplay dpy, EGLSurface surface'],
['EGLBoolean', ['eglQuerySurface'],
    'EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint* value'],
['EGLBoolean', ['eglBindAPI'], 'EGLenum api'],
['EGLenum', ['eglQueryAPI'], 'void'],
['EGLBoolean', ['eglWaitClient'], 'void'],
['EGLBoolean', ['eglReleaseThread'], 'void'],
['EGLSurface', ['eglCreatePbufferFromClientBuffer'],
    'EGLDisplay dpy, EGLenum buftype, void* buffer, EGLConfig config, '
    'const EGLint* attrib_list'],
['EGLBoolean', ['eglSurfaceAttrib'],
    'EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value'],
['EGLBoolean', ['eglBindTexImage'],
    'EGLDisplay dpy, EGLSurface surface, EGLint buffer'],
['EGLBoolean', ['eglReleaseTexImage'],
    'EGLDisplay dpy, EGLSurface surface, EGLint buffer'],
['EGLBoolean', ['eglSwapInterval'], 'EGLDisplay dpy, EGLint interval'],
['EGLContext', ['eglCreateContext'],
    'EGLDisplay dpy, EGLConfig config, EGLContext share_context, '
    'const EGLint* attrib_list'],
['EGLBoolean', ['eglDestroyContext'], 'EGLDisplay dpy, EGLContext ctx'],
['EGLBoolean', ['eglMakeCurrent'],
    'EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx'],
['EGLContext', ['eglGetCurrentContext'], 'void'],
['EGLSurface', ['eglGetCurrentSurface'], 'EGLint readdraw'],
['EGLDisplay', ['eglGetCurrentDisplay'], 'void'],
['EGLBoolean', ['eglQueryContext'],
    'EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint* value'],
['EGLBoolean', ['eglWaitGL'], 'void'],
['EGLBoolean', ['eglWaitNative'], 'EGLint engine'],
['EGLBoolean', ['eglSwapBuffers'], 'EGLDisplay dpy, EGLSurface surface'],
['EGLBoolean', ['eglCopyBuffers'],
    'EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target'],
['__eglMustCastToProperFunctionPointerType', ['eglGetProcAddress'],
    'const char* procname'],
]

WGL_FUNCTIONS = [
['HGLRC', ['wglCreateContext'], 'HDC hdc'],
['HGLRC', ['wglCreateLayerContext'], 'HDC hdc, int iLayerPlane'],
['BOOL', ['wglCopyContext'], 'HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask'],
['BOOL', ['wglDeleteContext'], 'HGLRC hglrc'],
['HGLRC', ['wglGetCurrentContext'], ''],
['HDC', ['wglGetCurrentDC'], ''],
['BOOL', ['wglMakeCurrent'], 'HDC hdc, HGLRC hglrc'],
['BOOL', ['wglShareLists'], 'HGLRC hglrc1, HGLRC hglrc2'],
['BOOL', ['wglSwapIntervalEXT'], 'int interval'],
['BOOL', ['wglSwapLayerBuffers'], 'HDC hdc, UINT fuPlanes'],
['const char*', ['wglGetExtensionsStringARB', 'wglGetExtensionsStringEXT'],
    'HDC hDC'],
['BOOL', ['wglChoosePixelFormatARB'],
    'HDC dc, const int* int_attrib_list, const float* float_attrib_list, '
    'UINT max_formats, int* formats, UINT* num_formats'],
['HPBUFFERARB', ['wglCreatePbufferARB'],
    'HDC hDC, int iPixelFormat, int iWidth, int iHeight, '
    'const int* piAttribList'],
['HDC', ['wglGetPbufferDCARB'], 'HPBUFFERARB hPbuffer'],
['int', ['wglReleasePbufferDCARB'], 'HPBUFFERARB hPbuffer, HDC hDC'],
['BOOL', ['wglDestroyPbufferARB'], 'HPBUFFERARB hPbuffer'],
['BOOL', ['wglQueryPbufferARB'],
    'HPBUFFERARB hPbuffer, int iAttribute, int* piValue'],
]

GLX_FUNCTIONS = [
['XVisualInfo*', ['glXChooseVisual'],
    'Display* dpy, int screen, int* attribList'],
['GLXContext', ['glXCreateContext'],
    'Display* dpy, XVisualInfo* vis, GLXContext shareList, int direct'],
['void', ['glXDestroyContext'], 'Display* dpy, GLXContext ctx'],
['int', ['glXMakeCurrent'],
    'Display* dpy, GLXDrawable drawable, GLXContext ctx'],
['void', ['glXCopyContext'],
    'Display* dpy, GLXContext src, GLXContext dst, unsigned long mask'],
['void', ['glXSwapBuffers'], 'Display* dpy, GLXDrawable drawable'],
['GLXPixmap', ['glXCreateGLXPixmap'],
    'Display* dpy, XVisualInfo* visual, Pixmap pixmap'],
['void', ['glXDestroyGLXPixmap'], 'Display* dpy, GLXPixmap pixmap'],
['int', ['glXQueryExtension'], 'Display* dpy, int* errorb, int* event'],
['int', ['glXQueryVersion'], 'Display* dpy, int* maj, int* min'],
['int', ['glXIsDirect'], 'Display* dpy, GLXContext ctx'],
['int', ['glXGetConfig'],
    'Display* dpy, XVisualInfo* visual, int attrib, int* value'],
['GLXContext', ['glXGetCurrentContext'], 'void'],
['GLXDrawable', ['glXGetCurrentDrawable'], 'void'],
['void', ['glXWaitGL'], 'void'],
['void', ['glXWaitX'], 'void'],
['void', ['glXUseXFont'], 'Font font, int first, int count, int list'],
['const char*', ['glXQueryExtensionsString'], 'Display* dpy, int screen'],
['const char*', ['glXQueryServerString'], 'Display* dpy, int screen, int name'],
['const char*', ['glXGetClientString'], 'Display* dpy, int name'],
['Display*', ['glXGetCurrentDisplay'], 'void'],
['GLXFBConfig*', ['glXChooseFBConfig'],
    'Display* dpy, int screen, const int* attribList, int* nitems'],
['int', ['glXGetFBConfigAttrib'],
    'Display* dpy, GLXFBConfig config, int attribute, int* value'],
['GLXFBConfig*', ['glXGetFBConfigs'],
    'Display* dpy, int screen, int* nelements'],
['XVisualInfo*', ['glXGetVisualFromFBConfig'],
    'Display* dpy, GLXFBConfig config'],
['GLXWindow', ['glXCreateWindow'],
    'Display* dpy, GLXFBConfig config, Window win, const int* attribList'],
['void', ['glXDestroyWindow'], 'Display* dpy, GLXWindow window'],
['GLXPixmap', ['glXCreatePixmap'],
    'Display* dpy, GLXFBConfig config, Pixmap pixmap, const int* attribList'],
['void', ['glXDestroyPixmap'], 'Display* dpy, GLXPixmap pixmap'],
['GLXPbuffer', ['glXCreatePbuffer'],
    'Display* dpy, GLXFBConfig config, const int* attribList'],
['void', ['glXDestroyPbuffer'], 'Display* dpy, GLXPbuffer pbuf'],
['void', ['glXQueryDrawable'],
    'Display* dpy, GLXDrawable draw, int attribute, unsigned int* value'],
['GLXContext', ['glXCreateNewContext'],
    'Display* dpy, GLXFBConfig config, int renderType, '
    'GLXContext shareList, int direct'],
['int', ['glXMakeContextCurrent'],
    'Display* dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx'],
['GLXDrawable', ['glXGetCurrentReadDrawable'], 'void'],
['int', ['glXQueryContext'],
    'Display* dpy, GLXContext ctx, int attribute, int* value'],
['void', ['glXSelectEvent'],
    'Display* dpy, GLXDrawable drawable, unsigned long mask'],
['void', ['glXGetSelectedEvent'],
    'Display* dpy, GLXDrawable drawable, unsigned long* mask'],
['void', ['glXSwapIntervalEXT'],
    'Display* dpy, GLXDrawable drawable, int interval'],
]

FUNCTION_SETS = [
  [GL_FUNCTIONS, 'gl'],
  [OSMESA_FUNCTIONS, 'osmesa'],
  [EGL_FUNCTIONS, 'egl'],
  [WGL_FUNCTIONS, 'wgl'],
  [GLX_FUNCTIONS, 'glx'],
]

def GenerateHeader(file, functions, set_name):
  """Generates gl_binding_autogen_x.h"""

  # Write file header.
  file.write('// Copyright (c) 2010 The Chromium Authors. All rights reserved.\n')
  file.write('// Use of this source code is governed by a BSD-style license that can be\n')
  file.write('// found in the LICENSE file.\n')
  file.write('\n')
  file.write('// This file is automatically generated.\n')
  file.write('\n')
  file.write('#ifndef APP_GFX_GL_GL_BINDINGS_AUTOGEN_%s_H_\n' % set_name.upper())
  file.write('#define APP_GFX_GL_GL_BINDINGS_AUTOGEN_%s_H_\n' % set_name.upper())

  # Write prototype for initialization function.
  file.write('\n')
  file.write('namespace gfx {\n')
  file.write('\n')
  file.write('void InitializeGLBindings%s();\n' % set_name.upper())
  file.write('void InitializeDebugGLBindings%s();\n' % set_name.upper())

  # Write typedefs for function pointer types. Always use the GL name for the
  # typedef.
  file.write('\n')
  for [return_type, names, arguments] in functions:
    file.write('typedef %s (GL_BINDING_CALL *%sProc)(%s);\n' %
        (return_type, names[0], arguments))

  # Write declarations for function pointers. Always use the GL name for the
  # declaration.
  file.write('\n')
  for [return_type, names, arguments] in functions:
    file.write('extern %sProc g_%s;\n' % (names[0], names[0]))
  file.write('\n')
  file.write( '}  // namespace gfx\n')

  # Write macros to invoke function pointers. Always use the GL name for the
  # macro.
  file.write('\n')
  for [return_type, names, arguments] in functions:
    file.write('#define %s ::gfx::g_%s\n' %
        (names[0], names[0]))

  file.write('\n')
  file.write('#endif  //  APP_GFX_GL_GL_BINDINGS_AUTOGEN_%s_H_\n' %
      set_name.upper())


def GenerateSource(file, functions, set_name):
  """Generates gl_binding_autogen_x.cc"""

  # Write file header.
  file.write('// Copyright (c) 2010 The Chromium Authors. All rights reserved.\n')
  file.write('// Use of this source code is governed by a BSD-style license that can be\n')
  file.write('// found in the LICENSE file.\n')
  file.write('\n')
  file.write('// This file is automatically generated.\n')
  file.write('\n')
  file.write('#include "app/gfx/gl/gl_bindings.h"\n')
  file.write('#include "app/gfx/gl/gl_implementation.h"\n')

  # Write definitions of function pointers.
  file.write('\n')
  file.write('namespace gfx {\n')
  file.write('\n')
  for [return_type, names, arguments] in functions:
    file.write('%sProc g_%s;\n' % (names[0], names[0]))

  file.write('\n')
  for [return_type, names, arguments] in functions:
    file.write('static %sProc g_debug_%s;\n' % (names[0], names[0]))

  # Write function to initialize the function pointers.
  file.write('\n')
  file.write('void InitializeGLBindings%s() {\n' % set_name.upper())
  for [return_type, names, arguments] in functions:
    for name in names:
      file.write('  if (!g_%s)\n' % names[0])
      file.write(
          '    g_%s = reinterpret_cast<%sProc>(GetGLProcAddress("%s"));\n' %
              (names[0], names[0], name))
  file.write('}\n')
  file.write('\n')

  # Write logging wrappers for each function.
  file.write('extern "C" {\n')
  for [return_type, names, arguments] in functions:
    file.write('\n')
    file.write('static %s GL_BINDING_CALL Debug_%s(%s) {\n' %
        (return_type, names[0], arguments))
    argument_names = re.sub(r'(const )?[a-zA-Z0-9]+\** ([a-zA-Z0-9]+)', r'\2',
                              arguments)
    argument_names = re.sub(r'(const )?[a-zA-Z0-9]+\** ([a-zA-Z0-9]+)', r'\2',
                              argument_names)
    log_argument_names = argument_names.replace(',', ' << ", " <<');
    if argument_names == 'void' or argument_names == '':
      argument_names = ''
      log_argument_names = ''
    else:
      log_argument_names = " << " + log_argument_names
    function_name = names[0]
    if return_type == 'void':
      file.write('  GL_SERVICE_LOG("%s" << "(" %s << ")");\n' %
          (function_name, log_argument_names))
      file.write('  g_debug_%s(%s);\n' %
          (function_name, argument_names))
    else:
      file.write('  GL_SERVICE_LOG("%s" << "(" %s << ")");\n' %
          (function_name, log_argument_names))
      file.write('  %s result = g_debug_%s(%s);\n' %
          (return_type, function_name, argument_names))
      file.write('  GL_SERVICE_LOG("GL_RESULT: " << result);\n');
      file.write('  return result;\n')
    file.write('}\n')
  file.write('}  // extern "C"\n')

  # Write function to initialize the function pointers.
  file.write('\n')
  file.write('void InitializeDebugGLBindings%s() {\n' % set_name.upper())
  for [return_type, names, arguments] in functions:
    for name in names:
      file.write('  if (!g_debug_%s) {\n' % names[0])
      file.write('    g_debug_%s = g_%s;\n' % (names[0], names[0]))
      file.write('    g_%s = Debug_%s;\n' % (names[0], names[0]))
      file.write('  }\n')
  file.write('}\n')
  file.write('\n')

  file.write( '}  // namespace gfx\n')


def GenerateMockSource(file, functions):
  """Generates functions that invoke a mock GLInterface"""

  file.write('// Copyright (c) 2010 The Chromium Authors. All rights reserved.\n')
  file.write('// Use of this source code is governed by a BSD-style license that can be\n')
  file.write('// found in the LICENSE file.\n')
  file.write('\n')
  file.write('// This file is automatically generated.\n')
  file.write('\n')
  file.write('#include <string.h>\n')
  file.write('\n')
  file.write('#include "app/gfx/gl/gl_interface.h"\n')

  file.write('\n')
  file.write('namespace gfx {\n')

  # Write function that trampoline into the GLInterface.
  for [return_type, names, arguments] in functions:
    file.write('\n')
    file.write('%s GL_BINDING_CALL Mock_%s(%s) {\n' %
        (return_type, names[0], arguments))
    argument_names = re.sub(r'(const )?[a-zA-Z0-9]+\** ([a-zA-Z0-9]+)', r'\2',
                              arguments)
    if argument_names == 'void':
      argument_names = ''
    function_name = names[0][2:]
    if return_type == 'void':
      file.write('  GLInterface::GetGLInterface()->%s(%s);\n' %
          (function_name, argument_names))
    else:
      file.write('  return GLInterface::GetGLInterface()->%s(%s);\n' %
          (function_name, argument_names))
    file.write('}\n')

  # Write a function to lookup a mock GL function based on its name.
  file.write('\n')
  file.write('void* GL_BINDING_CALL GetMockGLProcAddress(const char* name) {\n')
  for [return_type, names, arguments] in functions:
    file.write('  if (strcmp(name, "%s") == 0)\n' % names[0])
    file.write('    return reinterpret_cast<void*>(Mock_%s);\n' % names[0])
  file.write('  return NULL;\n')
  file.write('}\n');

  file.write('\n')
  file.write('}  // namespace gfx\n')


def main(argv):
  """This is the main function."""

  if len(argv) >= 1:
    dir = argv[0]
  else:
    dir = '.'

  for [functions, set_name] in FUNCTION_SETS:
    header_file = open(
        os.path.join(dir, 'gl_bindings_autogen_%s.h' % set_name), 'wb')
    GenerateHeader(header_file, functions, set_name)
    header_file.close()

    source_file = open(
        os.path.join(dir, 'gl_bindings_autogen_%s.cc' % set_name), 'wb')
    GenerateSource(source_file, functions, set_name)
    source_file.close()

  source_file = open(os.path.join(dir, 'gl_bindings_autogen_mock.cc'), 'wb')
  GenerateMockSource(source_file, GL_FUNCTIONS)
  source_file.close()


if __name__ == '__main__':
  main(sys.argv[1:])
