// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

// This file is included by gles2_implementation.h to declare the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_AUTOGEN_H_

void ActiveTexture(GLenum texture);

void AttachShader(GLuint program, GLuint shader) {
  GPU_CLIENT_LOG("[" << this << "] glAttachShader(" << program << ", " << shader << ")");  // NOLINT
  helper_->AttachShader(program, shader);
}

void BindAttribLocation(GLuint program, GLuint index, const char* name);

void BindBuffer(GLenum target, GLuint buffer) {
  GPU_CLIENT_LOG("[" << this << "] glBindBuffer(" << GLES2Util::GetStringBufferTarget(target) << ", " << buffer << ")");  // NOLINT
  if (IsBufferReservedId(buffer)) {
    SetGLError(GL_INVALID_OPERATION, "BindBuffer: buffer reserved id");
    return;
  }
  BindBufferHelper(target, buffer);
  helper_->BindBuffer(target, buffer);
}

void BindFramebuffer(GLenum target, GLuint framebuffer) {
  GPU_CLIENT_LOG("[" << this << "] glBindFramebuffer(" << GLES2Util::GetStringFrameBufferTarget(target) << ", " << framebuffer << ")");  // NOLINT
  if (IsFramebufferReservedId(framebuffer)) {
    SetGLError(
        GL_INVALID_OPERATION, "BindFramebuffer: framebuffer reserved id");
    return;
  }
  BindFramebufferHelper(target, framebuffer);
  helper_->BindFramebuffer(target, framebuffer);
}

void BindRenderbuffer(GLenum target, GLuint renderbuffer) {
  GPU_CLIENT_LOG("[" << this << "] glBindRenderbuffer(" << GLES2Util::GetStringRenderBufferTarget(target) << ", " << renderbuffer << ")");  // NOLINT
  if (IsRenderbufferReservedId(renderbuffer)) {
    SetGLError(
        GL_INVALID_OPERATION, "BindRenderbuffer: renderbuffer reserved id");
    return;
  }
  BindRenderbufferHelper(target, renderbuffer);
  helper_->BindRenderbuffer(target, renderbuffer);
}

void BindTexture(GLenum target, GLuint texture) {
  GPU_CLIENT_LOG("[" << this << "] glBindTexture(" << GLES2Util::GetStringTextureBindTarget(target) << ", " << texture << ")");  // NOLINT
  if (IsTextureReservedId(texture)) {
    SetGLError(GL_INVALID_OPERATION, "BindTexture: texture reserved id");
    return;
  }
  BindTextureHelper(target, texture);
  helper_->BindTexture(target, texture);
}

void BlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  GPU_CLIENT_LOG("[" << this << "] glBlendColor(" << red << ", " << green << ", " << blue << ", " << alpha << ")");  // NOLINT
  helper_->BlendColor(red, green, blue, alpha);
}

void BlendEquation(GLenum mode) {
  GPU_CLIENT_LOG("[" << this << "] glBlendEquation(" << GLES2Util::GetStringEquation(mode) << ")");  // NOLINT
  helper_->BlendEquation(mode);
}

void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
  GPU_CLIENT_LOG("[" << this << "] glBlendEquationSeparate(" << GLES2Util::GetStringEquation(modeRGB) << ", " << GLES2Util::GetStringEquation(modeAlpha) << ")");  // NOLINT
  helper_->BlendEquationSeparate(modeRGB, modeAlpha);
}

void BlendFunc(GLenum sfactor, GLenum dfactor) {
  GPU_CLIENT_LOG("[" << this << "] glBlendFunc(" << GLES2Util::GetStringSrcBlendFactor(sfactor) << ", " << GLES2Util::GetStringDstBlendFactor(dfactor) << ")");  // NOLINT
  helper_->BlendFunc(sfactor, dfactor);
}

void BlendFuncSeparate(
    GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
  GPU_CLIENT_LOG("[" << this << "] glBlendFuncSeparate(" << GLES2Util::GetStringSrcBlendFactor(srcRGB) << ", " << GLES2Util::GetStringDstBlendFactor(dstRGB) << ", " << GLES2Util::GetStringSrcBlendFactor(srcAlpha) << ", " << GLES2Util::GetStringDstBlendFactor(dstAlpha) << ")");  // NOLINT
  helper_->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void BufferData(
    GLenum target, GLsizeiptr size, const void* data, GLenum usage);

void BufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data);

GLenum CheckFramebufferStatus(GLenum target) {
  GPU_CLIENT_LOG("[" << this << "] glCheckFramebufferStatus(" << GLES2Util::GetStringFrameBufferTarget(target) << ")");  // NOLINT
  typedef CheckFramebufferStatus::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->CheckFramebufferStatus(
      target, result_shm_id(), result_shm_offset());
  WaitForCmd();
  GPU_CLIENT_LOG("returned " << *result);
  return *result;
}

void Clear(GLbitfield mask) {
  GPU_CLIENT_LOG("[" << this << "] glClear(" << mask << ")");
  helper_->Clear(mask);
}

void ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  GPU_CLIENT_LOG("[" << this << "] glClearColor(" << red << ", " << green << ", " << blue << ", " << alpha << ")");  // NOLINT
  helper_->ClearColor(red, green, blue, alpha);
}

void ClearDepthf(GLclampf depth) {
  GPU_CLIENT_LOG("[" << this << "] glClearDepthf(" << depth << ")");
  helper_->ClearDepthf(depth);
}

void ClearStencil(GLint s) {
  GPU_CLIENT_LOG("[" << this << "] glClearStencil(" << s << ")");
  helper_->ClearStencil(s);
}

