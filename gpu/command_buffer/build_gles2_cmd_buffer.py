#!/usr/bin/python
#
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""code generator for GL command buffers."""

import os
import os.path
import sys
import re
from optparse import OptionParser

_SIZE_OF_UINT32 = 4
_SIZE_OF_COMMAND_HEADER = 4
_FIRST_SPECIFIC_COMMAND_ID = 256

_LICENSE = """// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"""

# This string is copied directly out of the gl2.h file from GLES2.0
#
# Edits:
#
# *) Any argument that is a resourceID has been changed to GLid<Type>.
#    (not pointer arguments)
#
# *) All GLenums have been changed to GLenumTypeOfEnum
#
_GL_FUNCTIONS = """
GL_APICALL void         GL_APIENTRY glActiveTexture (GLenum texture);
GL_APICALL void         GL_APIENTRY glAttachShader (GLidProgram program, GLidShader shader);
GL_APICALL void         GL_APIENTRY glBindAttribLocation (GLidProgram program, GLuint index, const char* name);
GL_APICALL void         GL_APIENTRY glBindBuffer (GLenumBufferTarget target, GLidBuffer buffer);
GL_APICALL void         GL_APIENTRY glBindFramebuffer (GLenumFrameBufferTarget target, GLidFramebuffer framebuffer);
GL_APICALL void         GL_APIENTRY glBindRenderbuffer (GLenumRenderBufferTarget target, GLidRenderbuffer renderbuffer);
GL_APICALL void         GL_APIENTRY glBindTexture (GLenumTextureBindTarget target, GLidTexture texture);
GL_APICALL void         GL_APIENTRY glBlendColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
GL_APICALL void         GL_APIENTRY glBlendEquation ( GLenumEquation mode );
GL_APICALL void         GL_APIENTRY glBlendEquationSeparate (GLenumEquation modeRGB, GLenumEquation modeAlpha);
GL_APICALL void         GL_APIENTRY glBlendFunc (GLenumSrcBlendFactor sfactor, GLenumDstBlendFactor dfactor);
GL_APICALL void         GL_APIENTRY glBlendFuncSeparate (GLenumSrcBlendFactor srcRGB, GLenumDstBlendFactor dstRGB, GLenumSrcBlendFactor srcAlpha, GLenumDstBlendFactor dstAlpha);
GL_APICALL void         GL_APIENTRY glBufferData (GLenumBufferTarget target, GLsizeiptr size, const void* data, GLenumBufferUsage usage);
GL_APICALL void         GL_APIENTRY glBufferSubData (GLenumBufferTarget target, GLintptr offset, GLsizeiptr size, const void* data);
GL_APICALL GLenum       GL_APIENTRY glCheckFramebufferStatus (GLenumFrameBufferTarget target);
GL_APICALL void         GL_APIENTRY glClear (GLbitfield mask);
GL_APICALL void         GL_APIENTRY glClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
GL_APICALL void         GL_APIENTRY glClearDepthf (GLclampf depth);
GL_APICALL void         GL_APIENTRY glClearStencil (GLint s);
GL_APICALL void         GL_APIENTRY glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
GL_APICALL void         GL_APIENTRY glCompileShader (GLidShader shader);
GL_APICALL void         GL_APIENTRY glCompressedTexImage2D (GLenumTextureTarget target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data);
GL_APICALL void         GL_APIENTRY glCompressedTexSubImage2D (GLenumTextureTarget target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data);
GL_APICALL void         GL_APIENTRY glCopyTexImage2D (GLenumTextureTarget target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
GL_APICALL void         GL_APIENTRY glCopyTexSubImage2D (GLenumTextureTarget target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GL_APICALL GLuint       GL_APIENTRY glCreateProgram (void);
GL_APICALL GLuint       GL_APIENTRY glCreateShader (GLenumShaderType type);
GL_APICALL void         GL_APIENTRY glCullFace (GLenumFaceType mode);
GL_APICALL void         GL_APIENTRY glDeleteBuffers (GLsizei n, const GLuint* buffers);
GL_APICALL void         GL_APIENTRY glDeleteFramebuffers (GLsizei n, const GLuint* framebuffers);
GL_APICALL void         GL_APIENTRY glDeleteProgram (GLidProgram program);
GL_APICALL void         GL_APIENTRY glDeleteRenderbuffers (GLsizei n, const GLuint* renderbuffers);
GL_APICALL void         GL_APIENTRY glDeleteShader (GLidShader shader);
GL_APICALL void         GL_APIENTRY glDeleteTextures (GLsizei n, const GLuint* textures);
GL_APICALL void         GL_APIENTRY glDepthFunc (GLenumCmpFunction func);
GL_APICALL void         GL_APIENTRY glDepthMask (GLboolean flag);
GL_APICALL void         GL_APIENTRY glDepthRangef (GLclampf zNear, GLclampf zFar);
GL_APICALL void         GL_APIENTRY glDetachShader (GLidProgram program, GLidShader shader);
GL_APICALL void         GL_APIENTRY glDisable (GLenumCapability cap);
GL_APICALL void         GL_APIENTRY glDisableVertexAttribArray (GLuint index);
GL_APICALL void         GL_APIENTRY glDrawArrays (GLenumDrawMode mode, GLint first, GLsizei count);
GL_APICALL void         GL_APIENTRY glDrawElements (GLenumDrawMode mode, GLsizei count, GLenumIndexType type, const void* indices);
GL_APICALL void         GL_APIENTRY glEnable (GLenumCapability cap);
GL_APICALL void         GL_APIENTRY glEnableVertexAttribArray (GLuint index);
GL_APICALL void         GL_APIENTRY glFinish (void);
GL_APICALL void         GL_APIENTRY glFlush (void);
GL_APICALL void         GL_APIENTRY glFramebufferRenderbuffer (GLenumFrameBufferTarget target, GLenumAttachment attachment, GLenumRenderBufferTarget renderbuffertarget, GLidRenderbuffer renderbuffer);
GL_APICALL void         GL_APIENTRY glFramebufferTexture2D (GLenumFrameBufferTarget target, GLenumAttachment attachment, GLenumTextureTarget textarget, GLidTexture texture, GLint level);
GL_APICALL void         GL_APIENTRY glFrontFace (GLenumFaceMode mode);
GL_APICALL void         GL_APIENTRY glGenBuffers (GLsizei n, GLuint* buffers);
GL_APICALL void         GL_APIENTRY glGenerateMipmap (GLenumTextureBindTarget target);
GL_APICALL void         GL_APIENTRY glGenFramebuffers (GLsizei n, GLuint* framebuffers);
GL_APICALL void         GL_APIENTRY glGenRenderbuffers (GLsizei n, GLuint* renderbuffers);
GL_APICALL void         GL_APIENTRY glGenTextures (GLsizei n, GLuint* textures);
GL_APICALL void         GL_APIENTRY glGetActiveAttrib (GLidProgram program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name);
GL_APICALL void         GL_APIENTRY glGetActiveUniform (GLidProgram program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name);
GL_APICALL void         GL_APIENTRY glGetAttachedShaders (GLidProgram program, GLsizei maxcount, GLsizei* count, GLuint* shaders);
GL_APICALL GLint        GL_APIENTRY glGetAttribLocation (GLidProgram program, const char* name);
GL_APICALL void         GL_APIENTRY glGetBooleanv (GLenum pname, GLboolean* params);
GL_APICALL void         GL_APIENTRY glGetBufferParameteriv (GLenumBufferTarget target, GLenumBufferParameter pname, GLint* params);
GL_APICALL GLenum       GL_APIENTRY glGetError (void);
GL_APICALL void         GL_APIENTRY glGetFloatv (GLenum pname, GLfloat* params);
GL_APICALL void         GL_APIENTRY glGetFramebufferAttachmentParameteriv (GLenumFrameBufferTarget target, GLenumAttachment attachment, GLenumFrameBufferParameter pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetIntegerv (GLenum pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetProgramiv (GLidProgram program, GLenumProgramParameter pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetProgramInfoLog (GLidProgram program, GLsizei bufsize, GLsizei* length, char* infolog);
GL_APICALL void         GL_APIENTRY glGetRenderbufferParameteriv (GLenumRenderBufferTarget target, GLenumRenderBufferParameter pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetShaderiv (GLidShader shader, GLenumShaderParameter pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetShaderInfoLog (GLidShader shader, GLsizei bufsize, GLsizei* length, char* infolog);
GL_APICALL void         GL_APIENTRY glGetShaderPrecisionFormat (GLenumShaderType shadertype, GLenumShaderPercision precisiontype, GLint* range, GLint* precision);
GL_APICALL void         GL_APIENTRY glGetShaderSource (GLidShader shader, GLsizei bufsize, GLsizei* length, char* source);
GL_APICALL const GLubyte* GL_APIENTRY glGetString (GLenumStringType name);
GL_APICALL void         GL_APIENTRY glGetTexParameterfv (GLenumTextureTarget target, GLenumTextureParameter pname, GLfloat* params);
GL_APICALL void         GL_APIENTRY glGetTexParameteriv (GLenumTextureTarget target, GLenumTextureParameter pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetUniformfv (GLidProgram program, GLint location, GLfloat* params);
GL_APICALL void         GL_APIENTRY glGetUniformiv (GLidProgram program, GLint location, GLint* params);
GL_APICALL GLint        GL_APIENTRY glGetUniformLocation (GLidProgram program, const char* name);
GL_APICALL void         GL_APIENTRY glGetVertexAttribfv (GLuint index, GLenumVertexAttribute pname, GLfloat* params);
GL_APICALL void         GL_APIENTRY glGetVertexAttribiv (GLuint index, GLenumVertexAttribute pname, GLint* params);
GL_APICALL void         GL_APIENTRY glGetVertexAttribPointerv (GLuint index, GLenumVertexPointer pname, void** pointer);
GL_APICALL void         GL_APIENTRY glHint (GLenumHintTarget target, GLenumHintMode mode);
GL_APICALL GLboolean    GL_APIENTRY glIsBuffer (GLidBuffer buffer);
GL_APICALL GLboolean    GL_APIENTRY glIsEnabled (GLenumCapability cap);
GL_APICALL GLboolean    GL_APIENTRY glIsFramebuffer (GLidFramebuffer framebuffer);
GL_APICALL GLboolean    GL_APIENTRY glIsProgram (GLidProgram program);
GL_APICALL GLboolean    GL_APIENTRY glIsRenderbuffer (GLidRenderbuffer renderbuffer);
GL_APICALL GLboolean    GL_APIENTRY glIsShader (GLidShader shader);
GL_APICALL GLboolean    GL_APIENTRY glIsTexture (GLidTexture texture);
GL_APICALL void         GL_APIENTRY glLineWidth (GLfloat width);
GL_APICALL void         GL_APIENTRY glLinkProgram (GLidProgram program);
GL_APICALL void         GL_APIENTRY glPixelStorei (GLenumPixelStore pname, GLintPixelStoreAlignment param);
GL_APICALL void         GL_APIENTRY glPolygonOffset (GLfloat factor, GLfloat units);
GL_APICALL void         GL_APIENTRY glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenumReadPixelFormat format, GLenumPixelType type, void* pixels);
GL_APICALL void         GL_APIENTRY glReleaseShaderCompiler (void);
GL_APICALL void         GL_APIENTRY glRenderbufferStorage (GLenumRenderBufferTarget target, GLenumRenderBufferFormat internalformat, GLsizei width, GLsizei height);
GL_APICALL void         GL_APIENTRY glSampleCoverage (GLclampf value, GLboolean invert);
GL_APICALL void         GL_APIENTRY glScissor (GLint x, GLint y, GLsizei width, GLsizei height);
GL_APICALL void         GL_APIENTRY glShaderBinary (GLsizei n, const GLidShader* shaders, GLenum binaryformat, const void* binary, GLsizei length);
GL_APICALL void         GL_APIENTRY glShaderSource (GLidShader shader, GLsizei count, const char** str, const GLint* length);
GL_APICALL void         GL_APIENTRY glStencilFunc (GLenumCmpFunction func, GLint ref, GLuint mask);
GL_APICALL void         GL_APIENTRY glStencilFuncSeparate (GLenumFaceType face, GLenumCmpFunction func, GLint ref, GLuint mask);
GL_APICALL void         GL_APIENTRY glStencilMask (GLuint mask);
GL_APICALL void         GL_APIENTRY glStencilMaskSeparate (GLenumFaceType face, GLuint mask);
GL_APICALL void         GL_APIENTRY glStencilOp (GLenumStencilOp fail, GLenumStencilOp zfail, GLenumStencilOp zpass);
GL_APICALL void         GL_APIENTRY glStencilOpSeparate (GLenumFaceType face, GLenumStencilOp fail, GLenumStencilOp zfail, GLenumStencilOp zpass);
GL_APICALL void         GL_APIENTRY glTexImage2D (GLenumTextureTarget target, GLint level, GLintTextureFormat internalformat, GLsizei width, GLsizei height, GLintTextureBorder border, GLenumTextureFormat format, GLenumPixelType type, const void* pixels);
GL_APICALL void         GL_APIENTRY glTexParameterf (GLenumTextureBindTarget target, GLenumTextureParameter pname, GLfloat param);
GL_APICALL void         GL_APIENTRY glTexParameterfv (GLenumTextureBindTarget target, GLenumTextureParameter pname, const GLfloat* params);
GL_APICALL void         GL_APIENTRY glTexParameteri (GLenumTextureBindTarget target, GLenumTextureParameter pname, GLint param);
GL_APICALL void         GL_APIENTRY glTexParameteriv (GLenumTextureBindTarget target, GLenumTextureParameter pname, const GLint* params);
GL_APICALL void         GL_APIENTRY glTexSubImage2D (GLenumTextureTarget target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenumTextureFormat format, GLenumPixelType type, const void* pixels);
GL_APICALL void         GL_APIENTRY glUniform1f (GLint location, GLfloat x);
GL_APICALL void         GL_APIENTRY glUniform1fv (GLint location, GLsizei count, const GLfloat* v);
GL_APICALL void         GL_APIENTRY glUniform1i (GLint location, GLint x);
GL_APICALL void         GL_APIENTRY glUniform1iv (GLint location, GLsizei count, const GLint* v);
GL_APICALL void         GL_APIENTRY glUniform2f (GLint location, GLfloat x, GLfloat y);
GL_APICALL void         GL_APIENTRY glUniform2fv (GLint location, GLsizei count, const GLfloat* v);
GL_APICALL void         GL_APIENTRY glUniform2i (GLint location, GLint x, GLint y);
GL_APICALL void         GL_APIENTRY glUniform2iv (GLint location, GLsizei count, const GLint* v);
GL_APICALL void         GL_APIENTRY glUniform3f (GLint location, GLfloat x, GLfloat y, GLfloat z);
GL_APICALL void         GL_APIENTRY glUniform3fv (GLint location, GLsizei count, const GLfloat* v);
GL_APICALL void         GL_APIENTRY glUniform3i (GLint location, GLint x, GLint y, GLint z);
GL_APICALL void         GL_APIENTRY glUniform3iv (GLint location, GLsizei count, const GLint* v);
GL_APICALL void         GL_APIENTRY glUniform4f (GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
GL_APICALL void         GL_APIENTRY glUniform4fv (GLint location, GLsizei count, const GLfloat* v);
GL_APICALL void         GL_APIENTRY glUniform4i (GLint location, GLint x, GLint y, GLint z, GLint w);
GL_APICALL void         GL_APIENTRY glUniform4iv (GLint location, GLsizei count, const GLint* v);
GL_APICALL void         GL_APIENTRY glUniformMatrix2fv (GLint location, GLsizei count, GLbooleanFalse transpose, const GLfloat* value);
GL_APICALL void         GL_APIENTRY glUniformMatrix3fv (GLint location, GLsizei count, GLbooleanFalse transpose, const GLfloat* value);
GL_APICALL void         GL_APIENTRY glUniformMatrix4fv (GLint location, GLsizei count, GLbooleanFalse transpose, const GLfloat* value);
GL_APICALL void         GL_APIENTRY glUseProgram (GLidProgram program);
GL_APICALL void         GL_APIENTRY glValidateProgram (GLidProgram program);
GL_APICALL void         GL_APIENTRY glVertexAttrib1f (GLuint indx, GLfloat x);
GL_APICALL void         GL_APIENTRY glVertexAttrib1fv (GLuint indx, const GLfloat* values);
GL_APICALL void         GL_APIENTRY glVertexAttrib2f (GLuint indx, GLfloat x, GLfloat y);
GL_APICALL void         GL_APIENTRY glVertexAttrib2fv (GLuint indx, const GLfloat* values);
GL_APICALL void         GL_APIENTRY glVertexAttrib3f (GLuint indx, GLfloat x, GLfloat y, GLfloat z);
GL_APICALL void         GL_APIENTRY glVertexAttrib3fv (GLuint indx, const GLfloat* values);
GL_APICALL void         GL_APIENTRY glVertexAttrib4f (GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
GL_APICALL void         GL_APIENTRY glVertexAttrib4fv (GLuint indx, const GLfloat* values);
GL_APICALL void         GL_APIENTRY glVertexAttribPointer (GLuint indx, GLintVertexAttribSize size, GLenumVertexAttribType type, GLboolean normalized, GLsizei stride, const void* ptr);
GL_APICALL void         GL_APIENTRY glViewport (GLint x, GLint y, GLsizei width, GLsizei height);
// Non-GL commands.
GL_APICALL void         GL_APIENTRY glSwapBuffers (void);
"""

