// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

// This file is included by gles2_implementation.h to declare the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_AUTOGEN_H_

void ActiveTexture(GLenum texture) {
  helper_->ActiveTexture(texture);
}

void AttachShader(GLuint program, GLuint shader) {
  helper_->AttachShader(program, shader);
}

void BindAttribLocation(GLuint program, GLuint index, const char* name);

void BindFramebuffer(GLenum target, GLuint framebuffer) {
  if (IsFramebufferReservedId(framebuffer)) {
    SetGLError(
        GL_INVALID_OPERATION, "BindFramebuffer: framebuffer reserved id");
    return;
  }
  framebuffer_id_handler_->MarkAsUsedForBind(framebuffer);
  helper_->BindFramebuffer(target, framebuffer);
}

void BindRenderbuffer(GLenum target, GLuint renderbuffer) {
  if (IsRenderbufferReservedId(renderbuffer)) {
    SetGLError(
        GL_INVALID_OPERATION, "BindRenderbuffer: renderbuffer reserved id");
    return;
  }
  renderbuffer_id_handler_->MarkAsUsedForBind(renderbuffer);
  helper_->BindRenderbuffer(target, renderbuffer);
}

void BindTexture(GLenum target, GLuint texture) {
  if (IsTextureReservedId(texture)) {
    SetGLError(GL_INVALID_OPERATION, "BindTexture: texture reserved id");
    return;
  }
  texture_id_handler_->MarkAsUsedForBind(texture);
  helper_->BindTexture(target, texture);
}

void BlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  helper_->BlendColor(red, green, blue, alpha);
}

void BlendEquation(GLenum mode) {
  helper_->BlendEquation(mode);
}

void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
  helper_->BlendEquationSeparate(modeRGB, modeAlpha);
}

void BlendFunc(GLenum sfactor, GLenum dfactor) {
  helper_->BlendFunc(sfactor, dfactor);
}

void BlendFuncSeparate(
    GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
  helper_->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void BufferData(
    GLenum target, GLsizeiptr size, const void* data, GLenum usage);

void BufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data);

GLenum CheckFramebufferStatus(GLenum target) {
  typedef CheckFramebufferStatus::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->CheckFramebufferStatus(
      target, result_shm_id(), result_shm_offset());
  WaitForCmd();
  return *result;
}

void Clear(GLbitfield mask) {
  helper_->Clear(mask);
}

void ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  helper_->ClearColor(red, green, blue, alpha);
}

void ClearDepthf(GLclampf depth) {
  helper_->ClearDepthf(depth);
}

void ClearStencil(GLint s) {
  helper_->ClearStencil(s);
}

void ColorMask(
    GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
  helper_->ColorMask(red, green, blue, alpha);
}

void CompileShader(GLuint shader) {
  helper_->CompileShader(shader);
}

void CompressedTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLsizei width,
    GLsizei height, GLint border, GLsizei imageSize, const void* data);

void CompressedTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei imageSize, const void* data);

void CopyTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
    GLsizei width, GLsizei height, GLint border) {
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexImage2D: width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexImage2D: height < 0");
    return;
  }
  helper_->CopyTexImage2D(
      target, level, internalformat, x, y, width, height, border);
}

void CopyTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
    GLsizei width, GLsizei height) {
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexSubImage2D: width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexSubImage2D: height < 0");
    return;
  }
  helper_->CopyTexSubImage2D(
      target, level, xoffset, yoffset, x, y, width, height);
}

GLuint CreateProgram() {
  GLuint client_id;
  program_and_shader_id_handler_->MakeIds(0, 1, &client_id);
  helper_->CreateProgram(client_id);
  return client_id;
}

GLuint CreateShader(GLenum type) {
  GLuint client_id;
  program_and_shader_id_handler_->MakeIds(0, 1, &client_id);
  helper_->CreateShader(type, client_id);
  return client_id;
}

void CullFace(GLenum mode) {
  helper_->CullFace(mode);
}