void ColorMask(
    GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
  GPU_CLIENT_LOG("[" << this << "] glColorMask(" << GLES2Util::GetStringBool(
      red) << ", " << GLES2Util::GetStringBool(
          green) << ", " << GLES2Util::GetStringBool(
              blue) << ", " << GLES2Util::GetStringBool(alpha) << ")");
  helper_->ColorMask(red, green, blue, alpha);
}

void CompileShader(GLuint shader) {
  GPU_CLIENT_LOG("[" << this << "] glCompileShader(" << shader << ")");
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
  GPU_CLIENT_LOG("[" << this << "] glCopyTexImage2D(" << GLES2Util::GetStringTextureTarget(target) << ", " << level << ", " << GLES2Util::GetStringTextureInternalFormat(internalformat) << ", " << x << ", " << y << ", " << width << ", " << height << ", " << border << ")");  // NOLINT
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
  GPU_CLIENT_LOG("[" << this << "] glCopyTexSubImage2D(" << GLES2Util::GetStringTextureTarget(target) << ", " << level << ", " << xoffset << ", " << yoffset << ", " << x << ", " << y << ", " << width << ", " << height << ")");  // NOLINT
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
  GPU_CLIENT_LOG("[" << this << "] glCreateProgram(" << ")");
  GLuint client_id;
  id_handlers_[id_namespaces::kProgramsAndShaders]->
      MakeIds(0, 1, &client_id);
  helper_->CreateProgram(client_id);
  GPU_CLIENT_LOG("returned " << client_id);
  return client_id;
}

GLuint CreateShader(GLenum type) {
  GPU_CLIENT_LOG("[" << this << "] glCreateShader(" << GLES2Util::GetStringShaderType(type) << ")");  // NOLINT
  GLuint client_id;
  id_handlers_[id_namespaces::kProgramsAndShaders]->
      MakeIds(0, 1, &client_id);
  helper_->CreateShader(type, client_id);
  GPU_CLIENT_LOG("returned " << client_id);
  return client_id;
}

void CullFace(GLenum mode) {
  GPU_CLIENT_LOG("[" << this << "] glCullFace(" << GLES2Util::GetStringFaceType(
      mode) << ")");
  helper_->CullFace(mode);
}

void DeleteBuffers(GLsizei n, const GLuint* buffers) {
  GPU_CLIENT_LOG("[" << this << "] glDeleteBuffers(" << n << ", " << static_cast<const void*>(buffers) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << buffers[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_DCHECK(buffers[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteBuffers: n < 0");
    return;
  }
  DeleteBuffersHelper(n, buffers);
}

void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  GPU_CLIENT_LOG("[" << this << "] glDeleteFramebuffers(" << n << ", " << static_cast<const void*>(framebuffers) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << framebuffers[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_DCHECK(framebuffers[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteFramebuffers: n < 0");
    return;
  }
  DeleteFramebuffersHelper(n, framebuffers);
}

void DeleteProgram(GLuint program) {
  GPU_CLIENT_LOG("[" << this << "] glDeleteProgram(" << program << ")");
  GPU_CLIENT_DCHECK(program != 0);
  DeleteProgramHelper(program);
}

void DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  GPU_CLIENT_LOG("[" << this << "] glDeleteRenderbuffers(" << n << ", " << static_cast<const void*>(renderbuffers) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << renderbuffers[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_DCHECK(renderbuffers[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteRenderbuffers: n < 0");
    return;
  }
  DeleteRenderbuffersHelper(n, renderbuffers);
}

void DeleteShader(GLuint shader) {
  GPU_CLIENT_LOG("[" << this << "] glDeleteShader(" << shader << ")");
  GPU_CLIENT_DCHECK(shader != 0);
  DeleteShaderHelper(shader);
}

void DeleteTextures(GLsizei n, const GLuint* textures) {
  GPU_CLIENT_LOG("[" << this << "] glDeleteTextures(" << n << ", " << static_cast<const void*>(textures) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << textures[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_DCHECK(textures[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteTextures: n < 0");
    return;
  }
  DeleteTexturesHelper(n, textures);
}

void DepthFunc(GLenum func) {
  GPU_CLIENT_LOG("[" << this << "] glDepthFunc(" << GLES2Util::GetStringCmpFunction(func) << ")");  // NOLINT
  helper_->DepthFunc(func);
}

void DepthMask(GLboolean flag) {
  GPU_CLIENT_LOG("[" << this << "] glDepthMask(" << GLES2Util::GetStringBool(
      flag) << ")");
  helper_->DepthMask(flag);
}

void DepthRangef(GLclampf zNear, GLclampf zFar) {
  GPU_CLIENT_LOG("[" << this << "] glDepthRangef(" << zNear << ", " << zFar << ")");  // NOLINT
  helper_->DepthRangef(zNear, zFar);
}

void DetachShader(GLuint program, GLuint shader) {
  GPU_CLIENT_LOG("[" << this << "] glDetachShader(" << program << ", " << shader << ")");  // NOLINT
  helper_->DetachShader(program, shader);
}

void Disable(GLenum cap) {
  GPU_CLIENT_LOG("[" << this << "] glDisable(" << GLES2Util::GetStringCapability(cap) << ")");  // NOLINT
  helper_->Disable(cap);
}

void DrawArrays(GLenum mode, GLint first, GLsizei count);

void DrawElements(
    GLenum mode, GLsizei count, GLenum type, const void* indices);

void Enable(GLenum cap) {
  GPU_CLIENT_LOG("[" << this << "] glEnable(" << GLES2Util::GetStringCapability(
      cap) << ")");
  helper_->Enable(cap);
}

void Finish();

void Flush();

void FramebufferRenderbuffer(
    GLenum target, GLenum attachment, GLenum renderbuffertarget,
    GLuint renderbuffer) {
  GPU_CLIENT_LOG("[" << this << "] glFramebufferRenderbuffer(" << GLES2Util::GetStringFrameBufferTarget(target) << ", " << GLES2Util::GetStringAttachment(attachment) << ", " << GLES2Util::GetStringRenderBufferTarget(renderbuffertarget) << ", " << renderbuffer << ")");  // NOLINT
  helper_->FramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
}

void FramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
    GLint level) {
  GPU_CLIENT_LOG("[" << this << "] glFramebufferTexture2D(" << GLES2Util::GetStringFrameBufferTarget(target) << ", " << GLES2Util::GetStringAttachment(attachment) << ", " << GLES2Util::GetStringTextureTarget(textarget) << ", " << texture << ", " << level << ")");  // NOLINT
  helper_->FramebufferTexture2D(target, attachment, textarget, texture, level);
}

void FrontFace(GLenum mode) {
  GPU_CLIENT_LOG("[" << this << "] glFrontFace(" << GLES2Util::GetStringFaceMode(mode) << ")");  // NOLINT
  helper_->FrontFace(mode);
}

void GenBuffers(GLsizei n, GLuint* buffers) {
  GPU_CLIENT_LOG("[" << this << "] glGenBuffers(" << n << ", " << static_cast<const void*>(buffers) << ")");  // NOLINT
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenBuffers: n < 0");
    return;
  }
  id_handlers_[id_namespaces::kBuffers]->
      MakeIds(0, n, buffers);
  helper_->GenBuffersImmediate(n, buffers);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << buffers[i]);
    }
  });
}

void GenerateMipmap(GLenum target) {
  GPU_CLIENT_LOG("[" << this << "] glGenerateMipmap(" << GLES2Util::GetStringTextureBindTarget(target) << ")");  // NOLINT
  helper_->GenerateMipmap(target);
}

void GenFramebuffers(GLsizei n, GLuint* framebuffers) {
  GPU_CLIENT_LOG("[" << this << "] glGenFramebuffers(" << n << ", " << static_cast<const void*>(framebuffers) << ")");  // NOLINT
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenFramebuffers: n < 0");
    return;
  }
  id_handlers_[id_namespaces::kFramebuffers]->
      MakeIds(0, n, framebuffers);
  helper_->GenFramebuffersImmediate(n, framebuffers);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << framebuffers[i]);
    }
  });
}