# This is the list of all commmands that will be generated and their Id.
# If a command is not listed in this table it is an error.
# This lets us make sure that command ids do not change as the generator
# generates new variations of commands.

_CMD_ID_TABLE = {
  'ActiveTexture':                                             256,
  'AttachShader':                                              257,
  'BindAttribLocation':                                        258,
  'BindAttribLocationImmediate':                               259,
  'BindBuffer':                                                260,
  'BindFramebuffer':                                           261,
  'BindRenderbuffer':                                          262,
  'BindTexture':                                               263,
  'BlendColor':                                                264,
  'BlendEquation':                                             265,
  'BlendEquationSeparate':                                     266,
  'BlendFunc':                                                 267,
  'BlendFuncSeparate':                                         268,
  'BufferData':                                                269,
  'BufferDataImmediate':                                       270,
  'BufferSubData':                                             271,
  'BufferSubDataImmediate':                                    272,
  'CheckFramebufferStatus':                                    273,
  'Clear':                                                     274,
  'ClearColor':                                                275,
  'ClearDepthf':                                               276,
  'ClearStencil':                                              277,
  'ColorMask':                                                 278,
  'CompileShader':                                             279,
  'CompressedTexImage2D':                                      280,
  'CompressedTexImage2DImmediate':                             281,
  'CompressedTexSubImage2D':                                   282,
  'CompressedTexSubImage2DImmediate':                          283,
  'CopyTexImage2D':                                            284,
  'CopyTexSubImage2D':                                         285,
  'CreateProgram':                                             286,
  'CreateShader':                                              287,
  'CullFace':                                                  288,
  'DeleteBuffers':                                             289,
  'DeleteBuffersImmediate':                                    290,
  'DeleteFramebuffers':                                        291,
  'DeleteFramebuffersImmediate':                               292,
  'DeleteProgram':                                             293,
  'DeleteRenderbuffers':                                       294,
  'DeleteRenderbuffersImmediate':                              295,
  'DeleteShader':                                              296,
  'DeleteTextures':                                            297,
  'DeleteTexturesImmediate':                                   298,
  'DepthFunc':                                                 299,
  'DepthMask':                                                 300,
  'DepthRangef':                                               301,
  'DetachShader':                                              302,
  'Disable':                                                   303,
  'DisableVertexAttribArray':                                  304,
  'DrawArrays':                                                305,
  'DrawElements':                                              306,
  'Enable':                                                    307,
  'EnableVertexAttribArray':                                   308,
  'Finish':                                                    309,
  'Flush':                                                     310,
  'FramebufferRenderbuffer':                                   311,
  'FramebufferTexture2D':                                      312,
  'FrontFace':                                                 313,
  'GenBuffers':                                                314,
  'GenBuffersImmediate':                                       315,
  'GenerateMipmap':                                            316,
  'GenFramebuffers':                                           317,
  'GenFramebuffersImmediate':                                  318,
  'GenRenderbuffers':                                          319,
  'GenRenderbuffersImmediate':                                 320,
  'GenTextures':                                               321,
  'GenTexturesImmediate':                                      322,
  'GetActiveAttrib':                                           323,
  'GetActiveUniform':                                          324,
  'GetAttachedShaders':                                        325,
  'GetAttribLocation':                                         326,
  'GetAttribLocationImmediate':                                327,
  'GetBooleanv':                                               328,
  'GetBufferParameteriv':                                      329,
  'GetError':                                                  330,
  'GetFloatv':                                                 331,
  'GetFramebufferAttachmentParameteriv':                       332,
  'GetIntegerv':                                               333,
  'GetProgramiv':                                              334,
  'GetProgramInfoLog':                                         335,
  'GetRenderbufferParameteriv':                                336,
  'GetShaderiv':                                               337,
  'GetShaderInfoLog':                                          338,
  'GetShaderPrecisionFormat':                                  339,
  'GetShaderSource':                                           340,
  'GetString':                                                 341,
  'GetTexParameterfv':                                         342,
  'GetTexParameteriv':                                         343,
  'GetUniformfv':                                              344,
  'GetUniformiv':                                              345,
  'GetUniformLocation':                                        346,
  'GetUniformLocationImmediate':                               347,
  'GetVertexAttribfv':                                         348,
  'GetVertexAttribiv':                                         349,
  'GetVertexAttribPointerv':                                   350,
  'Hint':                                                      351,
  'IsBuffer':                                                  352,
  'IsEnabled':                                                 353,
  'IsFramebuffer':                                             354,
  'IsProgram':                                                 355,
  'IsRenderbuffer':                                            356,
  'IsShader':                                                  357,
  'IsTexture':                                                 358,
  'LineWidth':                                                 359,
  'LinkProgram':                                               360,
  'PixelStorei':                                               361,
  'PolygonOffset':                                             362,
  'ReadPixels':                                                363,
  'RenderbufferStorage':                                       364,
  'SampleCoverage':                                            365,
  'Scissor':                                                   366,
  'ShaderSource':                                              367,
  'ShaderSourceImmediate':                                     368,
  'StencilFunc':                                               369,
  'StencilFuncSeparate':                                       370,
  'StencilMask':                                               371,
  'StencilMaskSeparate':                                       372,
  'StencilOp':                                                 373,
  'StencilOpSeparate':                                         374,
  'TexImage2D':                                                375,
  'TexImage2DImmediate':                                       376,
  'TexParameterf':                                             377,
  'TexParameterfv':                                            378,
  'TexParameterfvImmediate':                                   379,
  'TexParameteri':                                             380,
  'TexParameteriv':                                            381,
  'TexParameterivImmediate':                                   382,
  'TexSubImage2D':                                             383,
  'TexSubImage2DImmediate':                                    384,
  'Uniform1f':                                                 385,
  'Uniform1fv':                                                386,
  'Uniform1fvImmediate':                                       387,
  'Uniform1i':                                                 388,
  'Uniform1iv':                                                389,
  'Uniform1ivImmediate':                                       390,
  'Uniform2f':                                                 391,
  'Uniform2fv':                                                392,
  'Uniform2fvImmediate':                                       393,
  'Uniform2i':                                                 394,
  'Uniform2iv':                                                395,
  'Uniform2ivImmediate':                                       396,
  'Uniform3f':                                                 397,
  'Uniform3fv':                                                398,
  'Uniform3fvImmediate':                                       399,
  'Uniform3i':                                                 400,
  'Uniform3iv':                                                401,
  'Uniform3ivImmediate':                                       402,
  'Uniform4f':                                                 403,
  'Uniform4fv':                                                404,
  'Uniform4fvImmediate':                                       405,
  'Uniform4i':                                                 406,
  'Uniform4iv':                                                407,
  'Uniform4ivImmediate':                                       408,
  'UniformMatrix2fv':                                          409,
  'UniformMatrix2fvImmediate':                                 410,
  'UniformMatrix3fv':                                          411,
  'UniformMatrix3fvImmediate':                                 412,
  'UniformMatrix4fv':                                          413,
  'UniformMatrix4fvImmediate':                                 414,
  'UseProgram':                                                415,
  'ValidateProgram':                                           416,
  'VertexAttrib1f':                                            417,
  'VertexAttrib1fv':                                           418,
  'VertexAttrib1fvImmediate':                                  419,
  'VertexAttrib2f':                                            420,
  'VertexAttrib2fv':                                           421,
  'VertexAttrib2fvImmediate':                                  422,
  'VertexAttrib3f':                                            423,
  'VertexAttrib3fv':                                           424,
  'VertexAttrib3fvImmediate':                                  425,
  'VertexAttrib4f':                                            426,
  'VertexAttrib4fv':                                           427,
  'VertexAttrib4fvImmediate':                                  428,
  'VertexAttribPointer':                                       429,
  'Viewport':                                                  430,
  'SwapBuffers':                                               431,
}

# This is a list of enum names and their valid values. It is used to map
# GLenum arguments to a specific set of valid values.
_ENUM_LISTS = {
  'FrameBufferTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_FRAMEBUFFER',
    ],
  },
  'RenderBufferTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_RENDERBUFFER',
    ],
  },
  'BufferTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_ARRAY_BUFFER',
      'GL_ELEMENT_ARRAY_BUFFER',
    ],
    'invalid': [
      'GL_RENDERBUFFER',
    ],
  },
  'BufferUsage': {
    'type': 'GLenum',
    'valid': [
      'GL_STREAM_DRAW',
      'GL_STATIC_DRAW',
      'GL_DYNAMIC_DRAW',
    ],
    'invalid': [
      'GL_STATIC_READ',
    ],
  },
  'TextureTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_TEXTURE_2D',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_X',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_X',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_Y',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_Y',
      'GL_TEXTURE_CUBE_MAP_POSITIVE_Z',
      'GL_TEXTURE_CUBE_MAP_NEGATIVE_Z',
    ],
    'invalid': [
      'GL_PROXY_TEXTURE_CUBE_MAP',
    ]
  },
  'TextureBindTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_TEXTURE_2D',
      'GL_TEXTURE_CUBE_MAP',
    ],
    'invalid': [
      'GL_TEXTURE_1D',
      'GL_TEXTURE_3D',
    ],
  },
  'ShaderType': {
    'type': 'GLenum',
    'valid': [
      'GL_VERTEX_SHADER',
      'GL_FRAGMENT_SHADER',
    ],
  },
  'FaceType': {
    'type': 'GLenum',
    'valid': [
      'GL_FRONT',
      'GL_BACK',
      'GL_FRONT_AND_BACK',
    ],
  },
  'FaceMode': {
    'type': 'GLenum',
    'valid': [
      'GL_CW',
      'GL_CCW',
    ],
  },
  'CmpFunction': {
    'type': 'GLenum',
    'valid': [
      'GL_NEVER',
      'GL_LESS',
      'GL_EQUAL',
      'GL_LEQUAL',
      'GL_GREATER',
      'GL_NOTEQUAL',
      'GL_GEQUAL',
      'GL_ALWAYS',
    ],
  },
  'Equation': {
    'type': 'GLenum',
    'valid': [
      'GL_FUNC_ADD',
      'GL_FUNC_SUBTRACT',
      'GL_FUNC_REVERSE_SUBTRACT',
    ],
    'invalid': [
      'GL_MIN',
      'GL_MAX',
    ],
  },
  'SrcBlendFactor': {
    'type': 'GLenum',
    'valid': [
      'GL_ZERO',
      'GL_ONE',
      'GL_SRC_COLOR',
      'GL_ONE_MINUS_SRC_COLOR',
      'GL_DST_COLOR',
      'GL_ONE_MINUS_DST_COLOR',
      'GL_SRC_ALPHA',
      'GL_ONE_MINUS_SRC_ALPHA',
      'GL_DST_ALPHA',
      'GL_ONE_MINUS_DST_ALPHA',
      'GL_CONSTANT_COLOR',
      'GL_ONE_MINUS_CONSTANT_COLOR',
      'GL_CONSTANT_ALPHA',
      'GL_ONE_MINUS_CONSTANT_ALPHA',
      'GL_SRC_ALPHA_SATURATE',
    ],
  },
  'DstBlendFactor': {
    'type': 'GLenum',
    'valid': [
      'GL_ZERO',
      'GL_ONE',
      'GL_SRC_COLOR',
      'GL_ONE_MINUS_SRC_COLOR',
      'GL_DST_COLOR',
      'GL_ONE_MINUS_DST_COLOR',
      'GL_SRC_ALPHA',
      'GL_ONE_MINUS_SRC_ALPHA',
      'GL_DST_ALPHA',
      'GL_ONE_MINUS_DST_ALPHA',
      'GL_CONSTANT_COLOR',
      'GL_ONE_MINUS_CONSTANT_COLOR',
      'GL_CONSTANT_ALPHA',
      'GL_ONE_MINUS_CONSTANT_ALPHA',
    ],
  },
  'Capability': {
    'type': 'GLenum',
    'valid': [
      'GL_BLEND',
      'GL_CULL_FACE',
      'GL_DEPTH_TEST',
      'GL_DITHER',
      'GL_POLYGON_OFFSET_FILL',
      'GL_SAMPLE_ALPHA_TO_COVERAGE',
      'GL_SAMPLE_COVERAGE',
      'GL_SCISSOR_TEST',
      'GL_STENCIL_TEST',
    ],
    'invalid': [
      'GL_CLIP_PLANE0',
      'GL_POINT_SPRITE',
    ],
  },
  'DrawMode': {
    'type': 'GLenum',
    'valid': [
      'GL_POINTS',
      'GL_LINE_STRIP',
      'GL_LINE_LOOP',
      'GL_LINES',
      'GL_TRIANGLE_STRIP',
      'GL_TRIANGLE_FAN',
      'GL_TRIANGLES',
    ],
    'invalid': [
      'GL_QUADS',
      'GL_POLYGON',
    ],
  },
  'IndexType': {
    'type': 'GLenum',
    'valid': [
      'GL_UNSIGNED_BYTE',
      'GL_UNSIGNED_SHORT',
    ],
    'invalid': [
      'GL_UNSIGNED_INT',
      'GL_INT',
    ],
  },
  'Attachment': {
    'type': 'GLenum',
    'valid': [
      'GL_COLOR_ATTACHMENT0',
      'GL_DEPTH_ATTACHMENT',
      'GL_STENCIL_ATTACHMENT',
    ],
  },
  'BufferParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_BUFFER_SIZE',
      'GL_BUFFER_USAGE',
    ],
    'invalid': [
      'GL_PIXEL_PACK_BUFFER',
    ],
  },
  'FrameBufferParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE',
      'GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME',
      'GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL',
      'GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE',
    ],
  },
  'ProgramParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_DELETE_STATUS',
      'GL_LINK_STATUS',
      'GL_VALIDATE_STATUS',
      'GL_INFO_LOG_LENGTH',
      'GL_ATTACHED_SHADERS',
      'GL_ACTIVE_ATTRIBUTES',
      'GL_ACTIVE_ATTRIBUTE_MAX_LENGTH',
      'GL_ACTIVE_UNIFORMS',
      'GL_ACTIVE_UNIFORM_MAX_LENGTH',
    ],
  },
  'RenderBufferParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_RENDERBUFFER_WIDTH',
      'GL_RENDERBUFFER_HEIGHT',
      'GL_RENDERBUFFER_INTERNAL_FORMAT',
      'GL_RENDERBUFFER_RED_SIZE',
      'GL_RENDERBUFFER_GREEN_SIZE',
      'GL_RENDERBUFFER_BLUE_SIZE',
      'GL_RENDERBUFFER_ALPHA_SIZE',
      'GL_RENDERBUFFER_DEPTH_SIZE',
      'GL_RENDERBUFFER_STENCIL_SIZE',
    ],
  },
  'ShaderParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_SHADER_TYPE',
      'GL_DELETE_STATUS',
      'GL_COMPILE_STATUS',
      'GL_INFO_LOG_LENGTH',
      'GL_SHADER_SOURCE_LENGTH',
    ],
  },
  'ShaderPercision': {
    'type': 'GLenum',
    'valid': [
      'GL_LOW_FLOAT',
      'GL_MEDIUM_FLOAT',
      'GL_HIGH_FLOAT',
      'GL_LOW_INT',
      'GL_MEDIUM_INT',
      'GL_HIGH_INT',
    ],
  },
  'StringType': {
    'type': 'GLenum',
    'valid': [
      'GL_VENDOR',
      'GL_RENDERER',
      'GL_VERSION',
      'GL_SHADING_LANGUAGE_VERSION',
      'GL_EXTENSIONS',
    ],
  },
  'TextureParameter': {
    'type': 'GLenum',
    'valid': [
      'GL_TEXTURE_MAG_FILTER',
      'GL_TEXTURE_MIN_FILTER',
      'GL_TEXTURE_WRAP_S',
      'GL_TEXTURE_WRAP_T',
    ],
    'invalid': [
      'GL_GENERATE_MIPMAP',
    ],
  },
  'VertexAttribute': {
    'type': 'GLenum',
    'valid': [
      'GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING',
      'GL_VERTEX_ATTRIB_ARRAY_ENABLED',
      'GL_VERTEX_ATTRIB_ARRAY_SIZE',
      'GL_VERTEX_ATTRIB_ARRAY_STRIDE',
      'GL_VERTEX_ATTRIB_ARRAY_TYPE',
      'GL_VERTEX_ATTRIB_ARRAY_NORMALIZED',
      'GL_CURRENT_VERTEX_ATTRIB',
    ],
  },
  'VertexPointer': {
    'type': 'GLenum',
    'valid': [
      'GL_VERTEX_ATTRIB_ARRAY_POINTER',
    ],
  },
  'HintTarget': {
    'type': 'GLenum',
    'valid': [
      'GL_GENERATE_MIPMAP_HINT',
    ],
    'invalid': [
      'GL_PERSPECTIVE_CORRECTION_HINT',
    ],
  },
  'HintMode': {
    'type': 'GLenum',
    'valid': [
      'GL_FASTEST',
      'GL_NICEST',
      'GL_DONT_CARE',
    ],
  },
  'PixelStore': {
    'type': 'GLenum',
    'valid': [
      'GL_PACK_ALIGNMENT',
      'GL_UNPACK_ALIGNMENT',
    ],
    'invalid': [
      'GL_PACK_SWAP_BYTES',
      'GL_UNPACK_SWAP_BYTES',
    ],
  },
  'PixelStoreAlignment': {
    'type': 'GLint',
    'valid': [
      '1',
      '2',
      '4',
      '8',
    ],
    'invalid': [
      '3',
      '9',
    ],
  },
  'ReadPixelFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_ALPHA',
      'GL_RGB',
      'GL_RGBA',
    ],
  },
  'PixelType': {
    'type': 'GLenum',
    'valid': [
      'GL_UNSIGNED_BYTE',
      'GL_UNSIGNED_SHORT_5_6_5',
      'GL_UNSIGNED_SHORT_4_4_4_4',
      'GL_UNSIGNED_SHORT_5_5_5_1',
    ],
    'invalid': [
      'GL_SHORT',
      'GL_INT',
    ],
  },
  'RenderBufferFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_RGBA4',
      'GL_RGB565',
      'GL_RGB5_A1',
      'GL_DEPTH_COMPONENT16',
      'GL_STENCIL_INDEX8',
    ],
  },
  'StencilOp': {
    'type': 'GLenum',
    'valid': [
      'GL_KEEP',
      'GL_ZERO',
      'GL_REPLACE',
      'GL_INCR',
      'GL_INCR_WRAP',
      'GL_DECR',
      'GL_DECR_WRAP',
      'GL_INVERT',
    ],
  },
  'TextureFormat': {
    'type': 'GLenum',
    'valid': [
      'GL_ALPHA',
      'GL_LUMINANCE',
      'GL_LUMINANCE_ALPHA',
      'GL_RGB',
      'GL_RGBA',
    ],
    'invalid': [
      'GL_BGRA',
      'GL_BGR',
    ],
  },
  'VertexAttribType': {
    'type': 'GLenum',
    'valid': [
      'GL_BYTE',
      'GL_UNSIGNED_BYTE',
      'GL_SHORT',
      'GL_UNSIGNED_SHORT',
    #  'GL_FIXED',  // This is not available on Desktop GL.
      'GL_FLOAT',
    ],
    'invalid': [
      'GL_DOUBLE',
    ],
  },
  'TextureBorder': {
    'type': 'GLint',
    'valid': [
      '0',
    ],
    'invalid': [
      '1',
    ],
  },
  'VertexAttribSize': {
    'type': 'GLint',
    'valid': [
      '1',
      '2',
      '3',
      '4',
    ],
    'invalid': [
      '0',
      '5',
    ],
  },
  'False': {
    'type': 'GLboolean',
    'valid': [
      'false',
    ],
    'invalid': [
      'true',
    ],
  },
}