void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteFramebuffers: n < 0");
    return;
  }
framebuffer_id_handler_->FreeIds(n, framebuffers);
  helper_->DeleteFramebuffersImmediate(n, framebuffers);
}

void DeleteProgram(GLuint program) {
  program_and_shader_id_handler_->FreeIds(1, &program);
  helper_->DeleteProgram(program);
}

void DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteRenderbuffers: n < 0");
    return;
  }
renderbuffer_id_handler_->FreeIds(n, renderbuffers);
  helper_->DeleteRenderbuffersImmediate(n, renderbuffers);
}

void DeleteShader(GLuint shader) {
  program_and_shader_id_handler_->FreeIds(1, &shader);
  helper_->DeleteShader(shader);
}

void DeleteTextures(GLsizei n, const GLuint* textures) {
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteTextures: n < 0");
    return;
  }
texture_id_handler_->FreeIds(n, textures);
  helper_->DeleteTexturesImmediate(n, textures);
}

void DepthFunc(GLenum func) {
  helper_->DepthFunc(func);
}

void DepthMask(GLboolean flag) {
  helper_->DepthMask(flag);
}

void DepthRangef(GLclampf zNear, GLclampf zFar) {
  helper_->DepthRangef(zNear, zFar);
}

void DetachShader(GLuint program, GLuint shader) {
  helper_->DetachShader(program, shader);
}

void Disable(GLenum cap) {
  helper_->Disable(cap);
}

void DrawElements(
    GLenum mode, GLsizei count, GLenum type, const void* indices);

void Enable(GLenum cap) {
  helper_->Enable(cap);
}

void Finish();

void Flush();

void FramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint renderbuffer) {
  helper_->FramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
}

void FramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
    GLint level) {
  helper_->FramebufferTexture2D(target, attachment, textarget, texture, level);
}

void FrontFace(GLenum mode) {
  helper_->FrontFace(mode);
}

void GenBuffers(GLsizei n, GLuint* buffers) {
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenBuffers: n < 0");
    return;
  }
  buffer_id_handler_->MakeIds(0, n, buffers);
  helper_->GenBuffersImmediate(n, buffers);
}

void GenerateMipmap(GLenum target) {
  helper_->GenerateMipmap(target);
}

void GenFramebuffers(GLsizei n, GLuint* framebuffers) {
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenFramebuffers: n < 0");
    return;
  }
  framebuffer_id_handler_->MakeIds(0, n, framebuffers);
  helper_->GenFramebuffersImmediate(n, framebuffers);
}

void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenRenderbuffers: n < 0");
    return;
  }
  renderbuffer_id_handler_->MakeIds(0, n, renderbuffers);
  helper_->GenRenderbuffersImmediate(n, renderbuffers);
}

void GenTextures(GLsizei n, GLuint* textures) {
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenTextures: n < 0");
    return;
  }
  texture_id_handler_->MakeIds(0, n, textures);
  helper_->GenTexturesImmediate(n, textures);
}

void GetActiveAttrib(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name);

void GetActiveUniform(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name);

void GetAttachedShaders(
    GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders);

GLint GetAttribLocation(GLuint program, const char* name);