void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  GPU_CLIENT_LOG("[" << this << "] glGenRenderbuffers(" << n << ", " << static_cast<const void*>(renderbuffers) << ")");  // NOLINT
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenRenderbuffers: n < 0");
    return;
  }
  id_handlers_[id_namespaces::kRenderbuffers]->
      MakeIds(0, n, renderbuffers);
  helper_->GenRenderbuffersImmediate(n, renderbuffers);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << renderbuffers[i]);
    }
  });
}

void GenTextures(GLsizei n, GLuint* textures) {
  GPU_CLIENT_LOG("[" << this << "] glGenTextures(" << n << ", " << static_cast<const void*>(textures) << ")");  // NOLINT
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenTextures: n < 0");
    return;
  }
  id_handlers_[id_namespaces::kTextures]->
      MakeIds(0, n, textures);
  helper_->GenTexturesImmediate(n, textures);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << textures[i]);
    }
  });
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
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLboolean, params);
  GPU_CLIENT_LOG("[" << this << "] glGetBooleanv(" << GLES2Util::GetStringGLState(pname) << ", " << static_cast<const void*>(params) << ")");  // NOLINT
  if (GetBooleanvHelper(pname, params)) {
    return;
  }
  typedef GetBooleanv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetBooleanv(pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}
void GetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << this << "] glGetBufferParameteriv(" << GLES2Util::GetStringBufferTarget(target) << ", " << GLES2Util::GetStringBufferParameter(pname) << ", " << static_cast<const void*>(params) << ")");  // NOLINT
  if (GetBufferParameterivHelper(target, pname, params)) {
    return;
  }
  typedef GetBufferParameteriv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetBufferParameteriv(target, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}
GLenum GetError();