# This table specifies types and other special data for the commands that
# will be generated.
#
# type:        defines which handler will be used to generate code.
# DecoderFunc: defines which function to call in the decoder to execute the
#              corresponding GL command. If not specified the GL command will
#              be called directly.
# cmd_args:    The arguments to use for the command. This overrides generating
#              them based on the GL function arguments.
#              a NonImmediate type is a type that stays a pointer even in
#              and immediate version of acommand.
# immediate:   Whether or not to generate an immediate command for the GL
#              function. The default is if there is exactly 1 pointer argument
#              in the GL function an immediate command is generated.
# needs_size:  If true a data_size field is added to the command.
# data_type:   The type of data the command uses. For PUTn or PUT types.
# count:       The number of units per element. For PUTn or PUT types.

_FUNCTION_INFO = {
  'BindAttribLocation': {'type': 'GLchar'},
  'BindBuffer': {'DecoderFunc': 'DoBindBuffer'},
  'BindFramebuffer': {'DecoderFunc': 'glBindFramebufferEXT'},
  'BindRenderbuffer': {'DecoderFunc': 'glBindRenderbufferEXT'},
  'BufferData': {'type': 'Manual', 'immediate': True},
  'BufferSubData': {'type': 'Data'},
  'CheckFramebufferStatus': {'DecoderFunc': 'glCheckFramebufferStatusEXT'},
  'ClearDepthf': {'DecoderFunc': 'glClearDepth'},
  'CompressedTexImage2D': {'type': 'Manual','immediate': True},
  'CompressedTexSubImage2D': {'type': 'Data'},
  'CreateProgram': {'type': 'Create'},
  'CreateShader': {'type': 'Create'},
  'DeleteBuffers': {'type': 'DELn', 'gl_test_func': 'glDeleteBuffersARB'},
  'DeleteFramebuffers': {
    'type': 'DELn',
    'gl_test_func': 'glDeleteFramebuffersEXT',
  },
  'DeleteProgram': {'type': 'Custom', 'DecoderFunc': 'DoDeleteProgram'},
  'DeleteRenderbuffers': {
    'type': 'DELn',
    'gl_test_func': 'glDeleteRenderbuffersEXT',
  },
  'DeleteShader': {'type': 'Custom', 'DecoderFunc': 'DoDeleteShader'},
  'DeleteTextures': {'type': 'DELn'},
  'DepthRangef': {'DecoderFunc': 'glDepthRange'},
  'DisableVertexAttribArray': {'DecoderFunc': 'DoDisableVertexAttribArray'},
  'DrawArrays': { 'DecoderFunc': 'DoDrawArrays', 'unit_test': False},
  'DrawElements': {
    'type': 'Manual',
    'cmd_args': 'GLenum mode, GLsizei count, GLenum type, GLuint index_offset',
  },
  'EnableVertexAttribArray': {'DecoderFunc': 'DoEnableVertexAttribArray'},
  'Finish': {'ImplFunc': False},
  'Flush': {'ImplFunc': False},
  'FramebufferRenderbuffer': {'DecoderFunc': 'glFramebufferRenderbufferEXT'},
  'FramebufferTexture2D': {'DecoderFunc': 'glFramebufferTexture2DEXT'},
  'GenerateMipmap': {'DecoderFunc': 'glGenerateMipmapEXT'},
  'GenBuffers': {'type': 'GENn', 'gl_test_func': 'glGenBuffersARB'},
  'GenFramebuffers': {'type': 'GENn', 'gl_test_func': 'glGenFramebuffersEXT'},
  'GenRenderbuffers': {'type': 'GENn', 'gl_test_func': 'glGenRenderbuffersEXT'},
  'GenTextures': {'type': 'GENn', 'gl_test_func': 'glGenTextures'},
  'GetActiveAttrib': {'type': 'Custom'},
  'GetActiveUniform': {'type': 'Custom'},
  'GetAttachedShaders': {'type': 'Custom'},
  'GetAttribLocation': {
    'type': 'HandWritten',
    'immediate': True,
    'needs_size': True,
    'cmd_args':
        'GLidProgram program, const char* name, NonImmediate GLint* location',
  },
  'GetBooleanv': {'type': 'GETn'},
  'GetBufferParameteriv': {'type': 'GETn'},
  'GetError': {'type': 'Is', 'DecoderFunc': 'GetGLError'},
  'GetFloatv': {'type': 'GETn'},
  'GetFramebufferAttachmentParameteriv': {
    'type': 'GETn',
    'DecoderFunc': 'glGetFramebufferAttachmentParameterivEXT',
  },
  'GetIntegerv': {'type': 'GETn'},
  'GetProgramiv': {'type': 'GETn'},
  'GetProgramInfoLog': {'type': 'STRn'},
  'GetRenderbufferParameteriv': {
    'type': 'GETn',
    'DecoderFunc': 'glGetRenderbufferParameterivEXT',
  },
  'GetShaderiv': {'type': 'GETn'},
  'GetShaderInfoLog': {'type': 'STRn'},
  'GetShaderPrecisionFormat': {'type': 'Custom'},
  'GetShaderSource': {'type': 'STRn'},
  'GetTexParameterfv': {'type': 'GETn'},
  'GetTexParameteriv': {'type': 'GETn'},
  'GetUniformfv': {'type': 'Custom', 'immediate': False},
  'GetUniformiv': {'type': 'Custom', 'immediate': False},
  'GetUniformLocation': {
    'type': 'HandWritten',
    'immediate': True,
    'needs_size': True,
    'cmd_args':
        'GLidProgram program, const char* name, NonImmediate GLint* location',
  },
  'GetVertexAttribfv': {'type': 'GETn'},
  'GetVertexAttribiv': {'type': 'GETn'},
  'GetVertexAttribPointerv': {'type': 'Custom', 'immediate': False},
  'IsBuffer': {'type': 'Is'},
  'IsEnabled': {'type': 'Is'},
  'IsFramebuffer': {'type': 'Is', 'DecoderFunc': 'glIsFramebufferEXT'},
  'IsProgram': {'type': 'Is'},
  'IsRenderbuffer': {'type': 'Is', 'DecoderFunc': 'glIsRenderbufferEXT'},
  'IsShader': {'type': 'Is'},
  'IsTexture': {'type': 'Is'},
  'LinkProgram': {'DecoderFunc': 'DoLinkProgram'},
  'PixelStorei': {'type': 'Manual'},
  'RenderbufferStorage': {'DecoderFunc': 'glRenderbufferStorageEXT'},
  'ReadPixels': {'type': 'Custom', 'immediate': False},
  'ReleaseShaderCompiler': {'type': 'Noop'},
  'ShaderBinary': {'type': 'Noop'},
  'ShaderSource': {
    'type': 'Manual',
    'immediate': True,
    'needs_size': True,
    'cmd_args':
        'GLuint shader, GLsizei count, const char* data',
  },
  'TexImage2D': {'type': 'Manual', 'immediate': True},
  'TexParameterfv': {'type': 'PUT', 'data_type': 'GLfloat', 'count': 1},
  'TexParameteriv': {'type': 'PUT', 'data_type': 'GLint', 'count': 1},
  'TexSubImage2D': {'type': 'Data'},
  'Uniform1fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 1},
  'Uniform1iv': {'type': 'PUTn', 'data_type': 'GLint', 'count': 1},
  'Uniform2fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 2},
  'Uniform2iv': {'type': 'PUTn', 'data_type': 'GLint', 'count': 2},
  'Uniform3fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 3},
  'Uniform3iv': {'type': 'PUTn', 'data_type': 'GLint', 'count': 3},
  'Uniform4fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 4},
  'Uniform4iv': {'type': 'PUTn', 'data_type': 'GLint', 'count': 4},
  'UniformMatrix2fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 4},
  'UniformMatrix3fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 9},
  'UniformMatrix4fv': {'type': 'PUTn', 'data_type': 'GLfloat', 'count': 16},
  'UseProgram': {'DecoderFunc': 'DoUseProgram', 'unit_test': False},
  'VertexAttrib1fv': {'type': 'PUT', 'data_type': 'GLfloat', 'count': 1},
  'VertexAttrib2fv': {'type': 'PUT', 'data_type': 'GLfloat', 'count': 2},
  'VertexAttrib3fv': {'type': 'PUT', 'data_type': 'GLfloat', 'count': 3},
  'VertexAttrib4fv': {'type': 'PUT', 'data_type': 'GLfloat', 'count': 4},
  'VertexAttribPointer': {
      'type': 'Manual',
      'cmd_args': 'GLuint indx, GLint size, GLenum type, GLboolean normalized, '
                  'GLsizei stride, GLuint offset',
  },
  'SwapBuffers': {
    'ImplFunc': False,
    'DecoderFunc': 'DoSwapBuffers',
    'unit_test': False,
  },
}


class CWriter(object):
  """Writes to a file formatting it for Google's style guidelines."""

  def __init__(self, filename):
    self.filename = filename
    self.file = open(filename, "wb")

  def Write(self, string):
    """Writes a string to a file spliting if it's > 80 characters."""
    lines = string.splitlines()
    num_lines = len(lines)
    for ii in range(0, num_lines):
      self.__WriteLine(lines[ii], ii < (num_lines - 1) or string[-1] == '\n')

  def __FindSplit(self, string):
    """Finds a place to split a string."""
    splitter = string.find('=')
    if splitter >= 0 and not string[splitter + 1] == '=' and splitter < 80:
      return splitter
    parts = string.split('(')
    if len(parts) > 1:
      splitter = len(parts[0])
      for ii in range(1, len(parts)):
        if (not parts[ii - 1][-3:] == "if " and
            (len(parts[ii]) > 0 and not parts[ii][0] == ")")
            and splitter < 80):
          return splitter
        splitter += len(parts[ii]) + 1
    done = False
    end = len(string)
    last_splitter = -1
    while not done:
      splitter = string[0:end].rfind(',')
      if splitter < 0:
        return last_splitter
      elif splitter >= 80:
        end = splitter
      else:
        return splitter

  def __WriteLine(self, line, ends_with_eol):
    """Given a signle line, writes it to a file, splitting if it's > 80 chars"""
    if len(line) >= 80:
      i = self.__FindSplit(line)
      if i > 0:
        line1 = line[0:i + 1]
        self.file.write(line1 + '\n')
        match = re.match("( +)", line1)
        indent = ""
        if match:
          indent = match.group(1)
        splitter = line[i]
        if not splitter == ',':
          indent = "    " + indent
        self.__WriteLine(indent + line[i + 1:].lstrip(), True)
        return
    self.file.write(line)
    if ends_with_eol:
      self.file.write('\n')

  def Close(self):
    """Close the file."""
    self.file.close()