void GetBooleanv(GLenum pname, GLboolean* params) {
  typedef GetBooleanv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetBooleanv(pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
}
void GetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  typedef GetBufferParameteriv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetBufferParameteriv(target, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
}
GLenum GetError();

void GetFloatv(GLenum pname, GLfloat* params) {
  typedef GetFloatv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetFloatv(pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
}
void GetFramebufferAttachmentParameteriv(
    GLenum target, GLenum attachment, GLenum pname, GLint* params) {
  typedef GetFramebufferAttachmentParameteriv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetFramebufferAttachmentParameteriv(target, attachment, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
}
void GetIntegerv(GLenum pname, GLint* params) {
  typedef GetIntegerv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetIntegerv(pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
}
void GetProgramiv(GLuint program, GLenum pname, GLint* params) {
  typedef GetProgramiv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetProgramiv(program, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
}
void GetProgramInfoLog(
    GLuint program, GLsizei bufsize, GLsizei* length, char* infolog) {
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetProgramInfoLog(program, kResultBucketId);
  if (bufsize > 0) {
    std::string str;
    if (GetBucketAsString(kResultBucketId, &str)) {
      GLsizei max_size =
          std::min(static_cast<size_t>(bufsize) - 1, str.size());
      if (length != NULL) {
        *length = max_size;
      }
      memcpy(infolog, str.c_str(), max_size);
      infolog[max_size] = '\0';
    }
  }
}
void GetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  typedef GetRenderbufferParameteriv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetRenderbufferParameteriv(target, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
}
void GetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  typedef GetShaderiv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetShaderiv(shader, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
}
void GetShaderInfoLog(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog) {
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetShaderInfoLog(shader, kResultBucketId);
  if (bufsize > 0) {
    std::string str;
    if (GetBucketAsString(kResultBucketId, &str)) {
      GLsizei max_size =
          std::min(static_cast<size_t>(bufsize) - 1, str.size());
      if (length != NULL) {
        *length = max_size;
      }
      memcpy(infolog, str.c_str(), max_size);
      infolog[max_size] = '\0';
    }
  }
}
void GetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);

void GetShaderSource(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* source) {
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetShaderSource(shader, kResultBucketId);
  if (bufsize > 0) {
    std::string str;
    if (GetBucketAsString(kResultBucketId, &str)) {
      GLsizei max_size =
          std::min(static_cast<size_t>(bufsize) - 1, str.size());
      if (length != NULL) {
        *length = max_size;
      }
      memcpy(source, str.c_str(), max_size);
      source[max_size] = '\0';
    }
  }
}
const GLubyte* GetString(GLenum name);

void GetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
  typedef GetTexParameterfv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetTexParameterfv(target, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
}
void GetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
  typedef GetTexParameteriv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetTexParameteriv(target, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
}
void GetUniformfv(GLuint program, GLint location, GLfloat* params);

void GetUniformiv(GLuint program, GLint location, GLint* params);

GLint GetUniformLocation(GLuint program, const char* name);

void GetVertexAttribPointerv(GLuint index, GLenum pname, void** pointer);

void Hint(GLenum target, GLenum mode) {
  helper_->Hint(target, mode);
}

GLboolean IsBuffer(GLuint buffer) {
  typedef IsBuffer::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsBuffer(buffer, result_shm_id(), result_shm_offset());
  WaitForCmd();
  return *result;
}

GLboolean IsEnabled(GLenum cap) {
  typedef IsEnabled::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsEnabled(cap, result_shm_id(), result_shm_offset());
  WaitForCmd();
  return *result;
}

GLboolean IsFramebuffer(GLuint framebuffer) {
  typedef IsFramebuffer::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsFramebuffer(framebuffer, result_shm_id(), result_shm_offset());
  WaitForCmd();
  return *result;
}

GLboolean IsProgram(GLuint program) {
  typedef IsProgram::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsProgram(program, result_shm_id(), result_shm_offset());
  WaitForCmd();
  return *result;
}

GLboolean IsRenderbuffer(GLuint renderbuffer) {
  typedef IsRenderbuffer::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsRenderbuffer(renderbuffer, result_shm_id(), result_shm_offset());
  WaitForCmd();
  return *result;
}

GLboolean IsShader(GLuint shader) {
  typedef IsShader::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsShader(shader, result_shm_id(), result_shm_offset());
  WaitForCmd();
  return *result;
}

GLboolean IsTexture(GLuint texture) {
  typedef IsTexture::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsTexture(texture, result_shm_id(), result_shm_offset());
  WaitForCmd();
  return *result;
}

void LineWidth(GLfloat width) {
  helper_->LineWidth(width);
}

void LinkProgram(GLuint program) {
  helper_->LinkProgram(program);
}

void PixelStorei(GLenum pname, GLint param);

void PolygonOffset(GLfloat factor, GLfloat units) {
  helper_->PolygonOffset(factor, units);
}

void ReadPixels(
    GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
    void* pixels);

void ReleaseShaderCompiler() {
  helper_->ReleaseShaderCompiler();
}

void RenderbufferStorage(
    GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorage: width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorage: height < 0");
    return;
  }
  helper_->RenderbufferStorage(target, internalformat, width, height);
}

void SampleCoverage(GLclampf value, GLboolean invert) {
  helper_->SampleCoverage(value, invert);
}

void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glScissor: width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glScissor: height < 0");
    return;
  }
  helper_->Scissor(x, y, width, height);
}

void ShaderBinary(
    GLsizei n, const GLuint* shaders, GLenum binaryformat, const void* binary,
    GLsizei length);

void ShaderSource(
    GLuint shader, GLsizei count, const char** str, const GLint* length);

void StencilFunc(GLenum func, GLint ref, GLuint mask) {
  helper_->StencilFunc(func, ref, mask);
}

void StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
  helper_->StencilFuncSeparate(face, func, ref, mask);
}