void GetFloatv(GLenum pname, GLfloat* params) {
  GPU_CLIENT_LOG("[" << this << "] glGetFloatv(" << GLES2Util::GetStringGLState(
      pname) << ", " << static_cast<const void*>(params) << ")");
  if (GetFloatvHelper(pname, params)) {
    return;
  }
  typedef GetFloatv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetFloatv(pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}
void GetFramebufferAttachmentParameteriv(
    GLenum target, GLenum attachment, GLenum pname, GLint* params) {
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << this << "] glGetFramebufferAttachmentParameteriv(" << GLES2Util::GetStringFrameBufferTarget(target) << ", " << GLES2Util::GetStringAttachment(attachment) << ", " << GLES2Util::GetStringFrameBufferParameter(pname) << ", " << static_cast<const void*>(params) << ")");  // NOLINT
  if (GetFramebufferAttachmentParameterivHelper(
      target, attachment, pname, params)) {
    return;
  }
  typedef GetFramebufferAttachmentParameteriv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetFramebufferAttachmentParameteriv(target, attachment, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}
void GetIntegerv(GLenum pname, GLint* params) {
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << this << "] glGetIntegerv(" << GLES2Util::GetStringGLState(pname) << ", " << static_cast<const void*>(params) << ")");  // NOLINT
  if (GetIntegervHelper(pname, params)) {
    return;
  }
  typedef GetIntegerv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetIntegerv(pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}
void GetProgramiv(GLuint program, GLenum pname, GLint* params) {
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << this << "] glGetProgramiv(" << program << ", " << GLES2Util::GetStringProgramParameter(pname) << ", " << static_cast<const void*>(params) << ")");  // NOLINT
  if (GetProgramivHelper(program, pname, params)) {
    return;
  }
  typedef GetProgramiv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetProgramiv(program, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}
void GetProgramInfoLog(
    GLuint program, GLsizei bufsize, GLsizei* length, char* infolog) {
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLsizei, length);
  GPU_CLIENT_LOG("[" << this << "] glGetProgramInfoLog" << "("
      << program << ", "
      << bufsize << ", "
      << static_cast<void*>(length) << ", "
      << static_cast<void*>(infolog) << ")");
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetProgramInfoLog(program, kResultBucketId);
  std::string str;
  GLsizei max_size = 0;
  if (GetBucketAsString(kResultBucketId, &str)) {
    if (bufsize > 0) {
      max_size =
          std::min(static_cast<size_t>(bufsize) - 1, str.size());
      memcpy(infolog, str.c_str(), max_size);
      infolog[max_size] = '\0';
      GPU_CLIENT_LOG("------\n" << infolog << "\n------");
    }
  }
  if (length != NULL) {
    *length = max_size;
  }
}
void GetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << this << "] glGetRenderbufferParameteriv(" << GLES2Util::GetStringRenderBufferTarget(target) << ", " << GLES2Util::GetStringRenderBufferParameter(pname) << ", " << static_cast<const void*>(params) << ")");  // NOLINT
  if (GetRenderbufferParameterivHelper(target, pname, params)) {
    return;
  }
  typedef GetRenderbufferParameteriv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetRenderbufferParameteriv(target, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}
void GetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << this << "] glGetShaderiv(" << shader << ", " << GLES2Util::GetStringShaderParameter(pname) << ", " << static_cast<const void*>(params) << ")");  // NOLINT
  if (GetShaderivHelper(shader, pname, params)) {
    return;
  }
  typedef GetShaderiv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetShaderiv(shader, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}
void GetShaderInfoLog(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog) {
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLsizei, length);
  GPU_CLIENT_LOG("[" << this << "] glGetShaderInfoLog" << "("
      << shader << ", "
      << bufsize << ", "
      << static_cast<void*>(length) << ", "
      << static_cast<void*>(infolog) << ")");
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetShaderInfoLog(shader, kResultBucketId);
  std::string str;
  GLsizei max_size = 0;
  if (GetBucketAsString(kResultBucketId, &str)) {
    if (bufsize > 0) {
      max_size =
          std::min(static_cast<size_t>(bufsize) - 1, str.size());
      memcpy(infolog, str.c_str(), max_size);
      infolog[max_size] = '\0';
      GPU_CLIENT_LOG("------\n" << infolog << "\n------");
    }
  }
  if (length != NULL) {
    *length = max_size;
  }
}
void GetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);

void GetShaderSource(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* source) {
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLsizei, length);
  GPU_CLIENT_LOG("[" << this << "] glGetShaderSource" << "("
      << shader << ", "
      << bufsize << ", "
      << static_cast<void*>(length) << ", "
      << static_cast<void*>(source) << ")");
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetShaderSource(shader, kResultBucketId);
  std::string str;
  GLsizei max_size = 0;
  if (GetBucketAsString(kResultBucketId, &str)) {
    if (bufsize > 0) {
      max_size =
          std::min(static_cast<size_t>(bufsize) - 1, str.size());
      memcpy(source, str.c_str(), max_size);
      source[max_size] = '\0';
      GPU_CLIENT_LOG("------\n" << source << "\n------");
    }
  }
  if (length != NULL) {
    *length = max_size;
  }
}
const GLubyte* GetString(GLenum name);

void GetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
  GPU_CLIENT_LOG("[" << this << "] glGetTexParameterfv(" << GLES2Util::GetStringGetTexParamTarget(target) << ", " << GLES2Util::GetStringTextureParameter(pname) << ", " << static_cast<const void*>(params) << ")");  // NOLINT
  if (GetTexParameterfvHelper(target, pname, params)) {
    return;
  }
  typedef GetTexParameterfv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetTexParameterfv(target, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}
void GetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << this << "] glGetTexParameteriv(" << GLES2Util::GetStringGetTexParamTarget(target) << ", " << GLES2Util::GetStringTextureParameter(pname) << ", " << static_cast<const void*>(params) << ")");  // NOLINT
  if (GetTexParameterivHelper(target, pname, params)) {
    return;
  }
  typedef GetTexParameteriv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetTexParameteriv(target, pname,
      result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}
void GetUniformfv(GLuint program, GLint location, GLfloat* params);

void GetUniformiv(GLuint program, GLint location, GLint* params);

GLint GetUniformLocation(GLuint program, const char* name);

void GetVertexAttribPointerv(GLuint index, GLenum pname, void** pointer);

void Hint(GLenum target, GLenum mode) {
  GPU_CLIENT_LOG("[" << this << "] glHint(" << GLES2Util::GetStringHintTarget(
      target) << ", " << GLES2Util::GetStringHintMode(mode) << ")");
  helper_->Hint(target, mode);
}

GLboolean IsBuffer(GLuint buffer) {
  GPU_CLIENT_LOG("[" << this << "] glIsBuffer(" << buffer << ")");
  typedef IsBuffer::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsBuffer(buffer, result_shm_id(), result_shm_offset());
  WaitForCmd();
  GPU_CLIENT_LOG("returned " << *result);
  return *result;
}