class CHeaderWriter(CWriter):
  """Writes a C Header file."""

  _non_alnum_re = re.compile(r'[^a-zA-Z0-9]')

  def __init__(self, filename, file_comment = None):
    CWriter.__init__(self, filename)

    base = os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    hpath = os.path.abspath(filename)[len(base) + 1:]
    self.guard = self._non_alnum_re.sub('_', hpath).upper() + '_'

    self.Write(_LICENSE)
    self.Write(
        "// This file is auto-generated. DO NOT EDIT!\n"
        "\n")
    if not file_comment == None:
      self.Write(file_comment)
    self.Write("#ifndef %s\n" % self.guard)
    self.Write("#define %s\n\n" % self.guard)

  def Close(self):
    self.Write("#endif  // %s\n\n" % self.guard)
    CWriter.Close(self)

class TypeHandler(object):
  """This class emits code for a particular type of function."""

  def __init__(self):
    pass

  def InitFunction(self, func):
    """Add or adjust anything type specific for this function."""
    if func.GetInfo('needs_size'):
      func.AddCmdArg(Argument('data_size', 'uint32'))

  def AddImmediateFunction(self, generator, func):
    """Adds an immediate version of a function."""
    # Generate an immediate command if there is only 1 pointer arg.
    immediate = func.GetInfo('immediate')  # can be True, False or None
    if immediate == True or immediate == None:
      if func.num_pointer_args == 1 or immediate:
        generator.AddFunction(ImmediateFunction(func))

  def WriteStruct(self, func, file):
    """Writes a structure that matches the arguments to a function."""
    file.Write("struct %s {\n" % func.name)
    file.Write("  typedef %s ValueType;\n" % func.name)
    file.Write("  static const CommandId kCmdId = k%s;\n" % func.name)
    func.WriteCmdArgFlag(file)
    file.Write("\n")

    func.WriteCmdComputeSize(file)
    func.WriteCmdSetHeader(file)
    func.WriteCmdInit(file)
    func.WriteCmdSet(file)

    file.Write("  gpu::CommandHeader header;\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("  %s %s;\n" % (arg.cmd_type, arg.name))
    file.Write("};\n")
    file.Write("\n")

    size = len(args) * _SIZE_OF_UINT32 + _SIZE_OF_COMMAND_HEADER
    file.Write("COMPILE_ASSERT(sizeof(%s) == %d,\n" % (func.name, size))
    file.Write("               Sizeof_%s_is_not_%d);\n" % (func.name, size))
    file.Write("COMPILE_ASSERT(offsetof(%s, header) == 0,\n" % func.name)
    file.Write("               OffsetOf_%s_header_not_0);\n" % func.name)
    offset = _SIZE_OF_COMMAND_HEADER
    for arg in args:
      file.Write("COMPILE_ASSERT(offsetof(%s, %s) == %d,\n" %
                 (func.name, arg.name, offset))
      file.Write("               OffsetOf_%s_%s_not_%d);\n" %
                 (func.name, arg.name, offset))
      offset += _SIZE_OF_UINT32
    file.Write("\n")

  def WriteHandlerImplementation(self, func, file):
    """Writes the handler implementation for this command."""
    file.Write("  %s(%s);\n" %
               (func.GetGLFunctionName(), func.MakeOriginalArgString("")))

  def WriteCmdSizeTest(self, func, file):
    """Writes the size test for a command."""
    file.Write("  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);\n")

  def WriteFormatTest(self, func, file):
    """Writes a format test for a command."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  %s cmd = { { 0 } };\n" % func.name)
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 11
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(");\n")
    value = 11
    file.Write("  EXPECT_EQ(static_cast<uint32>(%s::kCmdId),\n" % func.name)
    file.Write("            cmd.header.command);\n")
    func.type_handler.WriteCmdSizeTest(func, file)
    file.Write("  EXPECT_EQ(static_cast<char*>(next_cmd),\n")
    file.Write("            reinterpret_cast<char*>(&cmd) + sizeof(cmd));\n");
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateFormatTest(self, func, file):
    """Writes a format test for an immediate version of a command."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  int8 buf[256] = { 0, };\n")
    file.Write("  %s& cmd = *static_cast<%s*>(static_cast<void*>(&buf));\n" %
               (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 11
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(");\n")
    value = 11
    file.Write("  EXPECT_EQ(static_cast<uint32>(%s::kCmdId),\n" % func.name)
    file.Write("            cmd.header.command);\n")
    func.type_handler.WriteImmediateCmdSizeTest(func, file)
    file.Write("  EXPECT_EQ(static_cast<char*>(next_cmd),\n")
    file.Write("            reinterpret_cast<char*>(&cmd) + sizeof(cmd));\n");
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("}\n")
    file.Write("\n")

  def WriteGetDataSizeCode(self, func, file):
    """Writes the code to set data_size used in validation"""
    pass

  def WriteImmediateCmdSizeTest(self, func, file):
    """Writes a size test for an immediate version of a command."""
    file.Write("  // TODO(gman): Compute correct size.\n")
    file.Write("  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);\n")

  def WriteImmediateHandlerImplementation (self, func, file):
    """Writes the handler impl for the immediate version of a command."""
    file.Write("  %s(%s);\n" %
               (func.GetGLFunctionName(), func.MakeOriginalArgString("")))

  def WriteServiceImplementation(self, func, file):
    """Writes the service implementation for a command."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    if len(func.GetOriginalArgs()) > 0:
      last_arg = func.GetLastOriginalArg()
      all_but_last_arg = func.GetOriginalArgs()[:-1]
      for arg in all_but_last_arg:
        arg.WriteGetCode(file)
      self.WriteGetDataSizeCode(func, file)
      last_arg.WriteGetCode(file)
    func.WriteHandlerValidation(file)
    func.WriteHandlerImplementation(file)
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateServiceImplementation(self, func, file):
    """Writes the service implementation for an immediate version of command."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()
    all_but_last_arg = func.GetOriginalArgs()[:-1]
    for arg in all_but_last_arg:
      arg.WriteGetCode(file)
    self.WriteGetDataSizeCode(func, file)
    last_arg.WriteGetCode(file)
    func.WriteHandlerValidation(file)
    func.WriteHandlerImplementation(file)
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteValidUnitTest(self, func, file, test, extra = {}):
    """Writes a valid unit test."""
    name = func.name
    arg_strings = []
    count = 0
    for arg in func.GetOriginalArgs():
      arg_strings.append(arg.GetValidArg(count, 0))
      count += 1
    gl_arg_strings = []
    count = 0
    for arg in func.GetOriginalArgs():
      gl_arg_strings.append(arg.GetValidGLArg(count, 0))
      count += 1
    gl_func_name = func.GetGLTestFunctionName()
    vars = {
      'name':name,
      'gl_func_name': gl_func_name,
      'args': ", ".join(arg_strings),
      'gl_args': ", ".join(gl_arg_strings),
    }
    vars.update(extra)
    file.Write(test % vars)

  def WriteInvalidUnitTest(self, func, file, test, extra = {}):
    """Writes a invalid unit test."""
    arg_index = 0
    for arg in func.GetOriginalArgs():
      num_invalid_values = arg.GetNumInvalidValues()
      for value_index in range(0, num_invalid_values):
        arg_strings = []
        parse_result = "kNoError"
        count = 0
        for arg in func.GetOriginalArgs():
          if count == arg_index:
            (arg_string, parse_result) = arg.GetInvalidArg(count, value_index)
          else:
            arg_string = arg.GetValidArg(count, 0)
          arg_strings.append(arg_string)
          count += 1
        gl_arg_strings = []
        count = 0
        for arg in func.GetOriginalArgs():
          gl_arg_strings.append("_")
          count += 1
        gl_func_name = func.GetGLFunctionName()
        if gl_func_name.startswith("gl"):
          gl_func_name = gl_func_name[2:]
        else:
          gl_func_name = func.name

        vars = {
            'name': func.name,
            'arg_index': arg_index,
            'value_index': value_index,
            'gl_func_name': gl_func_name,
            'args': ", ".join(arg_strings),
            'all_but_last_args': ", ".join(arg_strings[:-1]),
            'gl_args': ", ".join(gl_arg_strings),
            'parse_result': parse_result,
        }
        vars.update(extra)
        file.Write(test % vars)
      arg_index += 1

  def WriteServiceUnitTest(self, func, file):
    """Writes the service unit test for a command."""
    if func.GetInfo('unit_test') == False:
      file.Write("// TODO(gman): %s\n" % func.name)
      return
    valid_test = """
TEST_F(GLES2DecoderTest, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));
  SpecializedSetup<%(name)s, 0>();
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
"""
    self.WriteValidUnitTest(func, file, valid_test)

    invalid_test = """
TEST_F(GLES2DecoderTest, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<%(name)s, 0>();
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Writes the service unit test for an immediate command."""
    file.Write("// TODO(gman): %s\n" % func.name)

  def WriteImmediateValidationCode(self, func, file):
    """Writes the validation code for an immediate version of a command."""
    pass

  def WriteGLES2ImplementationHeader(self, func, file):
    """Writes the GLES2 Implemention declaration."""
    impl_func = func.GetInfo('ImplFunc')
    if func.can_auto_generate and (impl_func == None or impl_func == True):
      file.Write("%s %s(%s) {\n" %
                 (func.return_type, func.original_name,
                  func.MakeTypedOriginalArgString("")))
      file.Write("  helper_->%s(%s);\n" %
                 (func.name, func.MakeOriginalArgString("")))
      file.Write("}\n")
      file.Write("\n")
    else:
      file.Write("%s %s(%s);\n" %
                 (func.return_type, func.original_name,
                  func.MakeTypedOriginalArgString("")))
      file.Write("\n")

  def WriteImmediateCmdComputeSize(self, func, file):
    """Writes the size computation code for the immediate version of a cmd."""
    file.Write("  static uint32 ComputeSize(uint32 size_in_bytes) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) +  // NOLINT\n")
    file.Write("        RoundSizeToMultipleOfEntries(size_in_bytes));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Writes the SetHeader function for the immediate version of a cmd."""
    file.Write("  void SetHeader(uint32 size_in_bytes) {\n")
    file.Write("    header.SetCmdByTotalSize<ValueType>(size_in_bytes);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Writes the Init function for the immediate version of a command."""
    raise Error

  def WriteImmediateCmdSet(self, func, file):
    """Writes the Set function for the immediate version of a command."""
    raise Error

  def WriteCmdHelper(self, func, file):
    """Writes the cmd helper definition for a cmd."""
    args = func.MakeCmdArgString("")
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedCmdArgString("")))
    file.Write("    gles2::%s& c = GetCmdSpace<gles2::%s>();\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % args)
    file.Write("  }\n\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Writes the cmd helper definition for the immediate version of a cmd."""
    args = func.MakeCmdArgString("")
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedCmdArgString("")))
    file.Write("    const uint32 s = 0;  // TODO(gman): compute correct size\n")
    file.Write("    gles2::%s& c = "
               "GetImmediateCmdSpaceTotalSize<gles2::%s>(s);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % args)
    file.Write("  }\n\n")


class CustomHandler(TypeHandler):
  """Handler for commands that are auto-generated but require minor tweaks."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateCmdGetTotalSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("    uint32 total_size = 0;  // TODO(gman): get correct size.\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void Init(%s) {\n" % func.MakeTypedCmdArgString("_"))
    self.WriteImmediateCmdGetTotalSize(func, file)
    file.Write("    SetHeader(total_size);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s) {\n" %
               func.MakeTypedCmdArgString("_", True))
    self.WriteImmediateCmdGetTotalSize(func, file)
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s);\n" % copy_args)
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, total_size);\n")
    file.Write("  }\n")
    file.Write("\n")


class TodoHandler(CustomHandler):
  """Handle for commands that are not yet implemented."""

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    pass


class HandWrittenHandler(CustomHandler):
  """Handler for comands where everything must be written by hand."""

  def InitFunction(self, func):
    """Add or adjust anything type specific for this function."""
    CustomHandler.InitFunction(self, func)
    func.can_auto_generate = False

  def WriteStruct(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): Write test for %s\n" % func.name)

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): Write test for %s\n" % func.name)


class ManualHandler(CustomHandler):
  """Handler for commands who's handlers must be written by hand."""

  def __init__(self):
    CustomHandler.__init__(self)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    pass

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): Implement test for %s\n" % func.name)

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s);\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("\n")

  def WriteImmediateCmdGetTotalSize(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Move this data to _FUNCTION_INFO?
    if func.name == 'ShaderSourceImmediate':
      file.Write("    uint32 total_size = ComputeSize(_data_size);\n")
    else:
      CustomHandler.WriteImmediateCmdGetTotalSize(self, func, file)


class DataHandler(TypeHandler):
  """Handler for glBufferData, glBufferSubData, glTexImage2D, glTexSubImage2D,
     glCompressedTexImage2D, glCompressedTexImageSub2D."""
  def __init__(self):
    TypeHandler.__init__(self)

  def WriteGetDataSizeCode(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Move this data to _FUNCTION_INFO?
    name = func.name
    if name.endswith("Immediate"):
      name = name[0:-9]
    if name == 'BufferData':
      file.Write("  uint32 data_size = size;\n")
    elif name == 'BufferSubData':
      file.Write("  uint32 data_size = size;\n")
    elif name == 'CompressedTexImage2D':
      file.Write("  uint32 data_size = imageSize;\n")
    elif name == 'CompressedTexSubImage2D':
      file.Write("  uint32 data_size = imageSize;\n")
    elif name == 'TexImage2D':
      file.Write("  uint32 data_size = GLES2Util::ComputeImageDataSize(\n")
      file.Write("      width, height, format, type, unpack_alignment_);\n")
    elif name == 'TexSubImage2D':
      file.Write("  uint32 data_size = GLES2Util::ComputeImageDataSize(\n")
      file.Write("      width, height, format, type, unpack_alignment_);\n")
    else:
      file.Write("// uint32 data_size = 0;  // TODO(gman): get correct size!\n")

  def WriteImmediateCmdGetTotalSize(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Move this data to _FUNCTION_INFO?
    if func.name == 'BufferDataImmediate':
      file.Write("    uint32 total_size = ComputeSize(_size);\n")
    elif func.name == 'BufferSubDataImmediate':
        file.Write("    uint32 total_size = ComputeSize(_size);\n")
    elif func.name == 'CompressedTexImage2DImmediate':
        file.Write("    uint32 total_size = ComputeSize(_imageSize);\n")
    elif func.name == 'CompressedTexSubImage2DImmediate':
        file.Write("    uint32 total_size = ComputeSize(_imageSize);\n")
    elif func.name == 'TexImage2DImmediate':
      file.Write(
          "    uint32 total_size = 0;  // TODO(gman): get correct size\n")
    elif func.name == 'TexSubImage2DImmediate':
      file.Write(
          "    uint32 total_size = 0;  // TODO(gman): get correct size\n")

  def WriteImmediateCmdSizeTest(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Move this data to _FUNCTION_INFO?
    if func.name == 'BufferDataImmediate':
      file.Write("    uint32 total_size = cmd.ComputeSize(cmd.size);\n")
    elif func.name == 'BufferSubDataImmediate':
        file.Write("    uint32 total_size = cmd.ComputeSize(cmd.size);\n")
    elif func.name == 'CompressedTexImage2DImmediate':
        file.Write("    uint32 total_size = cmd.ComputeSize(cmd.imageSize);\n")
    elif func.name == 'CompressedTexSubImage2DImmediate':
        file.Write("    uint32 total_size = cmd.ComputeSize(cmd.imageSize);\n")
    elif func.name == 'TexImage2DImmediate':
      file.Write(
          "    uint32 total_size = 0;  // TODO(gman): get correct size\n")
    elif func.name == 'TexSubImage2DImmediate':
      file.Write(
          "    uint32 total_size = 0;  // TODO(gman): get correct size\n")
    file.Write("  EXPECT_EQ(sizeof(cmd), total_size);\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void Init(%s) {\n" % func.MakeTypedCmdArgString("_"))
    self.WriteImmediateCmdGetTotalSize(func, file)
    file.Write("    SetHeader(total_size);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s) {\n" %
               func.MakeTypedCmdArgString("_", True))
    self.WriteImmediateCmdGetTotalSize(func, file)
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s);\n" % copy_args)
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, total_size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    # TODO(gman): Remove this exception.
    file.Write("// TODO(gman): Implement test for %s\n" % func.name)
    return

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)


class GENnHandler(TypeHandler):
  """Handler for glGen___ type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    pass

  def WriteGetDataSizeCode(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  uint32 data_size = n * sizeof(GLuint);\n")

  def WriteHandlerImplementation (self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  if (!GenGLObjects<GL%sHelper>(n, %s)) {\n"
               "    return error::kInvalidArguments;\n"
               "  }\n" %
               (func.name, func.GetLastOriginalArg().name))

  def WriteImmediateHandlerImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  if (!GenGLObjects<GL%sHelper>(n, %s)) {\n"
               "    return error::kInvalidArguments;\n"
               "  }\n" %
               (func.original_name, func.GetLastOriginalArg().name))

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  MakeIds(%s);\n" % func.MakeOriginalArgString(""))
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("}\n")
    file.Write("\n")

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(GLES2DecoderTest, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GetSharedMemoryAs<GLuint*>()[0] = kNewClientId;
  SpecializedSetup<%(name)s, 0>();
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GetServiceId(kNewClientId), kNewServiceId);
}
"""
    self.WriteValidUnitTest(func, file, valid_test)
    invalid_test = """
TEST_F(GLES2DecoderTest, %(name)sInvalidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(_, _)).Times(0);
  GetSharedMemoryAs<GLuint*>()[0] = client_%(resource_name)s_id_;
  SpecializedSetup<%(name)s, 0>();
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}
"""
    self.WriteValidUnitTest(func, file, invalid_test, {
          'resource_name': func.GetOriginalArgs()[1].name[0:-1]
        })

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(GLES2DecoderTest, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  GLuint temp = kNewClientId;
  SpecializedSetup<%(name)s, 0>();
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GetServiceId(kNewClientId), kNewServiceId);
}
"""
    self.WriteValidUnitTest(func, file, valid_test)
    invalid_test = """
TEST_F(GLES2DecoderTest, %(name)sInvalidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(_, _)).Times(0);
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  SpecializedSetup<%(name)s, 0>();
  cmd.Init(1, &client_%(resource_name)s_id_);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(cmd, sizeof(&client_%(resource_name)s_id_)));
}
"""
    self.WriteValidUnitTest(func, file, invalid_test, {
          'resource_name': func.GetOriginalArgs()[1].name[0:-1]
        })

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize(GLsizei n) {\n")
    file.Write(
        "    return static_cast<uint32>(sizeof(GLuint) * n);  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize(GLsizei n) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) + ComputeDataSize(n));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader(GLsizei n) {\n")
    file.Write("    header.SetCmdByTotalSize<ValueType>(ComputeSize(n));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    file.Write("    SetHeader(_n);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    memcpy(ImmediateDataAddress(this),\n")
    file.Write("           _%s, ComputeDataSize(_n));\n" % last_arg.name)
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    file.Write("    const uint32 size = ComputeSize(_n);\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    args = func.MakeOriginalArgString("")
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedOriginalArgString("")))
    file.Write("    const uint32 size = gles2::%s::ComputeSize(n);\n" %
               func.name)
    file.Write("    gles2::%s& c = "
               "GetImmediateCmdSpaceTotalSize<gles2::%s>(size);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % args)
    file.Write("  }\n\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  static GLuint ids[] = { 12, 23, 34, };\n")
    file.Write("  int8 buf[256] = { 0, };\n")
    file.Write("  %s& cmd = *static_cast<%s*>(static_cast<void*>(&buf));\n" %
               (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 11
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(",\n      ids);\n")
    args = func.GetCmdArgs()
    value = 11
    file.Write("  EXPECT_EQ(static_cast<uint32>(%s::kCmdId),\n" % func.name)
    file.Write("            cmd.header.command);\n")
    file.Write("  EXPECT_EQ(sizeof(cmd) +\n")
    file.Write("            RoundSizeToMultipleOfEntries(cmd.n * 4u),\n")
    file.Write("            cmd.header.size * 4u);\n")
    file.Write("  EXPECT_EQ(static_cast<char*>(next_cmd),\n")
    file.Write("            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +\n");
    file.Write("                RoundSizeToMultipleOfEntries(cmd.n * 4u));\n");
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  // TODO(gman): Check that ids were inserted;\n")
    file.Write("}\n")
    file.Write("\n")


class CreateHandler(TypeHandler):
  """Handler for glCreate___ type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    func.AddCmdArg(Argument("client_id", 'uint32'))

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(GLES2DecoderTest, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s))
      .WillOnce(Return(kNewServiceId));
  SpecializedSetup<%(name)s, 0>();
  %(name)s cmd;
  cmd.Init(%(args)s%(comma)skNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GetServiceId(kNewClientId), kNewServiceId);
}
"""
    comma = ""
    if len(func.GetOriginalArgs()):
      comma =", "
    self.WriteValidUnitTest(func, file, valid_test, {
          'comma': comma,
        })
    invalid_test = """
