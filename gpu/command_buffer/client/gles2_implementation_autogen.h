// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by gles2_implementation.h to declare the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_AUTOGEN_H_

virtual void ActiveTexture(GLenum texture) override;

virtual void AttachShader(GLuint program, GLuint shader) override;

virtual void BindAttribLocation(GLuint program,
                                GLuint index,
                                const char* name) override;

virtual void BindBuffer(GLenum target, GLuint buffer) override;

virtual void BindFramebuffer(GLenum target, GLuint framebuffer) override;

virtual void BindRenderbuffer(GLenum target, GLuint renderbuffer) override;

virtual void BindTexture(GLenum target, GLuint texture) override;

virtual void BlendColor(GLclampf red,
                        GLclampf green,
                        GLclampf blue,
                        GLclampf alpha) override;

virtual void BlendEquation(GLenum mode) override;

virtual void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) override;

virtual void BlendFunc(GLenum sfactor, GLenum dfactor) override;

virtual void BlendFuncSeparate(GLenum srcRGB,
                               GLenum dstRGB,
                               GLenum srcAlpha,
                               GLenum dstAlpha) override;

virtual void BufferData(GLenum target,
                        GLsizeiptr size,
                        const void* data,
                        GLenum usage) override;

virtual void BufferSubData(GLenum target,
                           GLintptr offset,
                           GLsizeiptr size,
                           const void* data) override;

virtual GLenum CheckFramebufferStatus(GLenum target) override;

virtual void Clear(GLbitfield mask) override;

virtual void ClearColor(GLclampf red,
                        GLclampf green,
                        GLclampf blue,
                        GLclampf alpha) override;

virtual void ClearDepthf(GLclampf depth) override;

virtual void ClearStencil(GLint s) override;

virtual void ColorMask(GLboolean red,
                       GLboolean green,
                       GLboolean blue,
                       GLboolean alpha) override;

virtual void CompileShader(GLuint shader) override;

virtual void CompressedTexImage2D(GLenum target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height,
                                  GLint border,
                                  GLsizei imageSize,
                                  const void* data) override;

virtual void CompressedTexSubImage2D(GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLenum format,
                                     GLsizei imageSize,
                                     const void* data) override;

virtual void CopyTexImage2D(GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height,
                            GLint border) override;

virtual void CopyTexSubImage2D(GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height) override;

virtual GLuint CreateProgram() override;

virtual GLuint CreateShader(GLenum type) override;

virtual void CullFace(GLenum mode) override;

virtual void DeleteBuffers(GLsizei n, const GLuint* buffers) override;

virtual void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) override;

virtual void DeleteProgram(GLuint program) override;

virtual void DeleteRenderbuffers(GLsizei n,
                                 const GLuint* renderbuffers) override;

virtual void DeleteShader(GLuint shader) override;

virtual void DeleteTextures(GLsizei n, const GLuint* textures) override;

virtual void DepthFunc(GLenum func) override;

virtual void DepthMask(GLboolean flag) override;

virtual void DepthRangef(GLclampf zNear, GLclampf zFar) override;

virtual void DetachShader(GLuint program, GLuint shader) override;

virtual void Disable(GLenum cap) override;

virtual void DrawArrays(GLenum mode, GLint first, GLsizei count) override;

virtual void DrawElements(GLenum mode,
                          GLsizei count,
                          GLenum type,
                          const void* indices) override;

virtual void Enable(GLenum cap) override;

virtual void Finish() override;

virtual void Flush() override;

virtual void FramebufferRenderbuffer(GLenum target,
                                     GLenum attachment,
                                     GLenum renderbuffertarget,
                                     GLuint renderbuffer) override;

virtual void FramebufferTexture2D(GLenum target,
                                  GLenum attachment,
                                  GLenum textarget,
                                  GLuint texture,
                                  GLint level) override;

virtual void FrontFace(GLenum mode) override;

virtual void GenBuffers(GLsizei n, GLuint* buffers) override;

virtual void GenerateMipmap(GLenum target) override;

virtual void GenFramebuffers(GLsizei n, GLuint* framebuffers) override;

virtual void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override;

virtual void GenTextures(GLsizei n, GLuint* textures) override;