GLboolean IsEnabled(GLenum cap) {
  GPU_CLIENT_LOG("[" << this << "] glIsEnabled(" << GLES2Util::GetStringCapability(cap) << ")");  // NOLINT
  typedef IsEnabled::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsEnabled(cap, result_shm_id(), result_shm_offset());
  WaitForCmd();
  GPU_CLIENT_LOG("returned " << *result);
  return *result;
}

GLboolean IsFramebuffer(GLuint framebuffer) {
  GPU_CLIENT_LOG("[" << this << "] glIsFramebuffer(" << framebuffer << ")");
  typedef IsFramebuffer::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsFramebuffer(framebuffer, result_shm_id(), result_shm_offset());
  WaitForCmd();
  GPU_CLIENT_LOG("returned " << *result);
  return *result;
}

GLboolean IsProgram(GLuint program) {
  GPU_CLIENT_LOG("[" << this << "] glIsProgram(" << program << ")");
  typedef IsProgram::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsProgram(program, result_shm_id(), result_shm_offset());
  WaitForCmd();
  GPU_CLIENT_LOG("returned " << *result);
  return *result;
}

GLboolean IsRenderbuffer(GLuint renderbuffer) {
  GPU_CLIENT_LOG("[" << this << "] glIsRenderbuffer(" << renderbuffer << ")");
  typedef IsRenderbuffer::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsRenderbuffer(renderbuffer, result_shm_id(), result_shm_offset());
  WaitForCmd();
  GPU_CLIENT_LOG("returned " << *result);
  return *result;
}

GLboolean IsShader(GLuint shader) {
  GPU_CLIENT_LOG("[" << this << "] glIsShader(" << shader << ")");
  typedef IsShader::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsShader(shader, result_shm_id(), result_shm_offset());
  WaitForCmd();
  GPU_CLIENT_LOG("returned " << *result);
  return *result;
}

GLboolean IsTexture(GLuint texture) {
  GPU_CLIENT_LOG("[" << this << "] glIsTexture(" << texture << ")");
  typedef IsTexture::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->IsTexture(texture, result_shm_id(), result_shm_offset());
  WaitForCmd();
  GPU_CLIENT_LOG("returned " << *result);
  return *result;
}

void LineWidth(GLfloat width) {
  GPU_CLIENT_LOG("[" << this << "] glLineWidth(" << width << ")");
  helper_->LineWidth(width);
}

void LinkProgram(GLuint program);

void PixelStorei(GLenum pname, GLint param);

void PolygonOffset(GLfloat factor, GLfloat units) {
  GPU_CLIENT_LOG("[" << this << "] glPolygonOffset(" << factor << ", " << units << ")");  // NOLINT
  helper_->PolygonOffset(factor, units);
}

void ReadPixels(
    GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
    void* pixels);

void ReleaseShaderCompiler() {
  GPU_CLIENT_LOG("[" << this << "] glReleaseShaderCompiler(" << ")");
  helper_->ReleaseShaderCompiler();
}

void RenderbufferStorage(
    GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  GPU_CLIENT_LOG("[" << this << "] glRenderbufferStorage(" << GLES2Util::GetStringRenderBufferTarget(target) << ", " << GLES2Util::GetStringRenderBufferFormat(internalformat) << ", " << width << ", " << height << ")");  // NOLINT
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
  GPU_CLIENT_LOG("[" << this << "] glSampleCoverage(" << value << ", " << GLES2Util::GetStringBool(invert) << ")");  // NOLINT
  helper_->SampleCoverage(value, invert);
}

void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  GPU_CLIENT_LOG("[" << this << "] glScissor(" << x << ", " << y << ", " << width << ", " << height << ")");  // NOLINT
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
  GPU_CLIENT_LOG("[" << this << "] glStencilFunc(" << GLES2Util::GetStringCmpFunction(func) << ", " << ref << ", " << mask << ")");  // NOLINT
  helper_->StencilFunc(func, ref, mask);
}

void StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
  GPU_CLIENT_LOG("[" << this << "] glStencilFuncSeparate(" << GLES2Util::GetStringFaceType(face) << ", " << GLES2Util::GetStringCmpFunction(func) << ", " << ref << ", " << mask << ")");  // NOLINT
  helper_->StencilFuncSeparate(face, func, ref, mask);
}

void StencilMask(GLuint mask) {
  GPU_CLIENT_LOG("[" << this << "] glStencilMask(" << mask << ")");
  helper_->StencilMask(mask);
}

void StencilMaskSeparate(GLenum face, GLuint mask) {
  GPU_CLIENT_LOG("[" << this << "] glStencilMaskSeparate(" << GLES2Util::GetStringFaceType(face) << ", " << mask << ")");  // NOLINT
  helper_->StencilMaskSeparate(face, mask);
}

void StencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  GPU_CLIENT_LOG("[" << this << "] glStencilOp(" << GLES2Util::GetStringStencilOp(fail) << ", " << GLES2Util::GetStringStencilOp(zfail) << ", " << GLES2Util::GetStringStencilOp(zpass) << ")");  // NOLINT
  helper_->StencilOp(fail, zfail, zpass);
}

void StencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass) {
  GPU_CLIENT_LOG("[" << this << "] glStencilOpSeparate(" << GLES2Util::GetStringFaceType(face) << ", " << GLES2Util::GetStringStencilOp(fail) << ", " << GLES2Util::GetStringStencilOp(zfail) << ", " << GLES2Util::GetStringStencilOp(zpass) << ")");  // NOLINT
  helper_->StencilOpSeparate(face, fail, zfail, zpass);
}

void TexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels);

void TexParameterf(GLenum target, GLenum pname, GLfloat param) {
  GPU_CLIENT_LOG("[" << this << "] glTexParameterf(" << GLES2Util::GetStringTextureBindTarget(target) << ", " << GLES2Util::GetStringTextureParameter(pname) << ", " << param << ")");  // NOLINT
  helper_->TexParameterf(target, pname, param);
}

void TexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
  GPU_CLIENT_LOG("[" << this << "] glTexParameterfv(" << GLES2Util::GetStringTextureBindTarget(target) << ", " << GLES2Util::GetStringTextureParameter(pname) << ", " << static_cast<const void*>(params) << ")");  // NOLINT
  GPU_CLIENT_LOG("values: " << params[0]);
  helper_->TexParameterfvImmediate(target, pname, params);
}

void TexParameteri(GLenum target, GLenum pname, GLint param) {
  GPU_CLIENT_LOG("[" << this << "] glTexParameteri(" << GLES2Util::GetStringTextureBindTarget(target) << ", " << GLES2Util::GetStringTextureParameter(pname) << ", " << param << ")");  // NOLINT
  helper_->TexParameteri(target, pname, param);
}

void TexParameteriv(GLenum target, GLenum pname, const GLint* params) {
  GPU_CLIENT_LOG("[" << this << "] glTexParameteriv(" << GLES2Util::GetStringTextureBindTarget(target) << ", " << GLES2Util::GetStringTextureParameter(pname) << ", " << static_cast<const void*>(params) << ")");  // NOLINT
  GPU_CLIENT_LOG("values: " << params[0]);
  helper_->TexParameterivImmediate(target, pname, params);
}

void TexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels);

void Uniform1f(GLint location, GLfloat x) {
  GPU_CLIENT_LOG("[" << this << "] glUniform1f(" << location << ", " << x << ")");  // NOLINT
  helper_->Uniform1f(location, x);
}

void Uniform1fv(GLint location, GLsizei count, const GLfloat* v) {
  GPU_CLIENT_LOG("[" << this << "] glUniform1fv(" << location << ", " << count << ", " << static_cast<const void*>(v) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
       GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 1]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform1fv: count < 0");
    return;
  }
  helper_->Uniform1fvImmediate(location, count, v);
}

void Uniform1i(GLint location, GLint x) {
  GPU_CLIENT_LOG("[" << this << "] glUniform1i(" << location << ", " << x << ")");  // NOLINT
  helper_->Uniform1i(location, x);
}

void Uniform1iv(GLint location, GLsizei count, const GLint* v) {
  GPU_CLIENT_LOG("[" << this << "] glUniform1iv(" << location << ", " << count << ", " << static_cast<const void*>(v) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
       GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 1]);
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform1iv: count < 0");
    return;
  }
  helper_->Uniform1ivImmediate(location, count, v);
}

void Uniform2f(GLint location, GLfloat x, GLfloat y) {
  GPU_CLIENT_LOG("[" << this << "] glUniform2f(" << location << ", " << x << ", " << y << ")");  // NOLINT
  helper_->Uniform2f(location, x, y);
}

void Uniform2fv(GLint location, GLsizei count, const GLfloat* v) {
  GPU_CLIENT_LOG("[" << this << "] glUniform2fv(" << location << ", " << count << ", " << static_cast<const void*>(v) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
       GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 2] << ", " << v[1 + i * 2]);  // NOLINT
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform2fv: count < 0");
    return;
  }
  helper_->Uniform2fvImmediate(location, count, v);
}

void Uniform2i(GLint location, GLint x, GLint y) {
  GPU_CLIENT_LOG("[" << this << "] glUniform2i(" << location << ", " << x << ", " << y << ")");  // NOLINT
  helper_->Uniform2i(location, x, y);
}

void Uniform2iv(GLint location, GLsizei count, const GLint* v) {
  GPU_CLIENT_LOG("[" << this << "] glUniform2iv(" << location << ", " << count << ", " << static_cast<const void*>(v) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
       GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 2] << ", " << v[1 + i * 2]);  // NOLINT
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform2iv: count < 0");
    return;
  }
  helper_->Uniform2ivImmediate(location, count, v);
}

void Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  GPU_CLIENT_LOG("[" << this << "] glUniform3f(" << location << ", " << x << ", " << y << ", " << z << ")");  // NOLINT
  helper_->Uniform3f(location, x, y, z);
}

void Uniform3fv(GLint location, GLsizei count, const GLfloat* v) {
  GPU_CLIENT_LOG("[" << this << "] glUniform3fv(" << location << ", " << count << ", " << static_cast<const void*>(v) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
       GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 3] << ", " << v[1 + i * 3] << ", " << v[2 + i * 3]);  // NOLINT
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform3fv: count < 0");
    return;
  }
  helper_->Uniform3fvImmediate(location, count, v);
}

void Uniform3i(GLint location, GLint x, GLint y, GLint z) {
  GPU_CLIENT_LOG("[" << this << "] glUniform3i(" << location << ", " << x << ", " << y << ", " << z << ")");  // NOLINT
  helper_->Uniform3i(location, x, y, z);
}

void Uniform3iv(GLint location, GLsizei count, const GLint* v) {
  GPU_CLIENT_LOG("[" << this << "] glUniform3iv(" << location << ", " << count << ", " << static_cast<const void*>(v) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
       GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 3] << ", " << v[1 + i * 3] << ", " << v[2 + i * 3]);  // NOLINT
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform3iv: count < 0");
    return;
  }
  helper_->Uniform3ivImmediate(location, count, v);
}

void Uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  GPU_CLIENT_LOG("[" << this << "] glUniform4f(" << location << ", " << x << ", " << y << ", " << z << ", " << w << ")");  // NOLINT
  helper_->Uniform4f(location, x, y, z, w);
}

void Uniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  GPU_CLIENT_LOG("[" << this << "] glUniform4fv(" << location << ", " << count << ", " << static_cast<const void*>(v) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
       GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 4] << ", " << v[1 + i * 4] << ", " << v[2 + i * 4] << ", " << v[3 + i * 4]);  // NOLINT
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform4fv: count < 0");
    return;
  }
  helper_->Uniform4fvImmediate(location, count, v);
}

void Uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
  GPU_CLIENT_LOG("[" << this << "] glUniform4i(" << location << ", " << x << ", " << y << ", " << z << ", " << w << ")");  // NOLINT
  helper_->Uniform4i(location, x, y, z, w);
}

void Uniform4iv(GLint location, GLsizei count, const GLint* v) {
  GPU_CLIENT_LOG("[" << this << "] glUniform4iv(" << location << ", " << count << ", " << static_cast<const void*>(v) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
       GPU_CLIENT_LOG("  " << i << ": " << v[0 + i * 4] << ", " << v[1 + i * 4] << ", " << v[2 + i * 4] << ", " << v[3 + i * 4]);  // NOLINT
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniform4iv: count < 0");
    return;
  }
  helper_->Uniform4ivImmediate(location, count, v);
}

void UniformMatrix2fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  GPU_CLIENT_LOG("[" << this << "] glUniformMatrix2fv(" << location << ", " << count << ", " << GLES2Util::GetStringBool(transpose) << ", " << static_cast<const void*>(value) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
       GPU_CLIENT_LOG("  " << i << ": " << value[0 + i * 4] << ", " << value[1 + i * 4] << ", " << value[2 + i * 4] << ", " << value[3 + i * 4]);  // NOLINT
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix2fv: count < 0");
    return;
  }
  helper_->UniformMatrix2fvImmediate(location, count, transpose, value);
}

void UniformMatrix3fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  GPU_CLIENT_LOG("[" << this << "] glUniformMatrix3fv(" << location << ", " << count << ", " << GLES2Util::GetStringBool(transpose) << ", " << static_cast<const void*>(value) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
       GPU_CLIENT_LOG("  " << i << ": " << value[0 + i * 9] << ", " << value[1 + i * 9] << ", " << value[2 + i * 9] << ", " << value[3 + i * 9] << ", " << value[4 + i * 9] << ", " << value[5 + i * 9] << ", " << value[6 + i * 9] << ", " << value[7 + i * 9] << ", " << value[8 + i * 9]);  // NOLINT
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix3fv: count < 0");
    return;
  }
  helper_->UniformMatrix3fvImmediate(location, count, transpose, value);
}

void UniformMatrix4fv(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
  GPU_CLIENT_LOG("[" << this << "] glUniformMatrix4fv(" << location << ", " << count << ", " << GLES2Util::GetStringBool(transpose) << ", " << static_cast<const void*>(value) << ")");  // NOLINT
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < count; ++i) {
       GPU_CLIENT_LOG("  " << i << ": " << value[0 + i * 16] << ", " << value[1 + i * 16] << ", " << value[2 + i * 16] << ", " << value[3 + i * 16] << ", " << value[4 + i * 16] << ", " << value[5 + i * 16] << ", " << value[6 + i * 16] << ", " << value[7 + i * 16] << ", " << value[8 + i * 16] << ", " << value[9 + i * 16] << ", " << value[10 + i * 16] << ", " << value[11 + i * 16] << ", " << value[12 + i * 16] << ", " << value[13 + i * 16] << ", " << value[14 + i * 16] << ", " << value[15 + i * 16]);  // NOLINT
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glUniformMatrix4fv: count < 0");
    return;
  }
  helper_->UniformMatrix4fvImmediate(location, count, transpose, value);
}

void UseProgram(GLuint program) {
  GPU_CLIENT_LOG("[" << this << "] glUseProgram(" << program << ")");
  helper_->UseProgram(program);
}

void ValidateProgram(GLuint program) {
  GPU_CLIENT_LOG("[" << this << "] glValidateProgram(" << program << ")");
  helper_->ValidateProgram(program);
}

void VertexAttrib1f(GLuint indx, GLfloat x) {
  GPU_CLIENT_LOG("[" << this << "] glVertexAttrib1f(" << indx << ", " << x << ")");  // NOLINT
  helper_->VertexAttrib1f(indx, x);
}

void VertexAttrib1fv(GLuint indx, const GLfloat* values) {
  GPU_CLIENT_LOG("[" << this << "] glVertexAttrib1fv(" << indx << ", " << static_cast<const void*>(values) << ")");  // NOLINT
  GPU_CLIENT_LOG("values: " << values[0]);
  helper_->VertexAttrib1fvImmediate(indx, values);
}

void VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
  GPU_CLIENT_LOG("[" << this << "] glVertexAttrib2f(" << indx << ", " << x << ", " << y << ")");  // NOLINT
  helper_->VertexAttrib2f(indx, x, y);
}

void VertexAttrib2fv(GLuint indx, const GLfloat* values) {
  GPU_CLIENT_LOG("[" << this << "] glVertexAttrib2fv(" << indx << ", " << static_cast<const void*>(values) << ")");  // NOLINT
  GPU_CLIENT_LOG("values: " << values[0] << ", " << values[1]);
  helper_->VertexAttrib2fvImmediate(indx, values);
}

void VertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  GPU_CLIENT_LOG("[" << this << "] glVertexAttrib3f(" << indx << ", " << x << ", " << y << ", " << z << ")");  // NOLINT
  helper_->VertexAttrib3f(indx, x, y, z);
}