TEST_F(GLES2DecoderTest, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<%(name)s, 0>();
  %(name)s cmd;
  cmd.Init(%(args)s%(comma)skNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test, {
          'comma': comma,
        })

  def WriteHandlerImplementation (self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  uint32 client_id = c.client_id;\n")
    file.Write("  %sHelper(%s);\n" %
               (func.name, func.MakeCmdArgString("")))

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  GLuint client_id;\n")
    file.Write("  MakeIds(1, &client_id);\n")
    file.Write("  helper_->%s(%s);\n" %
               (func.name, func.MakeCmdArgString("")))
    file.Write("  return client_id;\n")
    file.Write("}\n")
    file.Write("\n")


class DELnHandler(TypeHandler):
  """Handler for glDelete___ type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteGetDataSizeCode(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  uint32 data_size = n * sizeof(GLuint);\n")

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(GLES2DecoderTest, %(name)sValidArgs) {
  EXPECT_CALL(
      *gl_,
      %(gl_func_name)s(1, Pointee(kService%(upper_resource_name)sId)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = client_%(resource_name)s_id_;
  SpecializedSetup<%(name)s, 0>();
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GetServiceId(kNewClientId), 0u);
}
"""
    self.WriteValidUnitTest(func, file, valid_test, {
          'resource_name': func.GetOriginalArgs()[1].name[0:-1],
          'upper_resource_name':
              func.GetOriginalArgs()[1].name[0:-1].capitalize(),
        })
    invalid_test = """
TEST_F(GLES2DecoderTest, %(name)sInvalidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(1, Pointee(0)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = kInvalidClientId;
  SpecializedSetup<%(name)s, 0>();
  %(name)s cmd;
  cmd.Init(%(args)s);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
"""
    self.WriteValidUnitTest(func, file, invalid_test)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(GLES2DecoderTest, %(name)sValidArgs) {
  EXPECT_CALL(
      *gl_,
      %(gl_func_name)s(1, Pointee(kService%(upper_resource_name)sId)))
      .Times(1);
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  SpecializedSetup<%(name)s, 0>();
  cmd.Init(1, &client_%(resource_name)s_id_);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(client_%(resource_name)s_id_)));
  EXPECT_EQ(GetServiceId(kNewClientId), 0u);
}
"""
    self.WriteValidUnitTest(func, file, valid_test, {
          'resource_name': func.GetOriginalArgs()[1].name[0:-1],
          'upper_resource_name':
              func.GetOriginalArgs()[1].name[0:-1].capitalize(),
        })
    invalid_test = """
TEST_F(GLES2DecoderTest, %(name)sInvalidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(1, Pointee(0)))
      .Times(1);
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  SpecializedSetup<%(name)s, 0>();
  GLuint temp = kInvalidClientId;
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}
"""
    self.WriteValidUnitTest(func, file, invalid_test)

  def WriteHandlerImplementation (self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  DeleteGLObjects<GL%sHelper>(n, %s);\n" %
               (func.name, func.GetLastOriginalArg().name))

  def WriteImmediateHandlerImplementation (self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  DeleteGLObjects<GL%sHelper>(n, %s);\n" %
               (func.original_name, func.GetLastOriginalArg().name))

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  FreeIds(%s);\n" % func.MakeOriginalArgString(""))
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize(GLsizei n) {\n")
    file.Write(
        "    return static_cast<uint32>(sizeof(GLuint) * n);  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize(GLsizei n) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) + ComputeDataSize(n));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader(GLsizei n) {\n")
    file.Write("    header.SetCmdByTotalSize<ValueType>(ComputeSize(n));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    file.Write("    SetHeader(_n);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    memcpy(ImmediateDataAddress(this),\n")
    file.Write("           _%s, ComputeDataSize(_n));\n" % last_arg.name)
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    file.Write("    const uint32 size = ComputeSize(_n);\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    args = func.MakeOriginalArgString("")
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedOriginalArgString("")))
    file.Write("    const uint32 size = gles2::%s::ComputeSize(n);\n" %
               func.name)
    file.Write("    gles2::%s& c = "
               "GetImmediateCmdSpaceTotalSize<gles2::%s>(size);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % args)
    file.Write("  }\n\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  static GLuint ids[] = { 12, 23, 34, };\n")
    file.Write("  int8 buf[256] = { 0, };\n")
    file.Write("  %s& cmd = *static_cast<%s*>(static_cast<void*>(&buf));\n" %
               (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 11
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(",\n      ids);\n")
    args = func.GetCmdArgs()
    value = 11
    file.Write("  EXPECT_EQ(static_cast<uint32>(%s::kCmdId),\n" % func.name)
    file.Write("            cmd.header.command);\n")
    file.Write("  EXPECT_EQ(sizeof(cmd) +\n")
    file.Write("            RoundSizeToMultipleOfEntries(cmd.n * 4u),\n")
    file.Write("            cmd.header.size * 4u);\n")
    file.Write("  EXPECT_EQ(static_cast<char*>(next_cmd),\n")
    file.Write("            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +\n");
    file.Write("                RoundSizeToMultipleOfEntries(cmd.n * 4u));\n");
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  // TODO(gman): Check that ids were inserted;\n")
    file.Write("}\n")
    file.Write("\n")


class GETnHandler(TypeHandler):
  """Handler for GETn for glGetBooleanv, glGetFloatv, ... type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def AddImmediateFunction(self, generator, func):
    """Overrriden from TypeHandler."""
    pass

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()

    all_but_last_args = func.GetOriginalArgs()[:-1]
    for arg in all_but_last_args:
      arg.WriteGetCode(file)

    file.Write("  %s params;\n" % last_arg.type)
    file.Write("  GLsizei num_values = util_.GLGetNumValuesReturned(pname);\n")
    file.Write("  uint32 params_size = num_values * sizeof(*params);\n")
    file.Write("  params = GetSharedMemoryAs<%s>(\n" % last_arg.type)
    file.Write("      c.params_shm_id, c.params_shm_offset, params_size);\n")
    func.WriteHandlerValidation(file)
    func.WriteHandlerImplementation(file)
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    all_but_last_args = func.GetOriginalArgs()[:-1]
    arg_string = (
        ", ".join(["%s" % arg.name for arg in all_but_last_args]))
    file.Write("  helper_->%s(%s, result_shm_id(), result_shm_offset());\n" %
               (func.name, arg_string))
    file.Write("  WaitForCmd();\n")
    file.Write("  GLsizei num_values = util_.GLGetNumValuesReturned(pname);\n")
    file.Write(
        "  DCHECK_LE(num_values * sizeof(*params), kMaxSizeOfSimpleResult);\n")
    file.Write(
        "  memcpy(params, result_buffer_, num_values * sizeof(*params));\n")
    file.Write("}\n")
    file.Write("\n")


class PUTHandler(TypeHandler):
  """Handler for glTexParameter_v, glVertexAttrib_v functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Writes the service unit test for a command."""
    valid_test = """
TEST_F(GLES2DecoderTest, %(name)sValidArgs) {
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  EXPECT_CALL(
      *gl_,
      %(gl_func_name)s(%(gl_args)s,
          reinterpret_cast<%(data_type)s*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<%(name)s, 0>();
  %(data_type)s temp[%(data_count)s] = { 0, };
  cmd.Init(%(gl_args)s, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}
"""
    gl_arg_strings = []
    gl_any_strings = []
    count = 0
    for arg in func.GetOriginalArgs()[0:-1]:
      gl_arg_strings.append(arg.GetValidGLArg(count, 0))
      gl_any_strings.append("_")
      count += 1
    extra = {
      'data_type': func.GetInfo('data_type'),
      'data_count': func.GetInfo('count'),
      'gl_args': ", ".join(gl_arg_strings),
      'gl_any_args': ", ".join(gl_any_strings),
    }
    self.WriteValidUnitTest(func, file, valid_test, extra)

    invalid_test = """
TEST_F(GLES2DecoderTest, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_any_args)s, _)).Times(0);
  SpecializedSetup<%(name)s, 0>();
  %(data_type)s temp[%(data_count)s] = { 0, };
  cmd.Init(%(all_but_last_args)s, &temp[0]);
  EXPECT_EQ(error::%(parse_result)s,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test, extra)

  def WriteGetDataSizeCode(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  uint32 data_size = ComputeImmediateDataSize("
               "immediate_data_size, 1, sizeof(%s), %d);\n" %
               (func.info.data_type, func.info.count))

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize() {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(%s) * %d);  // NOLINT\n" %
               (func.info.data_type, func.info.count))
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize() {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write(
        "        sizeof(ValueType) + ComputeDataSize());  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader() {\n")
    file.Write(
        "    header.SetCmdByTotalSize<ValueType>(ComputeSize());\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    file.Write("    SetHeader();\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    memcpy(ImmediateDataAddress(this),\n")
    file.Write("           _%s, ComputeDataSize());\n" % last_arg.name)
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    file.Write("    const uint32 size = ComputeSize();\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    args = func.MakeOriginalArgString("")
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedOriginalArgString("")))
    file.Write("    const uint32 size = gles2::%s::ComputeSize();\n" %
               func.name)
    file.Write("    gles2::%s& c = "
               "GetImmediateCmdSpaceTotalSize<gles2::%s>(size);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % args)
    file.Write("  }\n\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  const int kSomeBaseValueToTestWith = 51;\n")
    file.Write("  static %s data[] = {\n" % func.info.data_type)
    for v in range(0, func.info.count):
      file.Write("    static_cast<%s>(kSomeBaseValueToTestWith + %d),\n" %
                 (func.info.data_type, v))
    file.Write("  };\n")
    file.Write("  int8 buf[256] = { 0, };\n")
    file.Write("  %s& cmd = *static_cast<%s*>(static_cast<void*>(&buf));\n" %
               (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 11
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(",\n      data);\n")
    args = func.GetCmdArgs()
    value = 11
    file.Write("  EXPECT_EQ(static_cast<uint32>(%s::kCmdId),\n" % func.name)
    file.Write("            cmd.header.command);\n")
    file.Write("  EXPECT_EQ(sizeof(cmd) +\n")
    file.Write("            RoundSizeToMultipleOfEntries(sizeof(data)),\n")
    file.Write("            cmd.header.size * 4u);\n")
    file.Write("  EXPECT_EQ(static_cast<char*>(next_cmd),\n")
    file.Write("            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +\n")
    file.Write("                RoundSizeToMultipleOfEntries(sizeof(data)));\n")
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  // TODO(gman): Check that data was inserted;\n")
    file.Write("}\n")
    file.Write("\n")


class PUTnHandler(TypeHandler):
  """Handler for PUTn 'glUniform__v' type functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Writes the service unit test for a command."""
    valid_test = """