void StencilMask(GLuint mask) {
  helper_->StencilMask(mask);
}

void StencilMaskSeparate(GLenum face, GLuint mask) {
  helper_->StencilMaskSeparate(face, mask);
}

void StencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  helper_->StencilOp(fail, zfail, zpass);
}

void StencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass) {
  helper_->StencilOpSeparate(face, fail, zfail, zpass);
}

void TexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels);

void TexParameterf(GLenum target, GLenum pname, GLfloat param) {
  helper_->TexParameterf(target, pname, param);
}

void TexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
  helper_->TexParameterfvImmediate(target, pname, params);
}

void TexParameteri(GLenum target, GLenum pname, GLint param) {
  helper_->TexParameteri(target, pname, param);
}

void TexParameteriv(GLenum target, GLenum pname, const GLint* params) {
  helper_->TexParameterivImmediate(target, pname, params);
}

void TexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels);

void Uniform1f(GLint location, GLfloat x) {
  helper_->Uniform1f(location, x);
}

void Uniform1fv(GLint location, GLsizei count, const GLfloat* v) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform1fv: count < 0");
    return;
  }
  helper_->Uniform1fvImmediate(location, count, v);
}

void Uniform1i(GLint location, GLint x) {
  helper_->Uniform1i(location, x);
}

void Uniform1iv(GLint location, GLsizei count, const GLint* v) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform1iv: count < 0");
    return;
  }
  helper_->Uniform1ivImmediate(location, count, v);
}

void Uniform2f(GLint location, GLfloat x, GLfloat y) {
  helper_->Uniform2f(location, x, y);
}

void Uniform2fv(GLint location, GLsizei count, const GLfloat* v) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform2fv: count < 0");
    return;
  }
  helper_->Uniform2fvImmediate(location, count, v);
}

void Uniform2i(GLint location, GLint x, GLint y) {
  helper_->Uniform2i(location, x, y);
}

void Uniform2iv(GLint location, GLsizei count, const GLint* v) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform2iv: count < 0");
    return;
  }
  helper_->Uniform2ivImmediate(location, count, v);
}

void Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  helper_->Uniform3f(location, x, y, z);
}

void Uniform3fv(GLint location, GLsizei count, const GLfloat* v) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform3fv: count < 0");
    return;
  }
  helper_->Uniform3fvImmediate(location, count, v);
}

void Uniform3i(GLint location, GLint x, GLint y, GLint z) {
  helper_->Uniform3i(location, x, y, z);
}

void Uniform3iv(GLint location, GLsizei count, const GLint* v) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform3iv: count < 0");
    return;
  }
  helper_->Uniform3ivImmediate(location, count, v);
}

void Uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  helper_->Uniform4f(location, x, y, z, w);
}

void Uniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform4fv: count < 0");
    return;
  }
  helper_->Uniform4fvImmediate(location, count, v);
}

void Uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
  helper_->Uniform4i(location, x, y, z, w);
}

void Uniform4iv(GLint location, GLsizei count, const GLint* v) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform4iv: count < 0");
    return;
  }
  helper_->Uniform4ivImmediate(location, count, v);
}

void UniformMatrix2fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix2fv: count < 0");
    return;
  }
  helper_->UniformMatrix2fvImmediate(location, count, transpose, value);
}

void UniformMatrix3fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix3fv: count < 0");
    return;
  }
  helper_->UniformMatrix3fvImmediate(location, count, transpose, value);
}

void UniformMatrix4fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix4fv: count < 0");
    return;
  }
  helper_->UniformMatrix4fvImmediate(location, count, transpose, value);
}

void UseProgram(GLuint program) {
  helper_->UseProgram(program);
}

void ValidateProgram(GLuint program) {
  helper_->ValidateProgram(program);
}

void VertexAttrib1f(GLuint indx, GLfloat x) {
  helper_->VertexAttrib1f(indx, x);
}

void VertexAttrib1fv(GLuint indx, const GLfloat* values) {
  helper_->VertexAttrib1fvImmediate(indx, values);
}

void VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
  helper_->VertexAttrib2f(indx, x, y);
}

void VertexAttrib2fv(GLuint indx, const GLfloat* values) {
  helper_->VertexAttrib2fvImmediate(indx, values);
}

void VertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  helper_->VertexAttrib3f(indx, x, y, z);
}

void VertexAttrib3fv(GLuint indx, const GLfloat* values) {
  helper_->VertexAttrib3fvImmediate(indx, values);
}

void VertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  helper_->VertexAttrib4f(indx, x, y, z, w);
}

void VertexAttrib4fv(GLuint indx, const GLfloat* values) {
  helper_->VertexAttrib4fvImmediate(indx, values);
}

void VertexAttribPointer(
    GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void* ptr);

void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glViewport: width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glViewport: height < 0");
    return;
  }
  helper_->Viewport(x, y, width, height);
}

void BlitFramebufferEXT(
    GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
    GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
  helper_->BlitFramebufferEXT(
      srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void RenderbufferStorageMultisampleEXT(
    GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
    GLsizei height) {
  if (samples < 0) {
    SetGLError(
        GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT: samples < 0");
    return;
  }
  if (width < 0) {
    SetGLError(
        GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT: width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(
        GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT: height < 0");
    return;
  }
  helper_->RenderbufferStorageMultisampleEXT(
      target, samples, internalformat, width, height);
}

void SwapBuffers();

GLuint GetMaxValueInBufferCHROMIUM(
    GLuint buffer_id, GLsizei count, GLenum type, GLuint offset) {
  typedef GetMaxValueInBufferCHROMIUM::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->GetMaxValueInBufferCHROMIUM(
      buffer_id, count, type, offset, result_shm_id(), result_shm_offset());
  WaitForCmd();
  return *result;
}

void GenSharedIdsCHROMIUM(
    GLuint namespace_id, GLuint id_offset, GLsizei n, GLuint* ids);

void DeleteSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids);

void RegisterSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids);

GLboolean CommandBufferEnableCHROMIUM(const char* feature);

void* MapBufferSubDataCHROMIUM(
    GLuint target, GLintptr offset, GLsizeiptr size, GLenum access);

void UnmapBufferSubDataCHROMIUM(const void* mem);

void* MapTexSubImage2DCHROMIUM(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, GLenum access);

void UnmapTexSubImage2DCHROMIUM(const void* mem);

void CopyTextureToParentTextureCHROMIUM(
    GLuint client_child_id, GLuint client_parent_id);

void ResizeCHROMIUM(GLuint width, GLuint height) {
  helper_->ResizeCHROMIUM(width, height);
}

const GLchar* GetRequestableExtensionsCHROMIUM();

void RequestExtensionCHROMIUM(const char* extension);

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_AUTOGEN_H_