virtual void GetActiveAttrib(GLuint program,
                             GLuint index,
                             GLsizei bufsize,
                             GLsizei* length,
                             GLint* size,
                             GLenum* type,
                             char* name) override;

virtual void GetActiveUniform(GLuint program,
                              GLuint index,
                              GLsizei bufsize,
                              GLsizei* length,
                              GLint* size,
                              GLenum* type,
                              char* name) override;

virtual void GetAttachedShaders(GLuint program,
                                GLsizei maxcount,
                                GLsizei* count,
                                GLuint* shaders) override;

virtual GLint GetAttribLocation(GLuint program, const char* name) override;

virtual void GetBooleanv(GLenum pname, GLboolean* params) override;

virtual void GetBufferParameteriv(GLenum target,
                                  GLenum pname,
                                  GLint* params) override;

virtual GLenum GetError() override;

virtual void GetFloatv(GLenum pname, GLfloat* params) override;

virtual void GetFramebufferAttachmentParameteriv(GLenum target,
                                                 GLenum attachment,
                                                 GLenum pname,
                                                 GLint* params) override;

virtual void GetIntegerv(GLenum pname, GLint* params) override;

virtual void GetProgramiv(GLuint program, GLenum pname, GLint* params) override;

virtual void GetProgramInfoLog(GLuint program,
                               GLsizei bufsize,
                               GLsizei* length,
                               char* infolog) override;

virtual void GetRenderbufferParameteriv(GLenum target,
                                        GLenum pname,
                                        GLint* params) override;

virtual void GetShaderiv(GLuint shader, GLenum pname, GLint* params) override;

virtual void GetShaderInfoLog(GLuint shader,
                              GLsizei bufsize,
                              GLsizei* length,
                              char* infolog) override;

virtual void GetShaderPrecisionFormat(GLenum shadertype,
                                      GLenum precisiontype,
                                      GLint* range,
                                      GLint* precision) override;

virtual void GetShaderSource(GLuint shader,
                             GLsizei bufsize,
                             GLsizei* length,
                             char* source) override;

virtual const GLubyte* GetString(GLenum name) override;

virtual void GetTexParameterfv(GLenum target,
                               GLenum pname,
                               GLfloat* params) override;

virtual void GetTexParameteriv(GLenum target,
                               GLenum pname,
                               GLint* params) override;

virtual void GetUniformfv(GLuint program,
                          GLint location,
                          GLfloat* params) override;

virtual void GetUniformiv(GLuint program,
                          GLint location,
                          GLint* params) override;

virtual GLint GetUniformLocation(GLuint program, const char* name) override;

virtual void GetVertexAttribPointerv(GLuint index,
                                     GLenum pname,
                                     void** pointer) override;

virtual void Hint(GLenum target, GLenum mode) override;

virtual GLboolean IsBuffer(GLuint buffer) override;

virtual GLboolean IsEnabled(GLenum cap) override;

virtual GLboolean IsFramebuffer(GLuint framebuffer) override;

virtual GLboolean IsProgram(GLuint program) override;

virtual GLboolean IsRenderbuffer(GLuint renderbuffer) override;

virtual GLboolean IsShader(GLuint shader) override;

virtual GLboolean IsTexture(GLuint texture) override;

virtual void LineWidth(GLfloat width) override;

virtual void LinkProgram(GLuint program) override;

virtual void PixelStorei(GLenum pname, GLint param) override;

virtual void PolygonOffset(GLfloat factor, GLfloat units) override;

virtual void ReadPixels(GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLenum format,
                        GLenum type,
                        void* pixels) override;

virtual void ReleaseShaderCompiler() override;

virtual void RenderbufferStorage(GLenum target,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height) override;

virtual void SampleCoverage(GLclampf value, GLboolean invert) override;

virtual void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) override;

virtual void ShaderBinary(GLsizei n,
                          const GLuint* shaders,
                          GLenum binaryformat,
                          const void* binary,
                          GLsizei length) override;

virtual void ShaderSource(GLuint shader,
                          GLsizei count,
                          const GLchar* const* str,
                          const GLint* length) override;

virtual void ShallowFinishCHROMIUM() override;