TEST_F(GLES2DecoderTest, %(name)sValidArgs) {
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  EXPECT_CALL(
      *gl_,
      %(gl_func_name)s(%(gl_args)s,
          reinterpret_cast<%(data_type)s*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<%(name)s, 0>();
  %(data_type)s temp[%(data_count)s * 2] = { 0, };
  cmd.Init(%(gl_args)s, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}
"""
    gl_arg_strings = []
    gl_any_strings = []
    count = 0
    for arg in func.GetOriginalArgs()[0:-1]:
      gl_arg_strings.append(arg.GetValidGLArg(count, 0))
      gl_any_strings.append("_")
      count += 1
    extra = {
      'data_type': func.GetInfo('data_type'),
      'data_count': func.GetInfo('count'),
      'gl_args': ", ".join(gl_arg_strings),
      'gl_any_args': ", ".join(gl_any_strings),
    }
    self.WriteValidUnitTest(func, file, valid_test, extra)

    invalid_test = """
TEST_F(GLES2DecoderTest, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  %(name)s& cmd = *GetImmediateAs<%(name)s>();
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_any_args)s, _)).Times(0);
  SpecializedSetup<%(name)s, 0>();
  %(data_type)s temp[%(data_count)s * 2] = { 0, };
  cmd.Init(%(all_but_last_args)s, &temp[0]);
  EXPECT_EQ(error::%(parse_result)s,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test, extra)

  def WriteGetDataSizeCode(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  uint32 data_size = ComputeImmediateDataSize("
               "immediate_data_size, 1, sizeof(%s), %d);\n" %
               (func.info.data_type, func.info.count))

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize(GLsizei count) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(%s) * %d * count);  // NOLINT\n" %
               (func.info.data_type, func.info.count))
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize(GLsizei count) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write(
        "        sizeof(ValueType) + ComputeDataSize(count));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader(GLsizei count) {\n")
    file.Write(
        "    header.SetCmdByTotalSize<ValueType>(ComputeSize(count));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void Init(%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_"),
                last_arg.type, last_arg.name))
    file.Write("    SetHeader(_count);\n")
    args = func.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    memcpy(ImmediateDataAddress(this),\n")
    file.Write("           _%s, ComputeDataSize(_count));\n" % last_arg.name)
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    copy_args = func.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s, %s _%s) {\n" %
               (func.MakeTypedCmdArgString("_", True),
                last_arg.type, last_arg.name))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _%s);\n" %
               (copy_args, last_arg.name))
    file.Write("    const uint32 size = ComputeSize(_count);\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    args = func.MakeOriginalArgString("")
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedOriginalArgString("")))
    file.Write("    const uint32 size = gles2::%s::ComputeSize(count);\n" %
               func.name)
    file.Write("    gles2::%s& c = "
               "GetImmediateCmdSpaceTotalSize<gles2::%s>(size);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % args)
    file.Write("  }\n\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  const int kSomeBaseValueToTestWith = 51;\n")
    file.Write("  static %s data[] = {\n" % func.info.data_type)
    for v in range(0, func.info.count * 2):
      file.Write("    static_cast<%s>(kSomeBaseValueToTestWith + %d),\n" %
                 (func.info.data_type, v))
    file.Write("  };\n")
    file.Write("  int8 buf[256] = { 0, };\n")
    file.Write("  %s& cmd = *static_cast<%s*>(static_cast<void*>(&buf));\n" %
               (func.name, func.name))
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    args = func.GetCmdArgs()
    value = 1
    for arg in args:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(",\n      data);\n")
    args = func.GetCmdArgs()
    value = 1
    file.Write("  EXPECT_EQ(static_cast<uint32>(%s::kCmdId),\n" % func.name)
    file.Write("            cmd.header.command);\n")
    file.Write("  EXPECT_EQ(sizeof(cmd) +\n")
    file.Write("            RoundSizeToMultipleOfEntries(sizeof(data)),\n")
    file.Write("            cmd.header.size * 4u);\n")
    file.Write("  EXPECT_EQ(static_cast<char*>(next_cmd),\n")
    file.Write("            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +\n")
    file.Write("                RoundSizeToMultipleOfEntries(sizeof(data)));\n")
    for arg in args:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  // TODO(gman): Check that data was inserted;\n")
    file.Write("}\n")
    file.Write("\n")


class GLcharHandler(TypeHandler):
  """Handler for functions that pass a single string ."""

  def __init__(self):
    TypeHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    func.AddCmdArg(Argument('data_size', 'uint32'))

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()

    all_but_last_arg = func.GetOriginalArgs()[:-1]
    for arg in all_but_last_arg:
      arg.WriteGetCode(file)

    file.Write("  uint32 name_size = c.data_size;\n")
    file.Write("  const char* name = GetSharedMemoryAs<%s>(\n" %
               last_arg.type)
    file.Write("      c.%s_shm_id, c.%s_shm_offset, name_size);\n" %
               (last_arg.name, last_arg.name))
    func.WriteHandlerValidation(file)
    arg_string = ", ".join(["%s" % arg.name for arg in all_but_last_arg])
    file.Write("  String name_str(name, name_size);\n")
    file.Write("  %s(%s, name_str.c_str());\n" %
               (func.GetGLFunctionName(), arg_string))
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()

    all_but_last_arg = func.GetOriginalArgs()[:-1]
    for arg in all_but_last_arg:
      arg.WriteGetCode(file)

    file.Write("  uint32 name_size = c.data_size;\n")
    file.Write(
        "  const char* name = GetImmediateDataAs<const char*>(\n")
    file.Write("      c, name_size, immediate_data_size);\n")
    func.WriteHandlerValidation(file)
    arg_string = ", ".join(["%s" % arg.name for arg in all_but_last_arg])
    file.Write("  String name_str(name, name_size);\n")
    file.Write("  %s(%s, name_str.c_str());\n" %
              (func.GetGLFunctionName(), arg_string))
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  // TODO(gman): This needs to change to use SendString.\n")
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeSize(uint32 data_size) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) + data_size);  // NOLINT\n")
    file.Write("  }\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    code = """
  void SetHeader(uint32 data_size) {
    header.SetCmdBySize<ValueType>(data_size);
  }
"""
    file.Write(code)

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    args = func.GetCmdArgs()
    set_code = []
    for arg in args:
      set_code.append("    %s = _%s;" % (arg.name, arg.name))
    code = """
  void Init(%(typed_args)s, uint32 _data_size) {
    SetHeader(_data_size);
%(set_code)s
    memcpy(ImmediateDataAddress(this), _%(last_arg)s, _data_size);
  }

"""
    file.Write(code % {
          "typed_args": func.MakeTypedOriginalArgString("_"),
          "set_code": "\n".join(set_code),
          "last_arg": last_arg.name
        })

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    last_arg = func.GetLastOriginalArg()
    file.Write("  void* Set(void* cmd%s, uint32 _data_size) {\n" %
               func.MakeTypedOriginalArgString("_", True))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s, _data_size);\n" %
               func.MakeOriginalArgString("_"))
    file.Write("    return NextImmediateCmdAddress<ValueType>("
               "cmd, _data_size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    args = func.MakeOriginalArgString("")
    last_arg = func.GetLastOriginalArg()
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedOriginalArgString("")))
    file.Write("    const uint32 data_size = strlen(name);\n")
    file.Write("    gles2::%s& c = GetImmediateCmdSpace<gles2::%s>("
               "data_size);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s, data_size);\n" % args)
    file.Write("  }\n\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    init_code = []
    check_code = []
    all_but_last_arg = func.GetCmdArgs()[:-1]
    value = 11
    for arg in all_but_last_arg:
      init_code.append("      static_cast<%s>(%d)," % (arg.type, value))
      value += 1
    value = 11
    for arg in all_but_last_arg:
      check_code.append("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);" %
                        (arg.type, value, arg.name))
      value += 1
    code = """
TEST(GLES2FormatTest, %(func_name)s) {
  int8 buf[256] = { 0, };
  %(func_name)s& cmd = *static_cast<%(func_name)s*>(static_cast<void*>(&buf));
  static const char* const test_str = \"test string\";
  void* next_cmd = cmd.Set(
      &cmd,
%(init_code)s
      test_str,
      strlen(test_str));
  EXPECT_EQ(static_cast<uint32>(%(func_name)s::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(strlen(test_str)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(strlen(test_str)));
%(check_code)s
  EXPECT_EQ(static_cast<uint32>(strlen(test_str)), cmd.data_size);
  EXPECT_EQ(0, memcmp(test_str, ImmediateDataAddress(&cmd), strlen(test_str)));
}
"""
    file.Write(code % {
          'func_name': func.name,
          'init_code': "\n".join(init_code),
          'check_code': "\n".join(check_code),
        })

class GetGLcharHandler(GLcharHandler):
  """Handler for glGetAttibLoc, glGetUniformLoc."""

  def __init__(self):
    GLcharHandler.__init__(self)

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()

    all_but_last_arg = func.GetOriginalArgs()
    for arg in all_but_last_arg:
      arg.WriteGetCode(file)

    file.Write("  uint32 name_size = c.data_size;\n")
    file.Write("  const char* name = GetSharedMemoryAs<%s>(\n" %
               last_arg.type)
    file.Write("      c.%s_shm_id, c.%s_shm_offset, name_size);\n" %
               (last_arg.name, last_arg.name))
    file.Write("  GLint* location = GetSharedMemoryAs<GLint*>(\n")
    file.Write(
        "      c.location_shm_id, c.location_shm_offset, sizeof(*location));\n")
    file.Write("  // TODO(gman): Validate location.\n")
    func.WriteHandlerValidation(file)
    arg_string = ", ".join(["%s" % arg.name for arg in all_but_last_arg])
    file.Write("  String name_str(name, name_size);\n")
    file.Write("  *location = %s(%s, name_str.c_str());\n" %
               (func.GetGLFunctionName(), arg_string))
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    last_arg = func.GetLastOriginalArg()

    all_but_last_arg = func.GetOriginalArgs()[:-1]
    for arg in all_but_last_arg:
      arg.WriteGetCode(file)

    file.Write("  uint32 name_size = c.data_size;\n")
    file.Write(
        "  const char* name = GetImmediateDataAs<const char*>(\n")
    file.Write("      c, name_size, immediate_data_size);\n")
    file.Write("  GLint* location = GetSharedMemoryAs<GLint*>(\n")
    file.Write(
        "      c.location_shm_id, c.location_shm_offset, sizeof(*location));\n")
    file.Write("  // TODO(gman): Validate location.\n")
    func.WriteHandlerValidation(file)
    arg_string = ", ".join(["%s" % arg.name for arg in all_but_last_arg])
    file.Write("  String name_str(name, name_size);\n")
    file.Write("  *location = %s(%s, name_str.c_str());\n" %
              (func.GetGLFunctionName(), arg_string))
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    file.Write("  // TODO(gman): This needs to change to use SendString.\n")
    file.Write("  GLint* result = shared_memory_.GetAddressAs<GLint*>(0);\n")
    file.Write("  DCHECK(false);  // pass in shared memory\n")
    file.Write("  helper_->%sImmediate(%s);\n" %
               (func.name, func.MakeOriginalArgString("")))
    file.Write("  int32 token = helper_->InsertToken();\n")
    file.Write("  helper_->WaitForToken(token);\n")
    file.Write("  return *result;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteImmediateCmdComputeSize(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  static uint32 ComputeDataSize(const char* s) {\n")
    file.Write("    return strlen(s);\n")
    file.Write("  }\n")
    file.Write("\n")
    file.Write("  static uint32 ComputeSize(const char* s) {\n")
    file.Write("    return static_cast<uint32>(\n")
    file.Write("        sizeof(ValueType) + ComputeDataSize(s));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSetHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void SetHeader(const char* s) {\n")
    file.Write("    header.SetCmdByTotalSize<ValueType>(ComputeSize(s));\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdInit(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void Init(%s) {\n" % func.MakeTypedInitString("_"))
    file.Write("    SetHeader(_name);\n")
    args = func.GetInitArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("    data_size = ComputeDataSize(_name);\n")
    file.Write("    memcpy(ImmediateDataAddress(this), _name, data_size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdSet(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void* Set(void* cmd%s) {\n" %
               func.MakeTypedInitString("_", True))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s);\n" %
               func.MakeInitString("_"))
    file.Write("    const uint32 size = ComputeSize(_name);\n")
    file.Write("    return NextImmediateCmdAddressTotalSize<ValueType>("
               "cmd, size);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteImmediateCmdHelper(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("  void %s(%s) {\n" %
               (func.name, func.MakeTypedCmdArgString("")))
    file.Write("    const uint32 size = gles2::%s::ComputeSize(name);\n" %
               func.name)
    file.Write("    gles2::%s& c = GetImmediateCmdSpaceTotalSize<gles2::%s>("
               "size);\n" %
               (func.name, func.name))
    file.Write("    c.Init(%s);\n" % func.MakeCmdArgString(""))
    file.Write("  }\n\n")

  def WriteImmediateFormatTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("TEST(GLES2FormatTest, %s) {\n" % func.name)
    file.Write("  int8 buf[256] = { 0, };\n")
    file.Write("  %s& cmd = *static_cast<%s*>(static_cast<void*>(&buf));\n" %
               (func.name, func.name))
    file.Write("  static const char* const test_str = \"test string\";\n")
    file.Write("  void* next_cmd = cmd.Set(\n")
    file.Write("      &cmd")
    all_but_last_arg = func.GetCmdArgs()[:-1]
    value = 11
    for arg in all_but_last_arg:
      file.Write(",\n      static_cast<%s>(%d)" % (arg.type, value))
      value += 1
    file.Write(",\n      test_str);\n")
    value = 11
    file.Write("  EXPECT_EQ(%s::kCmdId ^ cmd.header.command);\n" % func.name)
    file.Write("  EXPECT_EQ(sizeof(cmd)\n")
    file.Write("            RoundSizeToMultipleOfEntries(strlen(test_str)),\n")
    file.Write("            cmd.header.size * 4u);\n")
    file.Write("  EXPECT_EQ(static_cast<char*>(next_cmd),\n")
    file.Write("            reinterpret_cast<char*>(&cmd) + sizeof(cmd));\n");
    for arg in all_but_last_arg:
      file.Write("  EXPECT_EQ(static_cast<%s>(%d), cmd.%s);\n" %
                 (arg.type, value, arg.name))
      value += 1
    file.Write("  // TODO(gman): check that string got copied.\n")
    file.Write("}\n")
    file.Write("\n")


class IsHandler(TypeHandler):
  """Handler for glIs____ type and glGetError functions."""

  def __init__(self):
    TypeHandler.__init__(self)

  def InitFunction(self, func):
    """Overrriden from TypeHandler."""
    func.AddCmdArg(Argument("result_shm_id", 'uint32'))
    func.AddCmdArg(Argument("result_shm_offset", 'uint32'))

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    valid_test = """
TEST_F(GLES2DecoderTest, %(name)sValidArgs) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s));
  SpecializedSetup<%(name)s, 0>();
  %(name)s cmd;
  cmd.Init(%(args)s%(comma)sshared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
"""
    comma = ""
    if len(func.GetOriginalArgs()):
      comma =", "
    self.WriteValidUnitTest(func, file, valid_test, {
          'comma': comma,
        })

    invalid_test = """
TEST_F(GLES2DecoderTest, %(name)sInvalidArgs%(arg_index)d_%(value_index)d) {
  EXPECT_CALL(*gl_, %(gl_func_name)s(%(gl_args)s)).Times(0);
  SpecializedSetup<%(name)s, 0>();
  %(name)s cmd;
  cmd.Init(%(args)s%(comma)sshared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::%(parse_result)s, ExecuteCmd(cmd));
}
"""
    self.WriteInvalidUnitTest(func, file, invalid_test, {
          'comma': comma,
        })

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    args = func.GetOriginalArgs()
    for arg in args:
      arg.WriteGetCode(file)

    file.Write("  %s* result_dst = GetSharedMemoryAs<%s*>(\n" %
               (func.return_type, func.return_type))
    file.Write(
        "      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));\n")
    func.WriteHandlerValidation(file)
    file.Write("  *result_dst = %s(%s);\n" %
               (func.GetGLFunctionName(), func.MakeOriginalArgString("")))
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("%s %s(%s) {\n" %
               (func.return_type, func.original_name,
                func.MakeTypedOriginalArgString("")))
    arg_string = func.MakeOriginalArgString("")
    comma = ""
    if len(arg_string) > 0:
      comma = ", "
    file.Write("  helper_->%s(%s%sresult_shm_id(), result_shm_offset());\n" %
               (func.name, arg_string, comma))
    file.Write("  WaitForCmd();\n")
    file.Write("  return GetResultAs<%s>();\n" %
               func.return_type)
    file.Write("}\n")
    file.Write("\n")


class STRnHandler(TypeHandler):
  """Handler for GetProgramInfoLog, GetShaderInfoLog and GetShaderSource."""

  def __init__(self):
    TypeHandler.__init__(self)

  def WriteGLES2ImplementationHeader(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): Implement this\n")
    TypeHandler.WriteGLES2ImplementationHeader(self, func, file)

  def WriteServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteImmediateServiceUnitTest(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write("// TODO(gman): %s\n\n" % func.name)

  def WriteServiceImplementation(self, func, file):
    """Overrriden from TypeHandler."""
    file.Write(
        "error::Error GLES2DecoderImpl::Handle%s(\n" % func.name)
    file.Write(
        "    uint32 immediate_data_size, const gles2::%s& c) {\n" % func.name)
    args = func.GetOriginalArgs()
    all_but_last_2_args = args[:-2]
    for arg in all_but_last_2_args:
      arg.WriteGetCode(file)
    self.WriteGetDataSizeCode(func, file)
    size_arg = args[-2]
    file.Write("  uint32 size_shm_id = c.%s_shm_id;\n" % size_arg.name)
    file.Write("  uint32 size_shm_offset = c.%s_shm_offset;\n" % size_arg.name)
    file.Write("  GLsizei* length = NULL;\n")
    file.Write("  if (size_shm_id != 0 || size_shm_offset != 0) {\n"
               "    length = GetSharedMemoryAs<GLsizei*>(\n"
               "        size_shm_id, size_shm_offset, sizeof(*length));\n"
               "    if (!length) {\n"
               "      return error::kOutOfBounds;\n"
               "    }\n"
               "  }\n")
    dest_arg = args[-1]
    bufsize_arg = args[-3]
    file.Write(
        "  %s %s = GetSharedMemoryAs<%s>(\n" %
        (dest_arg.type, dest_arg.name, dest_arg.type))
    file.Write(
        "      c.%s_shm_id, c.%s_shm_offset, %s);\n" %
        (dest_arg.name, dest_arg.name, bufsize_arg.name))
    for arg in all_but_last_2_args + [dest_arg]:
      arg.WriteValidationCode(file)
    func.WriteValidationCode(file)
    func.WriteHandlerImplementation(file)
    file.Write("  return error::kNoError;\n")
    file.Write("}\n")
    file.Write("\n")


class FunctionInfo(object):
  """Holds info about a function."""

  def __init__(self, info, type_handler):
    for key in info:
      setattr(self, key, info[key])
    self.type_handler = type_handler
    if not 'type' in info:
      self.type = ''


class Argument(object):
  """A class that represents a function argument."""

  cmd_type_map_ = {
    'GLint': 'int32',
    'GLsizei': 'int32',
    'GLfloat': 'float',
    'GLclampf': 'float',
  }

  def __init__(self, name, type):
    self.name = name
    self.type = type

    if type in self.cmd_type_map_:
      self.cmd_type = self.cmd_type_map_[type]
    else:
      self.cmd_type = 'uint32'

  def IsPointer(self):
    """Returns true if argument is a pointer."""
    return False

  def AddCmdArgs(self, args):
    """Adds command arguments for this argument to the given list."""
    return args.append(self)

  def AddInitArgs(self, args):
    """Adds init arguments for this argument to the given list."""
    return args.append(self)

  def GetValidArg(self, offset, index):
    return str(offset + 1)

  def GetValidGLArg(self, offset, index):
    return str(offset + 1)

  def GetNumInvalidValues(self):
    """returns the number of invalid values to be tested."""
    return 0

  def GetInvalidArg(self, offset, index):
    """returns an invalid value and expected parse result by index."""
    return ("---ERROR0---", "---ERROR2---")

  def WriteGetCode(self, file):
    """Writes the code to get an argument from a command structure."""
    file.Write("  %s %s = static_cast<%s>(c.%s);\n" %
               (self.type, self.name, self.type, self.name))

  def WriteValidationCode(self, file):
    """Writes the validation code for an argument."""
    pass

  def WriteGetAddress(self, file):
    """Writes the code to get the address this argument refers to."""
    pass

  def GetImmediateVersion(self):
    """Gets the immediate version of this argument."""
    return self

class EnumBaseArgument(Argument):
  """Base calss for EnumArgument, IntArgument and BoolArgument"""

  def __init__(self, name, gl_type, type, gl_error):
    Argument.__init__(self, name, gl_type)

    self.local_type = type
    self.gl_error = gl_error
    name = type[len(gl_type):]
    self.enum_info = _ENUM_LISTS[name]

  def WriteValidationCode(self, file):
    file.Write("  if (!Validate%s(%s)) {\n" % (self.local_type, self.name))
    file.Write("    SetGLError(%s);\n" % self.gl_error)
    file.Write("    return error::kNoError;\n")
    file.Write("  }\n")

  def GetValidArg(self, offset, index):
    if 'valid' in self.enum_info:
      valid = self.enum_info['valid']
      num_valid = len(valid)
      if index >= num_valid:
        index = num_valid - 1
      return valid[index]
    return str(offset + 1)

  def GetValidGLArg(self, offset, index):
    return self.GetValidArg(offset, index)

  def GetNumInvalidValues(self):
    """returns the number of invalid values to be tested."""
    if 'invalid' in self.enum_info:
      invalid = self.enum_info['invalid']
      return len(invalid)
    return 0

  def GetInvalidArg(self, offset, index):
    """returns an invalid value by index."""
    if 'invalid' in self.enum_info:
      invalid = self.enum_info['invalid']
      num_invalid = len(invalid)
      if index >= num_invalid:
        index = num_invalid - 1
      return (invalid[index], "kNoError")
    return ("---ERROR1---", "kNoError")


class EnumArgument(EnumBaseArgument):
  """A class that represents a GLenum argument"""

  def __init__(self, name, type):
    EnumBaseArgument.__init__(self, name, "GLenum", type, "GL_INVALID_ENUM")


class IntArgument(EnumBaseArgument):
  """A class for a GLint argument that can only except specific values.

  For example glTexImage2D takes a GLint for its internalformat
  argument instead of a GLenum.
  """

  def __init__(self, name, type):
    EnumBaseArgument.__init__(self, name, "GLint", type, "GL_INVALID_VALUE")


class BoolArgument(EnumBaseArgument):
  """A class for a GLboolean argument that can only except specific values.

  For example glUniformMatrix takes a GLboolean for it's transpose but it
  must be false.
  """

  def __init__(self, name, type):
    EnumBaseArgument.__init__(self, name, "GLboolean", type, "GL_INVALID_VALUE")


class ImmediatePointerArgument(Argument):
  """A class that represents an immediate argument to a function.

  An immediate argument is one where the data follows the command.
  """

  def __init__(self, name, type):
    Argument.__init__(self, name, type)

  def AddCmdArgs(self, args):
    """Overridden from Argument."""
    pass

  def WriteGetCode(self, file):
    """Overridden from Argument."""
    file.Write(
      "  %s %s = GetImmediateDataAs<%s>(\n" %
      (self.type, self.name, self.type))
    file.Write("      c, data_size, immediate_data_size);\n")

  def WriteValidationCode(self, file):
    """Overridden from Argument."""
    file.Write("  if (%s == NULL) {\n" % self.name)
    file.Write("    return error::kOutOfBounds;\n")
    file.Write("  }\n")

  def GetImmediateVersion(self):
    """Overridden from Argument."""
    return None


class PointerArgument(Argument):
  """A class that represents a pointer argument to a function."""

  def __init__(self, name, type):
    Argument.__init__(self, name, type)

  def IsPointer(self):
    """Returns true if argument is a pointer."""
    return True

  def GetValidArg(self, offset, index):
    """Overridden from Argument."""
    return "shared_memory_id_, shared_memory_offset_"

  def GetValidGLArg(self, offset, index):
    """Overridden from Argument."""
    return "reinterpret_cast<%s>(shared_memory_address_)" % self.type

  def GetNumInvalidValues(self):
    """Overridden from Argument."""
    return 2

  def GetInvalidArg(self, offset, index):
    """Overridden from Argument."""
    if index == 0:
      return ("kInvalidSharedMemoryId, 0", "kOutOfBounds")
    else:
      return ("shared_memory_id_, kInvalidSharedMemoryOffset",
              "kOutOfBounds")

  def AddCmdArgs(self, args):
    """Overridden from Argument."""
    args.append(Argument("%s_shm_id" % self.name, 'uint32'))
    args.append(Argument("%s_shm_offset" % self.name, 'uint32'))

  def WriteGetCode(self, file):
    """Overridden from Argument."""
    file.Write(
        "  %s %s = GetSharedMemoryAs<%s>(\n" %
        (self.type, self.name, self.type))
    file.Write(
        "      c.%s_shm_id, c.%s_shm_offset, data_size);\n" %
        (self.name, self.name))

  def WriteGetAddress(self, file):
    """Overridden from Argument."""
    file.Write(
        "  %s %s = GetSharedMemoryAs<%s>(\n" %
        (self.type, self.name, self.type))
    file.Write(
        "      %s_shm_id, %s_shm_offset, %s_size);\n" %
        (self.name, self.name, self.name))

  def WriteValidationCode(self, file):
    """Overridden from Argument."""
    file.Write("  if (%s == NULL) {\n" % self.name)
    file.Write("    return error::kOutOfBounds;\n")
    file.Write("  }\n")

  def GetImmediateVersion(self):
    """Overridden from Argument."""
    return ImmediatePointerArgument(self.name, self.type)


class NonImmediatePointerArgument(PointerArgument):
  """A pointer argument that stays a pointer even in an immediate cmd."""

  def __init__(self, name, type):
    PointerArgument.__init__(self, name, type)

  def IsPointer(self):
    """Returns true if argument is a pointer."""
    return False

  def GetImmediateVersion(self):
    """Overridden from Argument."""
    return self


class ResourceIdArgument(Argument):
  """A class that represents a resource id argument to a function."""

  def __init__(self, name, type):
    match = re.match("(GLid\w+)", type)
    self.resource_type = match.group(1)[4:]
    type = type.replace(match.group(1), "GLuint")
    Argument.__init__(self, name, type)

  def WriteGetCode(self, file):
    """Overridden from Argument."""
    file.Write("  %s %s;\n" % (self.type, self.name))
    file.Write("  if (!id_manager_->GetServiceId(c.%s, &%s)) {\n" %
               (self.name, self.name))
    file.Write("    SetGLError(GL_INVALID_VALUE);\n")
    file.Write("    return error::kNoError;\n")
    file.Write("  }\n")

  def GetValidArg(self, offset, index):
    return "client_%s_id_" % self.resource_type.lower()

  def GetValidGLArg(self, offset, index):
    return "kService%sId" % self.resource_type

class Function(object):
  """A class that represents a function."""

  def __init__(self, name, info, return_type, original_args, args_for_cmds,
               cmd_args, init_args, num_pointer_args):
    self.name = name
    self.original_name = name
    self.info = info
    self.type_handler = info.type_handler
    self.return_type = return_type
    self.original_args = original_args
    self.num_pointer_args = num_pointer_args
    self.can_auto_generate = num_pointer_args == 0 and return_type == "void"
    self.cmd_args = cmd_args
    self.init_args = init_args
    self.args_for_cmds = args_for_cmds
    self.type_handler.InitFunction(self)

  def IsType(self, type_name):
    """Returns true if function is a certain type."""
    return self.info.type == type_name

  def GetInfo(self, name):
    """Returns a value from the function info for this function."""
    if hasattr(self.info, name):
      return getattr(self.info, name)
    return None

  def GetGLFunctionName(self):
    """Gets the function to call to execute GL for this command."""
    if self.GetInfo('DecoderFunc'):
      return self.GetInfo('DecoderFunc')
    return "gl%s" % self.original_name

  def GetGLTestFunctionName(self):
    gl_func_name = self.GetInfo('gl_test_func')
    if gl_func_name == None:
      gl_func_name = self.GetGLFunctionName()
    if gl_func_name.startswith("gl"):
      gl_func_name = gl_func_name[2:]
    else:
      gl_func_name = self.name
    return gl_func_name

  def AddCmdArg(self, arg):
    """Adds a cmd argument to this function."""
    self.cmd_args.append(arg)

  def GetCmdArgs(self):
    """Gets the command args for this function."""
    return self.cmd_args

  def GetInitArgs(self):
    """Gets the init args for this function."""
    return self.init_args

  def GetOriginalArgs(self):
    """Gets the original arguments to this function."""
    return self.original_args

  def GetLastOriginalArg(self):
    """Gets the last original argument to this function."""
    return self.original_args[len(self.original_args) - 1]

  def __GetArgList(self, arg_string, add_comma):
    """Adds a comma if arg_string is not empty and add_comma is true."""
    comma = ""
    if add_comma and len(arg_string):
      comma = ", "
    return "%s%s" % (comma, arg_string)

  def MakeTypedOriginalArgString(self, prefix, add_comma = False):
    """Gets a list of arguments as they arg in GL."""
    args = self.GetOriginalArgs()
    arg_string = ", ".join(
        ["%s %s%s" % (arg.type, prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeOriginalArgString(self, prefix, add_comma = False):
    """Gets the list of arguments as they are in GL."""
    args = self.GetOriginalArgs()
    arg_string = ", ".join(
        ["%s%s" % (prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeTypedCmdArgString(self, prefix, add_comma = False):
    """Gets a typed list of arguments as they need to be for command buffers."""
    args = self.GetCmdArgs()
    arg_string = ", ".join(
        ["%s %s%s" % (arg.type, prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeCmdArgString(self, prefix, add_comma = False):
    """Gets the list of arguments as they need to be for command buffers."""
    args = self.GetCmdArgs()
    arg_string = ", ".join(
        ["%s%s" % (prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeTypedInitString(self, prefix, add_comma = False):
    """Gets a typed list of arguments as they need to be for cmd Init/Set."""
    args = self.GetInitArgs()
    arg_string = ", ".join(
        ["%s %s%s" % (arg.type, prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def MakeInitString(self, prefix, add_comma = False):
    """Gets the list of arguments as they need to be for cmd Init/Set."""
    args = self.GetInitArgs()
    arg_string = ", ".join(
        ["%s%s" % (prefix, arg.name) for arg in args])
    return self.__GetArgList(arg_string, add_comma)

  def WriteHandlerValidation(self, file):
    """Writes validation code for the function."""
    for arg in self.GetOriginalArgs():
      arg.WriteValidationCode(file)
    self.WriteValidationCode(file)

  def WriteHandlerImplementation(self, file):
    """Writes the handler implementation for this command."""
    self.type_handler.WriteHandlerImplementation(self, file)

  def WriteValidationCode(self, file):
    """Writes the validation code for a command."""
    pass

  def WriteCmdArgFlag(self, file):
    """Writes the cmd kArgFlags constant."""
    file.Write("  static const cmd::ArgFlags kArgFlags = cmd::kFixed;\n")

  def WriteCmdComputeSize(self, file):
    """Writes the ComputeSize function for the command."""
    file.Write("  static uint32 ComputeSize() {\n")
    file.Write(
        "    return static_cast<uint32>(sizeof(ValueType));  // NOLINT\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteCmdSetHeader(self, file):
    """Writes the cmd's SetHeader function."""
    file.Write("  void SetHeader() {\n")
    file.Write("    header.SetCmd<ValueType>();\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteCmdInit(self, file):
    """Writes the cmd's Init function."""
    file.Write("  void Init(%s) {\n" % self.MakeTypedCmdArgString("_"))
    file.Write("    SetHeader();\n")
    args = self.GetCmdArgs()
    for arg in args:
      file.Write("    %s = _%s;\n" % (arg.name, arg.name))
    file.Write("  }\n")
    file.Write("\n")

  def WriteCmdSet(self, file):
    """Writes the cmd's Set function."""
    copy_args = self.MakeCmdArgString("_", False)
    file.Write("  void* Set(void* cmd%s) {\n" %
               self.MakeTypedCmdArgString("_", True))
    file.Write("    static_cast<ValueType*>(cmd)->Init(%s);\n" % copy_args)
    file.Write("    return NextCmdAddress<ValueType>(cmd);\n")
    file.Write("  }\n")
    file.Write("\n")

  def WriteStruct(self, file):
    self.type_handler.WriteStruct(self, file)

  def WriteCmdHelper(self, file):
    """Writes the cmd's helper."""
    self.type_handler.WriteCmdHelper(self, file)

  def WriteServiceImplementation(self, file):
    """Writes the service implementation for a command."""
    self.type_handler.WriteServiceImplementation(self, file)

  def WriteServiceUnitTest(self, file):
    """Writes the service implementation for a command."""
    self.type_handler.WriteServiceUnitTest(self, file)

  def WriteGLES2ImplementationHeader(self, file):
    """Writes the GLES2 Implemention declaration."""
    self.type_handler.WriteGLES2ImplementationHeader(self, file)

  def WriteFormatTest(self, file):
    """Writes the cmd's format test."""
    self.type_handler.WriteFormatTest(self, file)


class ImmediateFunction(Function):
  """A class that represnets an immediate function command."""

  def __init__(self, func):
    new_args = []
    for arg in func.GetOriginalArgs():
      new_arg = arg.GetImmediateVersion()
      if new_arg:
        new_args.append(new_arg)

    cmd_args = []
    new_args_for_cmds = []
    for arg in func.args_for_cmds:
      new_arg = arg.GetImmediateVersion()
      if new_arg:
        new_args_for_cmds.append(new_arg)
        new_arg.AddCmdArgs(cmd_args)

    new_init_args = []
    for arg in new_args_for_cmds:
      arg.AddInitArgs(new_init_args)

    Function.__init__(
        self,
        "%sImmediate" % func.name,
        func.info,
        func.return_type,
        new_args,
        new_args_for_cmds,
        cmd_args,
        new_init_args,
        0)
    self.original_name = func.name

  def WriteServiceImplementation(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateServiceImplementation(self, file)

  def WriteHandlerImplementation(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateHandlerImplementation(self, file)

  def WriteServiceUnitTest(self, file):
    """Writes the service implementation for a command."""
    self.type_handler.WriteImmediateServiceUnitTest(self, file)

  def WriteValidationCode(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateValidationCode(self, file)

  def WriteCmdArgFlag(self, file):
    """Overridden from Function"""
    file.Write("  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;\n")

  def WriteCmdComputeSize(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdComputeSize(self, file)

  def WriteCmdSetHeader(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdSetHeader(self, file)

  def WriteCmdInit(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdInit(self, file)

  def WriteCmdSet(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdSet(self, file)

  def WriteCmdHelper(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateCmdHelper(self, file)

  def WriteFormatTest(self, file):
    """Overridden from Function"""
    self.type_handler.WriteImmediateFormatTest(self, file)


def CreateArg(arg_string):
  """Creates an Argument."""
  arg_parts = arg_string.split()
  if len(arg_parts) == 1 and arg_parts[0] == 'void':
    return None
  # Is this a pointer argument?
  elif arg_string.find('*') >= 0:
    if arg_parts[0] == 'NonImmediate':
      return NonImmediatePointerArgument(
          arg_parts[-1],
          " ".join(arg_parts[1:-1]))
    else:
      return PointerArgument(
          arg_parts[-1],
          " ".join(arg_parts[0:-1]))
  # Is this a resource argument? Must come after pointer check.
  elif arg_parts[0].startswith('GLid'):
    return ResourceIdArgument(arg_parts[-1], " ".join(arg_parts[0:-1]))
  elif arg_parts[0].startswith('GLenum') and len(arg_parts[0]) > 6:
    return EnumArgument(arg_parts[-1], " ".join(arg_parts[0:-1]))
  elif arg_parts[0].startswith('GLboolean') and len(arg_parts[0]) > 9:
    return BoolArgument(arg_parts[-1], " ".join(arg_parts[0:-1]))
  elif (arg_parts[0].startswith('GLint') and len(arg_parts[0]) > 5 and
        arg_parts[0] != "GLintptr"):
    return IntArgument(arg_parts[-1], " ".join(arg_parts[0:-1]))
  else:
    return Argument(arg_parts[-1], " ".join(arg_parts[0:-1]))


class GLGenerator(object):
  """A class to generate GL command buffers."""

  _function_re = re.compile(r'GL_APICALL(.*?)GL_APIENTRY (.*?) \((.*?)\);')

  def __init__(self, verbose):
    self.original_functions = []
    self.functions = []
    self.verbose = verbose
    self.errors = 0
    self._function_info = {}
    self._empty_type_handler = TypeHandler()
    self._empty_function_info = FunctionInfo({}, self._empty_type_handler)

    self._type_handlers = {
      'Create': CreateHandler(),
      'Custom': CustomHandler(),
      'Data': DataHandler(),
      'DELn': DELnHandler(),
      'GENn': GENnHandler(),
      'GETn': GETnHandler(),
      'GetGLchar': GetGLcharHandler(),
      'GLchar': GLcharHandler(),
      'HandWritten': HandWrittenHandler(),
      'Is': IsHandler(),
      'Manual': ManualHandler(),
      'PUT': PUTHandler(),
      'PUTn': PUTnHandler(),
      'STRn': STRnHandler(),
      'Todo': TodoHandler(),
    }

    for func_name in _FUNCTION_INFO:
      info = _FUNCTION_INFO[func_name]
      type = ''
      if 'type' in info:
        type = info['type']
      self._function_info[func_name] = FunctionInfo(info,
                                                    self.GetTypeHandler(type))

  def AddFunction(self, func):
    """Adds a function."""
    self.functions.append(func)

  def GetTypeHandler(self, name):
    """Gets a type info for the given type."""
    if name in self._type_handlers:
      return self._type_handlers[name]
    return self._empty_type_handler

  def GetFunctionInfo(self, name):
    """Gets a type info for the given function name."""
    if name in self._function_info:
      return self._function_info[name]
    return self._empty_function_info

  def Log(self, msg):
    """Prints something if verbose is true."""
    if self.verbose:
      print msg

  def Error(self, msg):
    """Prints an error."""
    print "Error: %s" % msg
    self.errors += 1

  def WriteLicense(self, file):
    """Writes the license."""
    file.Write(_LICENSE)

  def WriteNamespaceOpen(self, file):
    """Writes the code for the namespace."""
    file.Write("namespace gpu {\n")
    file.Write("namespace gles2 {\n")
    file.Write("\n")

  def WriteNamespaceClose(self, file):
    """Writes the code to close the namespace."""
    file.Write("}  // namespace gles2\n")
    file.Write("}  // namespace gpu\n")
    file.Write("\n")

  def ParseArgs(self, arg_string):
    """Parses a function arg string."""
    args = []
    num_pointer_args = 0
    parts = arg_string.split(',')
    is_gl_enum = False
    for arg_string in parts:
      if arg_string.startswith('GLenum '):
        is_gl_enum = True
      arg = CreateArg(arg_string)
      if arg:
        args.append(arg)
        if arg.IsPointer():
          num_pointer_args += 1
    return (args, num_pointer_args, is_gl_enum)

  def ParseGLH(self, filename):
    """Parses the GL2.h file and extracts the functions"""
    for line in _GL_FUNCTIONS.splitlines():
      match = self._function_re.match(line)
      if match:
        func_name = match.group(2)[2:]
        func_info = self.GetFunctionInfo(func_name)
        if func_info.type != 'Noop':
          return_type = match.group(1).strip()
          arg_string = match.group(3)
          (args, num_pointer_args, is_gl_enum) = self.ParseArgs(arg_string)
          if is_gl_enum:
            self.Log("%s uses bare GLenum" % func_name)
          args_for_cmds = args
          if hasattr(func_info, 'cmd_args'):
            (args_for_cmds, num_pointer_args, is_gl_enum) = (
                self.ParseArgs(getattr(func_info, 'cmd_args')))
          cmd_args = []
          for arg in args_for_cmds:
            arg.AddCmdArgs(cmd_args)
          init_args = []
          for arg in args_for_cmds:
            arg.AddInitArgs(init_args)
          return_arg = CreateArg(return_type + " result")
          if return_arg:
            init_args.append(return_arg)
          f = Function(func_name, func_info, return_type, args, args_for_cmds,
                       cmd_args, init_args, num_pointer_args)
          self.original_functions.append(f)
          self.AddFunction(f)
          f.type_handler.AddImmediateFunction(self, f)

    self.Log("Auto Generated Functions    : %d" %
             len([f for f in self.functions if f.can_auto_generate or
                  (not f.IsType('') and not f.IsType('Custom') and
                   not f.IsType('Todo'))]))

    funcs = [f for f in self.functions if not f.can_auto_generate and
             (f.IsType('') or f.IsType('Custom') or f.IsType('Todo'))]
    self.Log("Non Auto Generated Functions: %d" % len(funcs))

    for f in funcs:
      self.Log("  %-10s %-20s gl%s" % (f.info.type, f.return_type, f.name))

  def WriteCommandIds(self, filename):
    """Writes the command buffer format"""
    file = CHeaderWriter(filename)
    file.Write("#define GLES2_COMMAND_LIST(OP) \\\n")
    for func in self.functions:
      if not func.name in _CMD_ID_TABLE:
        self.Error("Command %s not in _CMD_ID_TABLE" % func.name)
      file.Write("  %-60s /* %d */ \\\n" %
                 ("OP(%s)" % func.name, _CMD_ID_TABLE[func.name]))
    file.Write("\n")

    file.Write("enum CommandId {\n")
    file.Write("  kStartPoint = cmd::kLastCommonId,  "
               "// All GLES2 commands start after this.\n")
    file.Write("#define GLES2_CMD_OP(name) k ## name,\n")
    file.Write("  GLES2_COMMAND_LIST(GLES2_CMD_OP)\n")
    file.Write("#undef GLES2_CMD_OP\n")
    file.Write("  kNumCommands\n")
    file.Write("};\n")
    file.Write("\n")
    file.Close()

  def WriteFormat(self, filename):
    """Writes the command buffer format"""
    file = CHeaderWriter(filename)
    for func in self.functions:
      func.WriteStruct(file)
    file.Write("\n")
    file.Close()

  def WriteFormatTest(self, filename):
    """Writes the command buffer format test."""
    file = CHeaderWriter(
      filename,
      "// This file contains unit tests for gles2 commmands\n"
      "// It is included by gles2_cmd_format_test.cc\n"
      "\n")

    for func in self.functions:
      func.WriteFormatTest(file)

    file.Close()

  def WriteCommandIdTest(self, filename):
    """Writes the command id test."""
    file = CHeaderWriter(
        filename,
        "// This file contains unit tests for gles2 commmand ids\n")

    file.Write("// *** These IDs MUST NOT CHANGE!!! ***\n")
    file.Write("// Changing them will break all client programs.\n")
    file.Write("TEST(GLES2CommandIdTest, CommandIdsMatch) {\n")
    for func in self.functions:
      if not func.name in _CMD_ID_TABLE:
        self.Error("Command %s not in _CMD_ID_TABLE" % func.name)
      file.Write("  COMPILE_ASSERT(%s::kCmdId == %d,\n" %
                 (func.name, _CMD_ID_TABLE[func.name]))
      file.Write("                 GLES2_%s_kCmdId_mismatch);\n" % func.name)

    file.Write("}\n")
    file.Write("\n")
    file.Close()

  def WriteCmdHelperHeader(self, filename):
    """Writes the gles2 command helper."""
    file = CHeaderWriter(filename)

    for func in self.functions:
      func.WriteCmdHelper(file)

    file.Close()

  def WriteServiceImplementation(self, filename):
    """Writes the service decorder implementation."""
    file = CHeaderWriter(
        filename,
        "// It is included by gles2_cmd_decoder.cc\n")

    for func in self.functions:
      func.WriteServiceImplementation(file)

    file.Close()

  def WriteServiceUnitTests(self, filename):
    """Writes the service decorder unit tests."""
    file = CHeaderWriter(
        filename,
        "// It is included by gles2_cmd_decoder_unittest.cc\n")

    for func in self.functions:
      func.WriteServiceUnitTest(file)

    file.Close()


  def WriteGLES2CLibImplementation(self, filename):
    """Writes the GLES2 c lib implementation."""
    file = CHeaderWriter(
        filename,
        "// These functions emluate GLES2 over command buffers.\n")

    for func in self.original_functions:
      file.Write("%s GLES2%s(%s) {\n" %
                 (func.return_type, func.name,
                  func.MakeTypedOriginalArgString("")))
      return_string = "return "
      if func.return_type == "void":
        return_string = ""
      file.Write("  %sgles2::GetGLContext()->%s(%s);\n" %
                 (return_string, func.original_name,
                  func.MakeOriginalArgString("")))
      file.Write("}\n")

    file.Write("\n")

    file.Close()

  def WriteGLES2ImplementationHeader(self, filename):
    """Writes the GLES2 helper header."""
    file = CHeaderWriter(
        filename,
        "// This file is included by gles2_implementation.h to declare the\n"
        "// GL api functions.\n")
    for func in self.original_functions:
      func.WriteGLES2ImplementationHeader(file)
    file.Close()

  def WriteServiceUtilsHeader(self, filename):
    """Writes the gles2 auto generated utility header."""
    file = CHeaderWriter(filename)
    for enum in sorted(_ENUM_LISTS.keys()):
      file.Write("bool Validate%s%s(GLenum value);\n" % (_ENUM_LISTS[enum]['type'], enum))
    file.Write("\n")
    file.Close()

  def WriteServiceUtilsImplementation(self, filename):
    """Writes the gles2 auto generated utility implementation."""
    file = CHeaderWriter(filename)
    for enum in sorted(_ENUM_LISTS.keys()):
      file.Write("bool Validate%s%s(GLenum value) {\n" % (_ENUM_LISTS[enum]['type'], enum))
      file.Write("  switch (value) {\n")
      for value in _ENUM_LISTS[enum]['valid']:
        file.Write("    case %s:\n" % value)
      file.Write("      return true;\n")
      file.Write("    default:\n")
      file.Write("      return false;\n")
      file.Write("  }\n")
      file.Write("}\n")
      file.Write("\n")
    file.Close()


def main(argv):
  """This is the main function."""
  parser = OptionParser()
  parser.add_option(
      "-g", "--generate-implementation-templates", action="store_true",
      help="generates files that are generally hand edited..")
  parser.add_option(
      "--generate-command-id-tests", action="store_true",
      help="generate tests for commands ids. Commands MUST not change ID!")
  parser.add_option(
      "-v", "--verbose", action="store_true",
      help="prints more output.")

  (options, args) = parser.parse_args(args=argv)

  gen = GLGenerator(options.verbose)
  gen.ParseGLH("common/GLES2/gl2.h")
  gen.WriteCommandIds("common/gles2_cmd_ids_autogen.h")
  gen.WriteFormat("common/gles2_cmd_format_autogen.h")
  gen.WriteFormatTest("common/gles2_cmd_format_test_autogen.h")
  gen.WriteGLES2ImplementationHeader("client/gles2_implementation_autogen.h")
  gen.WriteGLES2CLibImplementation("client/gles2_c_lib_autogen.h")
  gen.WriteCmdHelperHeader("client/gles2_cmd_helper_autogen.h")
  gen.WriteServiceImplementation("service/gles2_cmd_decoder_autogen.h")
  gen.WriteServiceUnitTests("service/gles2_cmd_decoder_unittest_autogen.h")
  gen.WriteServiceUtilsHeader("service/gles2_cmd_validation_autogen.h")
  gen.WriteServiceUtilsImplementation(
      "service/gles2_cmd_validation_implementation_autogen.h")

  if options.generate_command_id_tests:
    gen.WriteCommandIdTest("common/gles2_cmd_id_test_autogen.h")

  if gen.errors > 0:
    print "%d errors" % gen.errors
    sys.exit(1)

if __name__ == '__main__':
  main(sys.argv[1:])