void VertexAttrib3fv(GLuint indx, const GLfloat* values) {
  GPU_CLIENT_LOG("[" << this << "] glVertexAttrib3fv(" << indx << ", " << static_cast<const void*>(values) << ")");  // NOLINT
  GPU_CLIENT_LOG("values: " << values[0] << ", " << values[1] << ", " << values[2]);  // NOLINT
  helper_->VertexAttrib3fvImmediate(indx, values);
}

void VertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  GPU_CLIENT_LOG("[" << this << "] glVertexAttrib4f(" << indx << ", " << x << ", " << y << ", " << z << ", " << w << ")");  // NOLINT
  helper_->VertexAttrib4f(indx, x, y, z, w);
}

void VertexAttrib4fv(GLuint indx, const GLfloat* values) {
  GPU_CLIENT_LOG("[" << this << "] glVertexAttrib4fv(" << indx << ", " << static_cast<const void*>(values) << ")");  // NOLINT
  GPU_CLIENT_LOG("values: " << values[0] << ", " << values[1] << ", " << values[2] << ", " << values[3]);  // NOLINT
  helper_->VertexAttrib4fvImmediate(indx, values);
}

void VertexAttribPointer(
    GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void* ptr);

void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  GPU_CLIENT_LOG("[" << this << "] glViewport(" << x << ", " << y << ", " << width << ", " << height << ")");  // NOLINT
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
  GPU_CLIENT_LOG("[" << this << "] glBlitFramebufferEXT(" << srcX0 << ", " << srcY0 << ", " << srcX1 << ", " << srcY1 << ", " << dstX0 << ", " << dstY0 << ", " << dstX1 << ", " << dstY1 << ", " << mask << ", " << GLES2Util::GetStringBlitFilter(filter) << ")");  // NOLINT
  helper_->BlitFramebufferEXT(
      srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void RenderbufferStorageMultisampleEXT(
    GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
    GLsizei height) {
  GPU_CLIENT_LOG("[" << this << "] glRenderbufferStorageMultisampleEXT(" << GLES2Util::GetStringRenderBufferTarget(target) << ", " << samples << ", " << GLES2Util::GetStringRenderBufferFormat(internalformat) << ", " << width << ", " << height << ")");  // NOLINT
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
  GPU_CLIENT_LOG("[" << this << "] glGetMaxValueInBufferCHROMIUM(" << buffer_id << ", " << count << ", " << GLES2Util::GetStringGetMaxIndexType(type) << ", " << offset << ")");  // NOLINT
  typedef GetMaxValueInBufferCHROMIUM::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->GetMaxValueInBufferCHROMIUM(
      buffer_id, count, type, offset, result_shm_id(), result_shm_offset());
  WaitForCmd();
  GPU_CLIENT_LOG("returned " << *result);
  return *result;
}

void GenSharedIdsCHROMIUM(
    GLuint namespace_id, GLuint id_offset, GLsizei n, GLuint* ids);

void DeleteSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids);

void RegisterSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids);

GLboolean EnableFeatureCHROMIUM(const char* feature);

void* MapBufferSubDataCHROMIUM(
    GLuint target, GLintptr offset, GLsizeiptr size, GLenum access);

void UnmapBufferSubDataCHROMIUM(const void* mem);

void* MapTexSubImage2DCHROMIUM(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, GLenum access);

void UnmapTexSubImage2DCHROMIUM(const void* mem);

void ResizeCHROMIUM(GLuint width, GLuint height);

const GLchar* GetRequestableExtensionsCHROMIUM();

void RequestExtensionCHROMIUM(const char* extension);

void RateLimitOffscreenContextCHROMIUM();

void GetMultipleIntegervCHROMIUM(
    const GLenum* pnames, GLuint count, GLint* results, GLsizeiptr size);

void GetProgramInfoCHROMIUM(
    GLuint program, GLsizei bufsize, GLsizei* size, void* info);

GLuint CreateStreamTextureCHROMIUM(GLuint texture);

void DestroyStreamTextureCHROMIUM(GLuint texture);

void GetTranslatedShaderSourceANGLE(
    GLuint shader, GLsizei bufsize, GLsizei* length, char* source) {
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLsizei, length);
  GPU_CLIENT_LOG("[" << this << "] glGetTranslatedShaderSourceANGLE" << "("
      << shader << ", "
      << bufsize << ", "
      << static_cast<void*>(length) << ", "
      << static_cast<void*>(source) << ")");
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetTranslatedShaderSourceANGLE(shader, kResultBucketId);
  std::string str;
  GLsizei max_size = 0;
  if (GetBucketAsString(kResultBucketId, &str)) {
    if (bufsize > 0) {
      max_size =
          std::min(static_cast<size_t>(bufsize) - 1, str.size());
      memcpy(source, str.c_str(), max_size);
      source[max_size] = '\0';
      GPU_CLIENT_LOG("------\n" << source << "\n------");
    }
  }
  if (length != NULL) {
    *length = max_size;
  }
}
void PostSubBufferCHROMIUM(GLint x, GLint y, GLint width, GLint height);

void TexImageIOSurface2DCHROMIUM(
    GLenum target, GLsizei width, GLsizei height, GLuint ioSurfaceId,
    GLuint plane) {
  GPU_CLIENT_LOG("[" << this << "] glTexImageIOSurface2DCHROMIUM(" << GLES2Util::GetStringTextureBindTarget(target) << ", " << width << ", " << height << ", " << ioSurfaceId << ", " << plane << ")");  // NOLINT
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexImageIOSurface2DCHROMIUM: width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexImageIOSurface2DCHROMIUM: height < 0");
    return;
  }
  helper_->TexImageIOSurface2DCHROMIUM(
      target, width, height, ioSurfaceId, plane);
}

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_AUTOGEN_H_