virtual void ShallowFlushCHROMIUM() override;

virtual void StencilFunc(GLenum func, GLint ref, GLuint mask) override;

virtual void StencilFuncSeparate(GLenum face,
                                 GLenum func,
                                 GLint ref,
                                 GLuint mask) override;

virtual void StencilMask(GLuint mask) override;

virtual void StencilMaskSeparate(GLenum face, GLuint mask) override;

virtual void StencilOp(GLenum fail, GLenum zfail, GLenum zpass) override;

virtual void StencilOpSeparate(GLenum face,
                               GLenum fail,
                               GLenum zfail,
                               GLenum zpass) override;

virtual void TexImage2D(GLenum target,
                        GLint level,
                        GLint internalformat,
                        GLsizei width,
                        GLsizei height,
                        GLint border,
                        GLenum format,
                        GLenum type,
                        const void* pixels) override;

virtual void TexParameterf(GLenum target, GLenum pname, GLfloat param) override;

virtual void TexParameterfv(GLenum target,
                            GLenum pname,
                            const GLfloat* params) override;

virtual void TexParameteri(GLenum target, GLenum pname, GLint param) override;

virtual void TexParameteriv(GLenum target,
                            GLenum pname,
                            const GLint* params) override;

virtual void TexSubImage2D(GLenum target,
                           GLint level,
                           GLint xoffset,
                           GLint yoffset,
                           GLsizei width,
                           GLsizei height,
                           GLenum format,
                           GLenum type,
                           const void* pixels) override;

virtual void Uniform1f(GLint location, GLfloat x) override;

virtual void Uniform1fv(GLint location,
                        GLsizei count,
                        const GLfloat* v) override;

virtual void Uniform1i(GLint location, GLint x) override;

virtual void Uniform1iv(GLint location, GLsizei count, const GLint* v) override;

virtual void Uniform2f(GLint location, GLfloat x, GLfloat y) override;

virtual void Uniform2fv(GLint location,
                        GLsizei count,
                        const GLfloat* v) override;

virtual void Uniform2i(GLint location, GLint x, GLint y) override;

virtual void Uniform2iv(GLint location, GLsizei count, const GLint* v) override;

virtual void Uniform3f(GLint location,
                       GLfloat x,
                       GLfloat y,
                       GLfloat z) override;

virtual void Uniform3fv(GLint location,
                        GLsizei count,
                        const GLfloat* v) override;

virtual void Uniform3i(GLint location, GLint x, GLint y, GLint z) override;

virtual void Uniform3iv(GLint location, GLsizei count, const GLint* v) override;

virtual void Uniform4f(GLint location,
                       GLfloat x,
                       GLfloat y,
                       GLfloat z,
                       GLfloat w) override;

virtual void Uniform4fv(GLint location,
                        GLsizei count,
                        const GLfloat* v) override;

virtual void Uniform4i(GLint location,
                       GLint x,
                       GLint y,
                       GLint z,
                       GLint w) override;

virtual void Uniform4iv(GLint location, GLsizei count, const GLint* v) override;

virtual void UniformMatrix2fv(GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat* value) override;

virtual void UniformMatrix3fv(GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat* value) override;

virtual void UniformMatrix4fv(GLint location,
                              GLsizei count,
                              GLboolean transpose,
                              const GLfloat* value) override;

virtual void UseProgram(GLuint program) override;

virtual void ValidateProgram(GLuint program) override;

virtual void VertexAttrib1f(GLuint indx, GLfloat x) override;

virtual void VertexAttrib1fv(GLuint indx, const GLfloat* values) override;

virtual void VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) override;

virtual void VertexAttrib2fv(GLuint indx, const GLfloat* values) override;

virtual void VertexAttrib3f(GLuint indx,
                            GLfloat x,
                            GLfloat y,
                            GLfloat z) override;

virtual void VertexAttrib3fv(GLuint indx, const GLfloat* values) override;

virtual void VertexAttrib4f(GLuint indx,
                            GLfloat x,
                            GLfloat y,
                            GLfloat z,
                            GLfloat w) override;

virtual void VertexAttrib4fv(GLuint indx, const GLfloat* values) override;

virtual void VertexAttribPointer(GLuint indx,
                                 GLint size,
                                 GLenum type,
                                 GLboolean normalized,
                                 GLsizei stride,
                                 const void* ptr) override;

virtual void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) override;

virtual void BlitFramebufferCHROMIUM(GLint srcX0,
                                     GLint srcY0,
                                     GLint srcX1,
                                     GLint srcY1,
                                     GLint dstX0,
                                     GLint dstY0,
                                     GLint dstX1,
                                     GLint dstY1,
                                     GLbitfield mask,
                                     GLenum filter) override;

virtual void RenderbufferStorageMultisampleCHROMIUM(GLenum target,
                                                    GLsizei samples,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height) override;

virtual void RenderbufferStorageMultisampleEXT(GLenum target,
                                               GLsizei samples,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height) override;

virtual void FramebufferTexture2DMultisampleEXT(GLenum target,
                                                GLenum attachment,
                                                GLenum textarget,
                                                GLuint texture,
                                                GLint level,
                                                GLsizei samples) override;

virtual void TexStorage2DEXT(GLenum target,
                             GLsizei levels,
                             GLenum internalFormat,
                             GLsizei width,
                             GLsizei height) override;

virtual void GenQueriesEXT(GLsizei n, GLuint* queries) override;

virtual void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override;

virtual GLboolean IsQueryEXT(GLuint id) override;

virtual void BeginQueryEXT(GLenum target, GLuint id) override;

virtual void EndQueryEXT(GLenum target) override;

virtual void GetQueryivEXT(GLenum target, GLenum pname, GLint* params) override;

virtual void GetQueryObjectuivEXT(GLuint id,
                                  GLenum pname,
                                  GLuint* params) override;

virtual void InsertEventMarkerEXT(GLsizei length,
                                  const GLchar* marker) override;

virtual void PushGroupMarkerEXT(GLsizei length, const GLchar* marker) override;

virtual void PopGroupMarkerEXT() override;

virtual void GenVertexArraysOES(GLsizei n, GLuint* arrays) override;

virtual void DeleteVertexArraysOES(GLsizei n, const GLuint* arrays) override;

virtual GLboolean IsVertexArrayOES(GLuint array) override;

virtual void BindVertexArrayOES(GLuint array) override;

virtual void SwapBuffers() override;

virtual GLuint GetMaxValueInBufferCHROMIUM(GLuint buffer_id,
                                           GLsizei count,
                                           GLenum type,
                                           GLuint offset) override;

virtual GLboolean EnableFeatureCHROMIUM(const char* feature) override;

virtual void* MapBufferCHROMIUM(GLuint target, GLenum access) override;

virtual GLboolean UnmapBufferCHROMIUM(GLuint target) override;

virtual void* MapBufferSubDataCHROMIUM(GLuint target,
                                       GLintptr offset,
                                       GLsizeiptr size,
                                       GLenum access) override;

virtual void UnmapBufferSubDataCHROMIUM(const void* mem) override;

virtual void* MapTexSubImage2DCHROMIUM(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLenum format,
                                       GLenum type,
                                       GLenum access) override;

virtual void UnmapTexSubImage2DCHROMIUM(const void* mem) override;

virtual void ResizeCHROMIUM(GLuint width,
                            GLuint height,
                            GLfloat scale_factor) override;

virtual const GLchar* GetRequestableExtensionsCHROMIUM() override;

virtual void RequestExtensionCHROMIUM(const char* extension) override;

virtual void RateLimitOffscreenContextCHROMIUM() override;

virtual void GetMultipleIntegervCHROMIUM(const GLenum* pnames,
                                         GLuint count,
                                         GLint* results,
                                         GLsizeiptr size) override;

virtual void GetProgramInfoCHROMIUM(GLuint program,
                                    GLsizei bufsize,
                                    GLsizei* size,
                                    void* info) override;

virtual GLuint CreateStreamTextureCHROMIUM(GLuint texture) override;

virtual GLuint CreateImageCHROMIUM(ClientBuffer buffer,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum internalformat) override;

virtual void DestroyImageCHROMIUM(GLuint image_id) override;

virtual GLuint CreateGpuMemoryBufferImageCHROMIUM(GLsizei width,
                                                  GLsizei height,
                                                  GLenum internalformat,
                                                  GLenum usage) override;

virtual void GetTranslatedShaderSourceANGLE(GLuint shader,
                                            GLsizei bufsize,
                                            GLsizei* length,
                                            char* source) override;

virtual void PostSubBufferCHROMIUM(GLint x,
                                   GLint y,
                                   GLint width,
                                   GLint height) override;

virtual void TexImageIOSurface2DCHROMIUM(GLenum target,
                                         GLsizei width,
                                         GLsizei height,
                                         GLuint ioSurfaceId,
                                         GLuint plane) override;

virtual void CopyTextureCHROMIUM(GLenum target,
                                 GLenum source_id,
                                 GLenum dest_id,
                                 GLint level,
                                 GLint internalformat,
                                 GLenum dest_type) override;

virtual void DrawArraysInstancedANGLE(GLenum mode,
                                      GLint first,
                                      GLsizei count,
                                      GLsizei primcount) override;

virtual void DrawElementsInstancedANGLE(GLenum mode,
                                        GLsizei count,
                                        GLenum type,
                                        const void* indices,
                                        GLsizei primcount) override;

virtual void VertexAttribDivisorANGLE(GLuint index, GLuint divisor) override;

virtual void GenMailboxCHROMIUM(GLbyte* mailbox) override;

virtual void ProduceTextureCHROMIUM(GLenum target,
                                    const GLbyte* mailbox) override;

virtual void ProduceTextureDirectCHROMIUM(GLuint texture,
                                          GLenum target,
                                          const GLbyte* mailbox) override;

virtual void ConsumeTextureCHROMIUM(GLenum target,
                                    const GLbyte* mailbox) override;

virtual GLuint CreateAndConsumeTextureCHROMIUM(GLenum target,
                                               const GLbyte* mailbox) override;

virtual void BindUniformLocationCHROMIUM(GLuint program,
                                         GLint location,
                                         const char* name) override;

virtual void BindTexImage2DCHROMIUM(GLenum target, GLint imageId) override;

virtual void ReleaseTexImage2DCHROMIUM(GLenum target, GLint imageId) override;

virtual void TraceBeginCHROMIUM(const char* name) override;

virtual void TraceEndCHROMIUM() override;

virtual void AsyncTexSubImage2DCHROMIUM(GLenum target,
                                        GLint level,
                                        GLint xoffset,
                                        GLint yoffset,
                                        GLsizei width,
                                        GLsizei height,
                                        GLenum format,
                                        GLenum type,
                                        const void* data) override;

virtual void AsyncTexImage2DCHROMIUM(GLenum target,
                                     GLint level,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLint border,
                                     GLenum format,
                                     GLenum type,
                                     const void* pixels) override;

virtual void WaitAsyncTexImage2DCHROMIUM(GLenum target) override;

virtual void WaitAllAsyncTexImage2DCHROMIUM() override;

virtual void DiscardFramebufferEXT(GLenum target,
                                   GLsizei count,
                                   const GLenum* attachments) override;

virtual void LoseContextCHROMIUM(GLenum current, GLenum other) override;

virtual GLuint InsertSyncPointCHROMIUM() override;

virtual void WaitSyncPointCHROMIUM(GLuint sync_point) override;

virtual void DrawBuffersEXT(GLsizei count, const GLenum* bufs) override;

virtual void DiscardBackbufferCHROMIUM() override;

virtual void ScheduleOverlayPlaneCHROMIUM(GLint plane_z_order,
                                          GLenum plane_transform,
                                          GLuint overlay_texture_id,
                                          GLint bounds_x,
                                          GLint bounds_y,
                                          GLint bounds_width,
                                          GLint bounds_height,
                                          GLfloat uv_x,
                                          GLfloat uv_y,
                                          GLfloat uv_width,
                                          GLfloat uv_height) override;

virtual void MatrixLoadfCHROMIUM(GLenum matrixMode, const GLfloat* m) override;

virtual void MatrixLoadIdentityCHROMIUM(GLenum matrixMode) override;

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_AUTOGEN_H_
