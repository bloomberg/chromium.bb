// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_AUTOGEN_H_

  void ActiveTexture(GLenum texture) {
    gles2::ActiveTexture* c = GetCmdSpace<gles2::ActiveTexture>();
    if (c) {
      c->Init(texture);
    }
  }

  void AttachShader(GLuint program, GLuint shader) {
    gles2::AttachShader* c = GetCmdSpace<gles2::AttachShader>();
    if (c) {
      c->Init(program, shader);
    }
  }

  void BindAttribLocation(
      GLuint program, GLuint index, uint32 name_shm_id, uint32 name_shm_offset,
      uint32 data_size) {
    gles2::BindAttribLocation* c = GetCmdSpace<gles2::BindAttribLocation>();
    if (c) {
      c->Init(program, index, name_shm_id, name_shm_offset, data_size);
    }
  }

  void BindAttribLocationImmediate(
      GLuint program, GLuint index, const char* name) {
    const uint32 data_size = strlen(name);
    gles2::BindAttribLocationImmediate* c =
        GetImmediateCmdSpace<gles2::BindAttribLocationImmediate>(data_size);
    if (c) {
      c->Init(program, index, name, data_size);
    }
  }

  void BindAttribLocationBucket(
      GLuint program, GLuint index, uint32 name_bucket_id) {
    gles2::BindAttribLocationBucket* c =
        GetCmdSpace<gles2::BindAttribLocationBucket>();
    if (c) {
      c->Init(program, index, name_bucket_id);
    }
  }

  void BindBuffer(GLenum target, GLuint buffer) {
    gles2::BindBuffer* c = GetCmdSpace<gles2::BindBuffer>();
    if (c) {
      c->Init(target, buffer);
    }
  }

  void BindFramebuffer(GLenum target, GLuint framebuffer) {
    gles2::BindFramebuffer* c = GetCmdSpace<gles2::BindFramebuffer>();
    if (c) {
      c->Init(target, framebuffer);
    }
  }

  void BindRenderbuffer(GLenum target, GLuint renderbuffer) {
    gles2::BindRenderbuffer* c = GetCmdSpace<gles2::BindRenderbuffer>();
    if (c) {
      c->Init(target, renderbuffer);
    }
  }

  void BindTexture(GLenum target, GLuint texture) {
    gles2::BindTexture* c = GetCmdSpace<gles2::BindTexture>();
    if (c) {
      c->Init(target, texture);
    }
  }

  void BlendColor(
      GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    gles2::BlendColor* c = GetCmdSpace<gles2::BlendColor>();
    if (c) {
      c->Init(red, green, blue, alpha);
    }
  }

  void BlendEquation(GLenum mode) {
    gles2::BlendEquation* c = GetCmdSpace<gles2::BlendEquation>();
    if (c) {
      c->Init(mode);
    }
  }

  void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    gles2::BlendEquationSeparate* c =
        GetCmdSpace<gles2::BlendEquationSeparate>();
    if (c) {
      c->Init(modeRGB, modeAlpha);
    }
  }

  void BlendFunc(GLenum sfactor, GLenum dfactor) {
    gles2::BlendFunc* c = GetCmdSpace<gles2::BlendFunc>();
    if (c) {
      c->Init(sfactor, dfactor);
    }
  }

  void BlendFuncSeparate(
      GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    gles2::BlendFuncSeparate* c = GetCmdSpace<gles2::BlendFuncSeparate>();
    if (c) {
      c->Init(srcRGB, dstRGB, srcAlpha, dstAlpha);
    }
  }

  void BufferData(
      GLenum target, GLsizeiptr size, uint32 data_shm_id,
      uint32 data_shm_offset, GLenum usage) {
    gles2::BufferData* c = GetCmdSpace<gles2::BufferData>();
    if (c) {
      c->Init(target, size, data_shm_id, data_shm_offset, usage);
    }
  }

  void BufferDataImmediate(GLenum target, GLsizeiptr size, GLenum usage) {
    const uint32 s = 0;  // TODO(gman): compute correct size
    gles2::BufferDataImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::BufferDataImmediate>(s);
    if (c) {
      c->Init(target, size, usage);
    }
  }

  void BufferSubData(
      GLenum target, GLintptr offset, GLsizeiptr size, uint32 data_shm_id,
      uint32 data_shm_offset) {
    gles2::BufferSubData* c = GetCmdSpace<gles2::BufferSubData>();
    if (c) {
      c->Init(target, offset, size, data_shm_id, data_shm_offset);
    }
  }

  void BufferSubDataImmediate(
      GLenum target, GLintptr offset, GLsizeiptr size) {
    const uint32 s = 0;  // TODO(gman): compute correct size
    gles2::BufferSubDataImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::BufferSubDataImmediate>(s);
    if (c) {
      c->Init(target, offset, size);
    }
  }

  void CheckFramebufferStatus(
      GLenum target, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::CheckFramebufferStatus* c =
        GetCmdSpace<gles2::CheckFramebufferStatus>();
    if (c) {
      c->Init(target, result_shm_id, result_shm_offset);
    }
  }

  void Clear(GLbitfield mask) {
    gles2::Clear* c = GetCmdSpace<gles2::Clear>();
    if (c) {
      c->Init(mask);
    }
  }

  void ClearColor(
      GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    gles2::ClearColor* c = GetCmdSpace<gles2::ClearColor>();
    if (c) {
      c->Init(red, green, blue, alpha);
    }
  }

  void ClearDepthf(GLclampf depth) {
    gles2::ClearDepthf* c = GetCmdSpace<gles2::ClearDepthf>();
    if (c) {
      c->Init(depth);
    }
  }

  void ClearStencil(GLint s) {
    gles2::ClearStencil* c = GetCmdSpace<gles2::ClearStencil>();
    if (c) {
      c->Init(s);
    }
  }

  void ColorMask(
      GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    gles2::ColorMask* c = GetCmdSpace<gles2::ColorMask>();
    if (c) {
      c->Init(red, green, blue, alpha);
    }
  }

  void CompileShader(GLuint shader) {
    gles2::CompileShader* c = GetCmdSpace<gles2::CompileShader>();
    if (c) {
      c->Init(shader);
    }
  }

  void CompressedTexImage2D(
      GLenum target, GLint level, GLenum internalformat, GLsizei width,
      GLsizei height, GLint border, GLsizei imageSize, uint32 data_shm_id,
      uint32 data_shm_offset) {
    gles2::CompressedTexImage2D* c =
        GetCmdSpace<gles2::CompressedTexImage2D>();
    if (c) {
      c->Init(
          target, level, internalformat, width, height, border, imageSize,
          data_shm_id, data_shm_offset);
    }
  }

  void CompressedTexImage2DImmediate(
      GLenum target, GLint level, GLenum internalformat, GLsizei width,
      GLsizei height, GLint border, GLsizei imageSize) {
    const uint32 s = 0;  // TODO(gman): compute correct size
    gles2::CompressedTexImage2DImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::CompressedTexImage2DImmediate>(s);
    if (c) {
      c->Init(target, level, internalformat, width, height, border, imageSize);
    }
  }

  void CompressedTexImage2DBucket(
      GLenum target, GLint level, GLenum internalformat, GLsizei width,
      GLsizei height, GLint border, GLuint bucket_id) {
    gles2::CompressedTexImage2DBucket* c =
        GetCmdSpace<gles2::CompressedTexImage2DBucket>();
    if (c) {
      c->Init(target, level, internalformat, width, height, border, bucket_id);
    }
  }

  void CompressedTexSubImage2D(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLsizei imageSize, uint32 data_shm_id,
      uint32 data_shm_offset) {
    gles2::CompressedTexSubImage2D* c =
        GetCmdSpace<gles2::CompressedTexSubImage2D>();
    if (c) {
      c->Init(
          target, level, xoffset, yoffset, width, height, format, imageSize,
          data_shm_id, data_shm_offset);
    }
  }

  void CompressedTexSubImage2DImmediate(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLsizei imageSize) {
    const uint32 s = 0;  // TODO(gman): compute correct size
    gles2::CompressedTexSubImage2DImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::CompressedTexSubImage2DImmediate>(
            s);
    if (c) {
      c->Init(
          target, level, xoffset, yoffset, width, height, format, imageSize);
    }
  }

  void CompressedTexSubImage2DBucket(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLuint bucket_id) {
    gles2::CompressedTexSubImage2DBucket* c =
        GetCmdSpace<gles2::CompressedTexSubImage2DBucket>();
    if (c) {
      c->Init(
          target, level, xoffset, yoffset, width, height, format, bucket_id);
    }
  }

  void CopyTexImage2D(
      GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
      GLsizei width, GLsizei height, GLint border) {
    gles2::CopyTexImage2D* c = GetCmdSpace<gles2::CopyTexImage2D>();
    if (c) {
      c->Init(target, level, internalformat, x, y, width, height, border);
    }
  }

  void CopyTexSubImage2D(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x,
      GLint y, GLsizei width, GLsizei height) {
    gles2::CopyTexSubImage2D* c = GetCmdSpace<gles2::CopyTexSubImage2D>();
    if (c) {
      c->Init(target, level, xoffset, yoffset, x, y, width, height);
    }
  }

  void CreateProgram(uint32 client_id) {
    gles2::CreateProgram* c = GetCmdSpace<gles2::CreateProgram>();
    if (c) {
      c->Init(client_id);
    }
  }

  void CreateShader(GLenum type, uint32 client_id) {
    gles2::CreateShader* c = GetCmdSpace<gles2::CreateShader>();
    if (c) {
      c->Init(type, client_id);
    }
  }

  void CullFace(GLenum mode) {
    gles2::CullFace* c = GetCmdSpace<gles2::CullFace>();
    if (c) {
      c->Init(mode);
    }
  }

  void DeleteBuffers(
      GLsizei n, uint32 buffers_shm_id, uint32 buffers_shm_offset) {
    gles2::DeleteBuffers* c = GetCmdSpace<gles2::DeleteBuffers>();
    if (c) {
      c->Init(n, buffers_shm_id, buffers_shm_offset);
    }
  }

  void DeleteBuffersImmediate(GLsizei n, const GLuint* buffers) {
    const uint32 size = gles2::DeleteBuffersImmediate::ComputeSize(n);
    gles2::DeleteBuffersImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::DeleteBuffersImmediate>(size);
    if (c) {
      c->Init(n, buffers);
    }
  }

  void DeleteFramebuffers(
      GLsizei n, uint32 framebuffers_shm_id, uint32 framebuffers_shm_offset) {
    gles2::DeleteFramebuffers* c = GetCmdSpace<gles2::DeleteFramebuffers>();
    if (c) {
      c->Init(n, framebuffers_shm_id, framebuffers_shm_offset);
    }
  }

  void DeleteFramebuffersImmediate(GLsizei n, const GLuint* framebuffers) {
    const uint32 size = gles2::DeleteFramebuffersImmediate::ComputeSize(n);
    gles2::DeleteFramebuffersImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::DeleteFramebuffersImmediate>(
            size);
    if (c) {
      c->Init(n, framebuffers);
    }
  }

  void DeleteProgram(GLuint program) {
    gles2::DeleteProgram* c = GetCmdSpace<gles2::DeleteProgram>();
    if (c) {
      c->Init(program);
    }
  }

  void DeleteRenderbuffers(
      GLsizei n, uint32 renderbuffers_shm_id,
      uint32 renderbuffers_shm_offset) {
    gles2::DeleteRenderbuffers* c = GetCmdSpace<gles2::DeleteRenderbuffers>();
    if (c) {
      c->Init(n, renderbuffers_shm_id, renderbuffers_shm_offset);
    }
  }

  void DeleteRenderbuffersImmediate(GLsizei n, const GLuint* renderbuffers) {
    const uint32 size = gles2::DeleteRenderbuffersImmediate::ComputeSize(n);
    gles2::DeleteRenderbuffersImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::DeleteRenderbuffersImmediate>(
            size);
    if (c) {
      c->Init(n, renderbuffers);
    }
  }

  void DeleteShader(GLuint shader) {
    gles2::DeleteShader* c = GetCmdSpace<gles2::DeleteShader>();
    if (c) {
      c->Init(shader);
    }
  }

  void DeleteTextures(
      GLsizei n, uint32 textures_shm_id, uint32 textures_shm_offset) {
    gles2::DeleteTextures* c = GetCmdSpace<gles2::DeleteTextures>();
    if (c) {
      c->Init(n, textures_shm_id, textures_shm_offset);
    }
  }

  void DeleteTexturesImmediate(GLsizei n, const GLuint* textures) {
    const uint32 size = gles2::DeleteTexturesImmediate::ComputeSize(n);
    gles2::DeleteTexturesImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::DeleteTexturesImmediate>(size);
    if (c) {
      c->Init(n, textures);
    }
  }

  void DepthFunc(GLenum func) {
    gles2::DepthFunc* c = GetCmdSpace<gles2::DepthFunc>();
    if (c) {
      c->Init(func);
    }
  }

  void DepthMask(GLboolean flag) {
    gles2::DepthMask* c = GetCmdSpace<gles2::DepthMask>();
    if (c) {
      c->Init(flag);
    }
  }

  void DepthRangef(GLclampf zNear, GLclampf zFar) {
    gles2::DepthRangef* c = GetCmdSpace<gles2::DepthRangef>();
    if (c) {
      c->Init(zNear, zFar);
    }
  }

  void DetachShader(GLuint program, GLuint shader) {
    gles2::DetachShader* c = GetCmdSpace<gles2::DetachShader>();
    if (c) {
      c->Init(program, shader);
    }
  }

  void Disable(GLenum cap) {
    gles2::Disable* c = GetCmdSpace<gles2::Disable>();
    if (c) {
      c->Init(cap);
    }
  }

  void DisableVertexAttribArray(GLuint index) {
    gles2::DisableVertexAttribArray* c =
        GetCmdSpace<gles2::DisableVertexAttribArray>();
    if (c) {
      c->Init(index);
    }
  }

  void DrawArrays(GLenum mode, GLint first, GLsizei count) {
    gles2::DrawArrays* c = GetCmdSpace<gles2::DrawArrays>();
    if (c) {
      c->Init(mode, first, count);
    }
  }

  void DrawElements(
      GLenum mode, GLsizei count, GLenum type, GLuint index_offset) {
    gles2::DrawElements* c = GetCmdSpace<gles2::DrawElements>();
    if (c) {
      c->Init(mode, count, type, index_offset);
    }
  }

  void Enable(GLenum cap) {
    gles2::Enable* c = GetCmdSpace<gles2::Enable>();
    if (c) {
      c->Init(cap);
    }
  }

  void EnableVertexAttribArray(GLuint index) {
    gles2::EnableVertexAttribArray* c =
        GetCmdSpace<gles2::EnableVertexAttribArray>();
    if (c) {
      c->Init(index);
    }
  }

  void Finish() {
    gles2::Finish* c = GetCmdSpace<gles2::Finish>();
    if (c) {
      c->Init();
    }
  }

  void Flush() {
    gles2::Flush* c = GetCmdSpace<gles2::Flush>();
    if (c) {
      c->Init();
    }
  }

  void FramebufferRenderbuffer(
      GLenum target, GLenum attachment, GLenum renderbuffertarget,
      GLuint renderbuffer) {
    gles2::FramebufferRenderbuffer* c =
        GetCmdSpace<gles2::FramebufferRenderbuffer>();
    if (c) {
      c->Init(target, attachment, renderbuffertarget, renderbuffer);
    }
  }

  void FramebufferTexture2D(
      GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
      GLint level) {
    gles2::FramebufferTexture2D* c =
        GetCmdSpace<gles2::FramebufferTexture2D>();
    if (c) {
      c->Init(target, attachment, textarget, texture, level);
    }
  }

  void FrontFace(GLenum mode) {
    gles2::FrontFace* c = GetCmdSpace<gles2::FrontFace>();
    if (c) {
      c->Init(mode);
    }
  }

  void GenBuffers(
      GLsizei n, uint32 buffers_shm_id, uint32 buffers_shm_offset) {
    gles2::GenBuffers* c = GetCmdSpace<gles2::GenBuffers>();
    if (c) {
      c->Init(n, buffers_shm_id, buffers_shm_offset);
    }
  }

  void GenBuffersImmediate(GLsizei n, GLuint* buffers) {
    const uint32 size = gles2::GenBuffersImmediate::ComputeSize(n);
    gles2::GenBuffersImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::GenBuffersImmediate>(size);
    if (c) {
      c->Init(n, buffers);
    }
  }

  void GenerateMipmap(GLenum target) {
    gles2::GenerateMipmap* c = GetCmdSpace<gles2::GenerateMipmap>();
    if (c) {
      c->Init(target);
    }
  }

  void GenFramebuffers(
      GLsizei n, uint32 framebuffers_shm_id, uint32 framebuffers_shm_offset) {
    gles2::GenFramebuffers* c = GetCmdSpace<gles2::GenFramebuffers>();
    if (c) {
      c->Init(n, framebuffers_shm_id, framebuffers_shm_offset);
    }
  }

  void GenFramebuffersImmediate(GLsizei n, GLuint* framebuffers) {
    const uint32 size = gles2::GenFramebuffersImmediate::ComputeSize(n);
    gles2::GenFramebuffersImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::GenFramebuffersImmediate>(size);
    if (c) {
      c->Init(n, framebuffers);
    }
  }

  void GenRenderbuffers(
      GLsizei n, uint32 renderbuffers_shm_id,
      uint32 renderbuffers_shm_offset) {
    gles2::GenRenderbuffers* c = GetCmdSpace<gles2::GenRenderbuffers>();
    if (c) {
      c->Init(n, renderbuffers_shm_id, renderbuffers_shm_offset);
    }
  }

  void GenRenderbuffersImmediate(GLsizei n, GLuint* renderbuffers) {
    const uint32 size = gles2::GenRenderbuffersImmediate::ComputeSize(n);
    gles2::GenRenderbuffersImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::GenRenderbuffersImmediate>(size);
    if (c) {
      c->Init(n, renderbuffers);
    }
  }

  void GenTextures(
      GLsizei n, uint32 textures_shm_id, uint32 textures_shm_offset) {
    gles2::GenTextures* c = GetCmdSpace<gles2::GenTextures>();
    if (c) {
      c->Init(n, textures_shm_id, textures_shm_offset);
    }
  }

  void GenTexturesImmediate(GLsizei n, GLuint* textures) {
    const uint32 size = gles2::GenTexturesImmediate::ComputeSize(n);
    gles2::GenTexturesImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::GenTexturesImmediate>(size);
    if (c) {
      c->Init(n, textures);
    }
  }

  void GetActiveAttrib(
      GLuint program, GLuint index, uint32 name_bucket_id, uint32 result_shm_id,
      uint32 result_shm_offset) {
    gles2::GetActiveAttrib* c = GetCmdSpace<gles2::GetActiveAttrib>();
    if (c) {
      c->Init(
          program, index, name_bucket_id, result_shm_id, result_shm_offset);
    }
  }

  void GetActiveUniform(
      GLuint program, GLuint index, uint32 name_bucket_id, uint32 result_shm_id,
      uint32 result_shm_offset) {
    gles2::GetActiveUniform* c = GetCmdSpace<gles2::GetActiveUniform>();
    if (c) {
      c->Init(
          program, index, name_bucket_id, result_shm_id, result_shm_offset);
    }
  }

  void GetAttachedShaders(
      GLuint program, uint32 result_shm_id, uint32 result_shm_offset,
      uint32 result_size) {
    gles2::GetAttachedShaders* c = GetCmdSpace<gles2::GetAttachedShaders>();
    if (c) {
      c->Init(program, result_shm_id, result_shm_offset, result_size);
    }
  }

  void GetBooleanv(
      GLenum pname, uint32 params_shm_id, uint32 params_shm_offset) {
    gles2::GetBooleanv* c = GetCmdSpace<gles2::GetBooleanv>();
    if (c) {
      c->Init(pname, params_shm_id, params_shm_offset);
    }
  }

  void GetBufferParameteriv(
      GLenum target, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::GetBufferParameteriv* c =
        GetCmdSpace<gles2::GetBufferParameteriv>();
    if (c) {
      c->Init(target, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetError(uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::GetError* c = GetCmdSpace<gles2::GetError>();
    if (c) {
      c->Init(result_shm_id, result_shm_offset);
    }
  }

  void GetFloatv(
      GLenum pname, uint32 params_shm_id, uint32 params_shm_offset) {
    gles2::GetFloatv* c = GetCmdSpace<gles2::GetFloatv>();
    if (c) {
      c->Init(pname, params_shm_id, params_shm_offset);
    }
  }

  void GetFramebufferAttachmentParameteriv(
      GLenum target, GLenum attachment, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::GetFramebufferAttachmentParameteriv* c =
        GetCmdSpace<gles2::GetFramebufferAttachmentParameteriv>();
    if (c) {
      c->Init(target, attachment, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetIntegerv(
      GLenum pname, uint32 params_shm_id, uint32 params_shm_offset) {
    gles2::GetIntegerv* c = GetCmdSpace<gles2::GetIntegerv>();
    if (c) {
      c->Init(pname, params_shm_id, params_shm_offset);
    }
  }

  void GetProgramiv(
      GLuint program, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::GetProgramiv* c = GetCmdSpace<gles2::GetProgramiv>();
    if (c) {
      c->Init(program, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetProgramInfoLog(GLuint program, uint32 bucket_id) {
    gles2::GetProgramInfoLog* c = GetCmdSpace<gles2::GetProgramInfoLog>();
    if (c) {
      c->Init(program, bucket_id);
    }
  }

  void GetRenderbufferParameteriv(
      GLenum target, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::GetRenderbufferParameteriv* c =
        GetCmdSpace<gles2::GetRenderbufferParameteriv>();
    if (c) {
      c->Init(target, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetShaderiv(
      GLuint shader, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::GetShaderiv* c = GetCmdSpace<gles2::GetShaderiv>();
    if (c) {
      c->Init(shader, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetShaderInfoLog(GLuint shader, uint32 bucket_id) {
    gles2::GetShaderInfoLog* c = GetCmdSpace<gles2::GetShaderInfoLog>();
    if (c) {
      c->Init(shader, bucket_id);
    }
  }

  void GetShaderPrecisionFormat(
      GLenum shadertype, GLenum precisiontype, uint32 result_shm_id,
      uint32 result_shm_offset) {
    gles2::GetShaderPrecisionFormat* c =
        GetCmdSpace<gles2::GetShaderPrecisionFormat>();
    if (c) {
      c->Init(shadertype, precisiontype, result_shm_id, result_shm_offset);
    }
  }

  void GetShaderSource(GLuint shader, uint32 bucket_id) {
    gles2::GetShaderSource* c = GetCmdSpace<gles2::GetShaderSource>();
    if (c) {
      c->Init(shader, bucket_id);
    }
  }

  void GetString(GLenum name, uint32 bucket_id) {
    gles2::GetString* c = GetCmdSpace<gles2::GetString>();
    if (c) {
      c->Init(name, bucket_id);
    }
  }

  void GetTexParameterfv(
      GLenum target, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::GetTexParameterfv* c = GetCmdSpace<gles2::GetTexParameterfv>();
    if (c) {
      c->Init(target, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetTexParameteriv(
      GLenum target, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::GetTexParameteriv* c = GetCmdSpace<gles2::GetTexParameteriv>();
    if (c) {
      c->Init(target, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetUniformfv(
      GLuint program, GLint location, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::GetUniformfv* c = GetCmdSpace<gles2::GetUniformfv>();
    if (c) {
      c->Init(program, location, params_shm_id, params_shm_offset);
    }
  }

  void GetUniformiv(
      GLuint program, GLint location, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::GetUniformiv* c = GetCmdSpace<gles2::GetUniformiv>();
    if (c) {
      c->Init(program, location, params_shm_id, params_shm_offset);
    }
  }

  void GetVertexAttribfv(
      GLuint index, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::GetVertexAttribfv* c = GetCmdSpace<gles2::GetVertexAttribfv>();
    if (c) {
      c->Init(index, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetVertexAttribiv(
      GLuint index, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::GetVertexAttribiv* c = GetCmdSpace<gles2::GetVertexAttribiv>();
    if (c) {
      c->Init(index, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetVertexAttribPointerv(
      GLuint index, GLenum pname, uint32 pointer_shm_id,
      uint32 pointer_shm_offset) {
    gles2::GetVertexAttribPointerv* c =
        GetCmdSpace<gles2::GetVertexAttribPointerv>();
    if (c) {
      c->Init(index, pname, pointer_shm_id, pointer_shm_offset);
    }
  }

  void Hint(GLenum target, GLenum mode) {
    gles2::Hint* c = GetCmdSpace<gles2::Hint>();
    if (c) {
      c->Init(target, mode);
    }
  }

  void IsBuffer(
      GLuint buffer, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::IsBuffer* c = GetCmdSpace<gles2::IsBuffer>();
    if (c) {
      c->Init(buffer, result_shm_id, result_shm_offset);
    }
  }

  void IsEnabled(GLenum cap, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::IsEnabled* c = GetCmdSpace<gles2::IsEnabled>();
    if (c) {
      c->Init(cap, result_shm_id, result_shm_offset);
    }
  }

  void IsFramebuffer(
      GLuint framebuffer, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::IsFramebuffer* c = GetCmdSpace<gles2::IsFramebuffer>();
    if (c) {
      c->Init(framebuffer, result_shm_id, result_shm_offset);
    }
  }

  void IsProgram(
      GLuint program, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::IsProgram* c = GetCmdSpace<gles2::IsProgram>();
    if (c) {
      c->Init(program, result_shm_id, result_shm_offset);
    }
  }

  void IsRenderbuffer(
      GLuint renderbuffer, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::IsRenderbuffer* c = GetCmdSpace<gles2::IsRenderbuffer>();
    if (c) {
      c->Init(renderbuffer, result_shm_id, result_shm_offset);
    }
  }

  void IsShader(
      GLuint shader, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::IsShader* c = GetCmdSpace<gles2::IsShader>();
    if (c) {
      c->Init(shader, result_shm_id, result_shm_offset);
    }
  }

  void IsTexture(
      GLuint texture, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::IsTexture* c = GetCmdSpace<gles2::IsTexture>();
    if (c) {
      c->Init(texture, result_shm_id, result_shm_offset);
    }
  }

  void LineWidth(GLfloat width) {
    gles2::LineWidth* c = GetCmdSpace<gles2::LineWidth>();
    if (c) {
      c->Init(width);
    }
  }

  void LinkProgram(GLuint program) {
    gles2::LinkProgram* c = GetCmdSpace<gles2::LinkProgram>();
    if (c) {
      c->Init(program);
    }
  }

  void PixelStorei(GLenum pname, GLint param) {
    gles2::PixelStorei* c = GetCmdSpace<gles2::PixelStorei>();
    if (c) {
      c->Init(pname, param);
    }
  }

  void PolygonOffset(GLfloat factor, GLfloat units) {
    gles2::PolygonOffset* c = GetCmdSpace<gles2::PolygonOffset>();
    if (c) {
      c->Init(factor, units);
    }
  }

  void ReadPixels(
      GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
      GLenum type, uint32 pixels_shm_id, uint32 pixels_shm_offset,
      uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::ReadPixels* c = GetCmdSpace<gles2::ReadPixels>();
    if (c) {
      c->Init(
          x, y, width, height, format, type, pixels_shm_id, pixels_shm_offset,
          result_shm_id, result_shm_offset);
    }
  }

  void ReleaseShaderCompiler() {
    gles2::ReleaseShaderCompiler* c =
        GetCmdSpace<gles2::ReleaseShaderCompiler>();
    if (c) {
      c->Init();
    }
  }

  void RenderbufferStorage(
      GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    gles2::RenderbufferStorage* c = GetCmdSpace<gles2::RenderbufferStorage>();
    if (c) {
      c->Init(target, internalformat, width, height);
    }
  }

  void SampleCoverage(GLclampf value, GLboolean invert) {
    gles2::SampleCoverage* c = GetCmdSpace<gles2::SampleCoverage>();
    if (c) {
      c->Init(value, invert);
    }
  }

  void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    gles2::Scissor* c = GetCmdSpace<gles2::Scissor>();
    if (c) {
      c->Init(x, y, width, height);
    }
  }

  void ShaderBinary(
      GLsizei n, uint32 shaders_shm_id, uint32 shaders_shm_offset,
      GLenum binaryformat, uint32 binary_shm_id, uint32 binary_shm_offset,
      GLsizei length) {
    gles2::ShaderBinary* c = GetCmdSpace<gles2::ShaderBinary>();
    if (c) {
      c->Init(
          n, shaders_shm_id, shaders_shm_offset, binaryformat, binary_shm_id,
          binary_shm_offset, length);
    }
  }

  void ShaderSource(
      GLuint shader, uint32 data_shm_id, uint32 data_shm_offset,
      uint32 data_size) {
    gles2::ShaderSource* c = GetCmdSpace<gles2::ShaderSource>();
    if (c) {
      c->Init(shader, data_shm_id, data_shm_offset, data_size);
    }
  }

  void ShaderSourceImmediate(GLuint shader, uint32 data_size) {
    const uint32 s = 0;  // TODO(gman): compute correct size
    gles2::ShaderSourceImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::ShaderSourceImmediate>(s);
    if (c) {
      c->Init(shader, data_size);
    }
  }

  void ShaderSourceBucket(GLuint shader, uint32 data_bucket_id) {
    gles2::ShaderSourceBucket* c = GetCmdSpace<gles2::ShaderSourceBucket>();
    if (c) {
      c->Init(shader, data_bucket_id);
    }
  }

  void StencilFunc(GLenum func, GLint ref, GLuint mask) {
    gles2::StencilFunc* c = GetCmdSpace<gles2::StencilFunc>();
    if (c) {
      c->Init(func, ref, mask);
    }
  }

  void StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    gles2::StencilFuncSeparate* c = GetCmdSpace<gles2::StencilFuncSeparate>();
    if (c) {
      c->Init(face, func, ref, mask);
    }
  }

  void StencilMask(GLuint mask) {
    gles2::StencilMask* c = GetCmdSpace<gles2::StencilMask>();
    if (c) {
      c->Init(mask);
    }
  }

  void StencilMaskSeparate(GLenum face, GLuint mask) {
    gles2::StencilMaskSeparate* c = GetCmdSpace<gles2::StencilMaskSeparate>();
    if (c) {
      c->Init(face, mask);
    }
  }

  void StencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    gles2::StencilOp* c = GetCmdSpace<gles2::StencilOp>();
    if (c) {
      c->Init(fail, zfail, zpass);
    }
  }

  void StencilOpSeparate(
      GLenum face, GLenum fail, GLenum zfail, GLenum zpass) {
    gles2::StencilOpSeparate* c = GetCmdSpace<gles2::StencilOpSeparate>();
    if (c) {
      c->Init(face, fail, zfail, zpass);
    }
  }

  void TexImage2D(
      GLenum target, GLint level, GLint internalformat, GLsizei width,
      GLsizei height, GLint border, GLenum format, GLenum type,
      uint32 pixels_shm_id, uint32 pixels_shm_offset) {
    gles2::TexImage2D* c = GetCmdSpace<gles2::TexImage2D>();
    if (c) {
      c->Init(
          target, level, internalformat, width, height, border, format, type,
          pixels_shm_id, pixels_shm_offset);
    }
  }

  void TexImage2DImmediate(
      GLenum target, GLint level, GLint internalformat, GLsizei width,
      GLsizei height, GLint border, GLenum format, GLenum type) {
    const uint32 s = 0;  // TODO(gman): compute correct size
    gles2::TexImage2DImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::TexImage2DImmediate>(s);
    if (c) {
      c->Init(
          target, level, internalformat, width, height, border, format, type);
    }
  }

  void TexParameterf(GLenum target, GLenum pname, GLfloat param) {
    gles2::TexParameterf* c = GetCmdSpace<gles2::TexParameterf>();
    if (c) {
      c->Init(target, pname, param);
    }
  }

  void TexParameterfv(
      GLenum target, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::TexParameterfv* c = GetCmdSpace<gles2::TexParameterfv>();
    if (c) {
      c->Init(target, pname, params_shm_id, params_shm_offset);
    }
  }

  void TexParameterfvImmediate(
      GLenum target, GLenum pname, const GLfloat* params) {
    const uint32 size = gles2::TexParameterfvImmediate::ComputeSize();
    gles2::TexParameterfvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::TexParameterfvImmediate>(size);
    if (c) {
      c->Init(target, pname, params);
    }
  }

  void TexParameteri(GLenum target, GLenum pname, GLint param) {
    gles2::TexParameteri* c = GetCmdSpace<gles2::TexParameteri>();
    if (c) {
      c->Init(target, pname, param);
    }
  }

  void TexParameteriv(
      GLenum target, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::TexParameteriv* c = GetCmdSpace<gles2::TexParameteriv>();
    if (c) {
      c->Init(target, pname, params_shm_id, params_shm_offset);
    }
  }

  void TexParameterivImmediate(
      GLenum target, GLenum pname, const GLint* params) {
    const uint32 size = gles2::TexParameterivImmediate::ComputeSize();
    gles2::TexParameterivImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::TexParameterivImmediate>(size);
    if (c) {
      c->Init(target, pname, params);
    }
  }

  void TexSubImage2D(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLenum type, uint32 pixels_shm_id,
      uint32 pixels_shm_offset, GLboolean internal) {
    gles2::TexSubImage2D* c = GetCmdSpace<gles2::TexSubImage2D>();
    if (c) {
      c->Init(
          target, level, xoffset, yoffset, width, height, format, type,
          pixels_shm_id, pixels_shm_offset, internal);
    }
  }

  void TexSubImage2DImmediate(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLenum type, GLboolean internal) {
    const uint32 s = 0;  // TODO(gman): compute correct size
    gles2::TexSubImage2DImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::TexSubImage2DImmediate>(s);
    if (c) {
      c->Init(
          target, level, xoffset, yoffset, width, height, format, type,
          internal);
    }
  }

  void Uniform1f(GLint location, GLfloat x) {
    gles2::Uniform1f* c = GetCmdSpace<gles2::Uniform1f>();
    if (c) {
      c->Init(location, x);
    }
  }

  void Uniform1fv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::Uniform1fv* c = GetCmdSpace<gles2::Uniform1fv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform1fvImmediate(GLint location, GLsizei count, const GLfloat* v) {
    const uint32 size = gles2::Uniform1fvImmediate::ComputeSize(count);
    gles2::Uniform1fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::Uniform1fvImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform1i(GLint location, GLint x) {
    gles2::Uniform1i* c = GetCmdSpace<gles2::Uniform1i>();
    if (c) {
      c->Init(location, x);
    }
  }

  void Uniform1iv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::Uniform1iv* c = GetCmdSpace<gles2::Uniform1iv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform1ivImmediate(GLint location, GLsizei count, const GLint* v) {
    const uint32 size = gles2::Uniform1ivImmediate::ComputeSize(count);
    gles2::Uniform1ivImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::Uniform1ivImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform2f(GLint location, GLfloat x, GLfloat y) {
    gles2::Uniform2f* c = GetCmdSpace<gles2::Uniform2f>();
    if (c) {
      c->Init(location, x, y);
    }
  }

  void Uniform2fv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::Uniform2fv* c = GetCmdSpace<gles2::Uniform2fv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform2fvImmediate(GLint location, GLsizei count, const GLfloat* v) {
    const uint32 size = gles2::Uniform2fvImmediate::ComputeSize(count);
    gles2::Uniform2fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::Uniform2fvImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform2i(GLint location, GLint x, GLint y) {
    gles2::Uniform2i* c = GetCmdSpace<gles2::Uniform2i>();
    if (c) {
      c->Init(location, x, y);
    }
  }

  void Uniform2iv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::Uniform2iv* c = GetCmdSpace<gles2::Uniform2iv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform2ivImmediate(GLint location, GLsizei count, const GLint* v) {
    const uint32 size = gles2::Uniform2ivImmediate::ComputeSize(count);
    gles2::Uniform2ivImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::Uniform2ivImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
    gles2::Uniform3f* c = GetCmdSpace<gles2::Uniform3f>();
    if (c) {
      c->Init(location, x, y, z);
    }
  }

  void Uniform3fv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::Uniform3fv* c = GetCmdSpace<gles2::Uniform3fv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform3fvImmediate(GLint location, GLsizei count, const GLfloat* v) {
    const uint32 size = gles2::Uniform3fvImmediate::ComputeSize(count);
    gles2::Uniform3fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::Uniform3fvImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform3i(GLint location, GLint x, GLint y, GLint z) {
    gles2::Uniform3i* c = GetCmdSpace<gles2::Uniform3i>();
    if (c) {
      c->Init(location, x, y, z);
    }
  }

  void Uniform3iv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::Uniform3iv* c = GetCmdSpace<gles2::Uniform3iv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform3ivImmediate(GLint location, GLsizei count, const GLint* v) {
    const uint32 size = gles2::Uniform3ivImmediate::ComputeSize(count);
    gles2::Uniform3ivImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::Uniform3ivImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    gles2::Uniform4f* c = GetCmdSpace<gles2::Uniform4f>();
    if (c) {
      c->Init(location, x, y, z, w);
    }
  }

  void Uniform4fv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::Uniform4fv* c = GetCmdSpace<gles2::Uniform4fv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform4fvImmediate(GLint location, GLsizei count, const GLfloat* v) {
    const uint32 size = gles2::Uniform4fvImmediate::ComputeSize(count);
    gles2::Uniform4fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::Uniform4fvImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
    gles2::Uniform4i* c = GetCmdSpace<gles2::Uniform4i>();
    if (c) {
      c->Init(location, x, y, z, w);
    }
  }

  void Uniform4iv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::Uniform4iv* c = GetCmdSpace<gles2::Uniform4iv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform4ivImmediate(GLint location, GLsizei count, const GLint* v) {
    const uint32 size = gles2::Uniform4ivImmediate::ComputeSize(count);
    gles2::Uniform4ivImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::Uniform4ivImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void UniformMatrix2fv(
      GLint location, GLsizei count, GLboolean transpose, uint32 value_shm_id,
      uint32 value_shm_offset) {
    gles2::UniformMatrix2fv* c = GetCmdSpace<gles2::UniformMatrix2fv>();
    if (c) {
      c->Init(location, count, transpose, value_shm_id, value_shm_offset);
    }
  }

  void UniformMatrix2fvImmediate(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value) {
    const uint32 size = gles2::UniformMatrix2fvImmediate::ComputeSize(count);
    gles2::UniformMatrix2fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::UniformMatrix2fvImmediate>(size);
    if (c) {
      c->Init(location, count, transpose, value);
    }
  }

  void UniformMatrix3fv(
      GLint location, GLsizei count, GLboolean transpose, uint32 value_shm_id,
      uint32 value_shm_offset) {
    gles2::UniformMatrix3fv* c = GetCmdSpace<gles2::UniformMatrix3fv>();
    if (c) {
      c->Init(location, count, transpose, value_shm_id, value_shm_offset);
    }
  }

  void UniformMatrix3fvImmediate(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value) {
    const uint32 size = gles2::UniformMatrix3fvImmediate::ComputeSize(count);
    gles2::UniformMatrix3fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::UniformMatrix3fvImmediate>(size);
    if (c) {
      c->Init(location, count, transpose, value);
    }
  }

  void UniformMatrix4fv(
      GLint location, GLsizei count, GLboolean transpose, uint32 value_shm_id,
      uint32 value_shm_offset) {
    gles2::UniformMatrix4fv* c = GetCmdSpace<gles2::UniformMatrix4fv>();
    if (c) {
      c->Init(location, count, transpose, value_shm_id, value_shm_offset);
    }
  }

  void UniformMatrix4fvImmediate(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value) {
    const uint32 size = gles2::UniformMatrix4fvImmediate::ComputeSize(count);
    gles2::UniformMatrix4fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::UniformMatrix4fvImmediate>(size);
    if (c) {
      c->Init(location, count, transpose, value);
    }
  }

  void UseProgram(GLuint program) {
    gles2::UseProgram* c = GetCmdSpace<gles2::UseProgram>();
    if (c) {
      c->Init(program);
    }
  }

  void ValidateProgram(GLuint program) {
    gles2::ValidateProgram* c = GetCmdSpace<gles2::ValidateProgram>();
    if (c) {
      c->Init(program);
    }
  }

  void VertexAttrib1f(GLuint indx, GLfloat x) {
    gles2::VertexAttrib1f* c = GetCmdSpace<gles2::VertexAttrib1f>();
    if (c) {
      c->Init(indx, x);
    }
  }

  void VertexAttrib1fv(
      GLuint indx, uint32 values_shm_id, uint32 values_shm_offset) {
    gles2::VertexAttrib1fv* c = GetCmdSpace<gles2::VertexAttrib1fv>();
    if (c) {
      c->Init(indx, values_shm_id, values_shm_offset);
    }
  }

  void VertexAttrib1fvImmediate(GLuint indx, const GLfloat* values) {
    const uint32 size = gles2::VertexAttrib1fvImmediate::ComputeSize();
    gles2::VertexAttrib1fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::VertexAttrib1fvImmediate>(size);
    if (c) {
      c->Init(indx, values);
    }
  }

  void VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
    gles2::VertexAttrib2f* c = GetCmdSpace<gles2::VertexAttrib2f>();
    if (c) {
      c->Init(indx, x, y);
    }
  }

  void VertexAttrib2fv(
      GLuint indx, uint32 values_shm_id, uint32 values_shm_offset) {
    gles2::VertexAttrib2fv* c = GetCmdSpace<gles2::VertexAttrib2fv>();
    if (c) {
      c->Init(indx, values_shm_id, values_shm_offset);
    }
  }

  void VertexAttrib2fvImmediate(GLuint indx, const GLfloat* values) {
    const uint32 size = gles2::VertexAttrib2fvImmediate::ComputeSize();
    gles2::VertexAttrib2fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::VertexAttrib2fvImmediate>(size);
    if (c) {
      c->Init(indx, values);
    }
  }

  void VertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
    gles2::VertexAttrib3f* c = GetCmdSpace<gles2::VertexAttrib3f>();
    if (c) {
      c->Init(indx, x, y, z);
    }
  }

  void VertexAttrib3fv(
      GLuint indx, uint32 values_shm_id, uint32 values_shm_offset) {
    gles2::VertexAttrib3fv* c = GetCmdSpace<gles2::VertexAttrib3fv>();
    if (c) {
      c->Init(indx, values_shm_id, values_shm_offset);
    }
  }

  void VertexAttrib3fvImmediate(GLuint indx, const GLfloat* values) {
    const uint32 size = gles2::VertexAttrib3fvImmediate::ComputeSize();
    gles2::VertexAttrib3fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::VertexAttrib3fvImmediate>(size);
    if (c) {
      c->Init(indx, values);
    }
  }

  void VertexAttrib4f(
      GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    gles2::VertexAttrib4f* c = GetCmdSpace<gles2::VertexAttrib4f>();
    if (c) {
      c->Init(indx, x, y, z, w);
    }
  }

  void VertexAttrib4fv(
      GLuint indx, uint32 values_shm_id, uint32 values_shm_offset) {
    gles2::VertexAttrib4fv* c = GetCmdSpace<gles2::VertexAttrib4fv>();
    if (c) {
      c->Init(indx, values_shm_id, values_shm_offset);
    }
  }

  void VertexAttrib4fvImmediate(GLuint indx, const GLfloat* values) {
    const uint32 size = gles2::VertexAttrib4fvImmediate::ComputeSize();
    gles2::VertexAttrib4fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::VertexAttrib4fvImmediate>(size);
    if (c) {
      c->Init(indx, values);
    }
  }

  void VertexAttribPointer(
      GLuint indx, GLint size, GLenum type, GLboolean normalized,
      GLsizei stride, GLuint offset) {
    gles2::VertexAttribPointer* c = GetCmdSpace<gles2::VertexAttribPointer>();
    if (c) {
      c->Init(indx, size, type, normalized, stride, offset);
    }
  }

  void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    gles2::Viewport* c = GetCmdSpace<gles2::Viewport>();
    if (c) {
      c->Init(x, y, width, height);
    }
  }

  void BlitFramebufferEXT(
      GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
      GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    gles2::BlitFramebufferEXT* c = GetCmdSpace<gles2::BlitFramebufferEXT>();
    if (c) {
      c->Init(
          srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask,
          filter);
    }
  }

  void RenderbufferStorageMultisampleEXT(
      GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
      GLsizei height) {
    gles2::RenderbufferStorageMultisampleEXT* c =
        GetCmdSpace<gles2::RenderbufferStorageMultisampleEXT>();
    if (c) {
      c->Init(target, samples, internalformat, width, height);
    }
  }

  void TexStorage2DEXT(
      GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width,
      GLsizei height) {
    gles2::TexStorage2DEXT* c = GetCmdSpace<gles2::TexStorage2DEXT>();
    if (c) {
      c->Init(target, levels, internalFormat, width, height);
    }
  }

  void GenQueriesEXT(
      GLsizei n, uint32 queries_shm_id, uint32 queries_shm_offset) {
    gles2::GenQueriesEXT* c = GetCmdSpace<gles2::GenQueriesEXT>();
    if (c) {
      c->Init(n, queries_shm_id, queries_shm_offset);
    }
  }

  void GenQueriesEXTImmediate(GLsizei n, GLuint* queries) {
    const uint32 size = gles2::GenQueriesEXTImmediate::ComputeSize(n);
    gles2::GenQueriesEXTImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::GenQueriesEXTImmediate>(size);
    if (c) {
      c->Init(n, queries);
    }
  }

  void DeleteQueriesEXT(
      GLsizei n, uint32 queries_shm_id, uint32 queries_shm_offset) {
    gles2::DeleteQueriesEXT* c = GetCmdSpace<gles2::DeleteQueriesEXT>();
    if (c) {
      c->Init(n, queries_shm_id, queries_shm_offset);
    }
  }

  void DeleteQueriesEXTImmediate(GLsizei n, const GLuint* queries) {
    const uint32 size = gles2::DeleteQueriesEXTImmediate::ComputeSize(n);
    gles2::DeleteQueriesEXTImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::DeleteQueriesEXTImmediate>(size);
    if (c) {
      c->Init(n, queries);
    }
  }

  void BeginQueryEXT(
      GLenum target, GLuint id, uint32 sync_data_shm_id,
      uint32 sync_data_shm_offset) {
    gles2::BeginQueryEXT* c = GetCmdSpace<gles2::BeginQueryEXT>();
    if (c) {
      c->Init(target, id, sync_data_shm_id, sync_data_shm_offset);
    }
  }

  void EndQueryEXT(GLenum target, GLuint submit_count) {
    gles2::EndQueryEXT* c = GetCmdSpace<gles2::EndQueryEXT>();
    if (c) {
      c->Init(target, submit_count);
    }
  }

  void SwapBuffers() {
    gles2::SwapBuffers* c = GetCmdSpace<gles2::SwapBuffers>();
    if (c) {
      c->Init();
    }
  }

  void GetMaxValueInBufferCHROMIUM(
      GLuint buffer_id, GLsizei count, GLenum type, GLuint offset,
      uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::GetMaxValueInBufferCHROMIUM* c =
        GetCmdSpace<gles2::GetMaxValueInBufferCHROMIUM>();
    if (c) {
      c->Init(
          buffer_id, count, type, offset, result_shm_id, result_shm_offset);
    }
  }

  void GenSharedIdsCHROMIUM(
      GLuint namespace_id, GLuint id_offset, GLsizei n, uint32 ids_shm_id,
      uint32 ids_shm_offset) {
    gles2::GenSharedIdsCHROMIUM* c =
        GetCmdSpace<gles2::GenSharedIdsCHROMIUM>();
    if (c) {
      c->Init(namespace_id, id_offset, n, ids_shm_id, ids_shm_offset);
    }
  }

  void DeleteSharedIdsCHROMIUM(
      GLuint namespace_id, GLsizei n, uint32 ids_shm_id,
      uint32 ids_shm_offset) {
    gles2::DeleteSharedIdsCHROMIUM* c =
        GetCmdSpace<gles2::DeleteSharedIdsCHROMIUM>();
    if (c) {
      c->Init(namespace_id, n, ids_shm_id, ids_shm_offset);
    }
  }

  void RegisterSharedIdsCHROMIUM(
      GLuint namespace_id, GLsizei n, uint32 ids_shm_id,
      uint32 ids_shm_offset) {
    gles2::RegisterSharedIdsCHROMIUM* c =
        GetCmdSpace<gles2::RegisterSharedIdsCHROMIUM>();
    if (c) {
      c->Init(namespace_id, n, ids_shm_id, ids_shm_offset);
    }
  }

  void EnableFeatureCHROMIUM(
      GLuint bucket_id, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::EnableFeatureCHROMIUM* c =
        GetCmdSpace<gles2::EnableFeatureCHROMIUM>();
    if (c) {
      c->Init(bucket_id, result_shm_id, result_shm_offset);
    }
  }

  void ResizeCHROMIUM(GLuint width, GLuint height) {
    gles2::ResizeCHROMIUM* c = GetCmdSpace<gles2::ResizeCHROMIUM>();
    if (c) {
      c->Init(width, height);
    }
  }

  void GetRequestableExtensionsCHROMIUM(uint32 bucket_id) {
    gles2::GetRequestableExtensionsCHROMIUM* c =
        GetCmdSpace<gles2::GetRequestableExtensionsCHROMIUM>();
    if (c) {
      c->Init(bucket_id);
    }
  }

  void RequestExtensionCHROMIUM(uint32 bucket_id) {
    gles2::RequestExtensionCHROMIUM* c =
        GetCmdSpace<gles2::RequestExtensionCHROMIUM>();
    if (c) {
      c->Init(bucket_id);
    }
  }

  void GetMultipleIntegervCHROMIUM(
      uint32 pnames_shm_id, uint32 pnames_shm_offset, GLuint count,
      uint32 results_shm_id, uint32 results_shm_offset, GLsizeiptr size) {
    gles2::GetMultipleIntegervCHROMIUM* c =
        GetCmdSpace<gles2::GetMultipleIntegervCHROMIUM>();
    if (c) {
      c->Init(
          pnames_shm_id, pnames_shm_offset, count, results_shm_id,
          results_shm_offset, size);
    }
  }

  void GetProgramInfoCHROMIUM(GLuint program, uint32 bucket_id) {
    gles2::GetProgramInfoCHROMIUM* c =
        GetCmdSpace<gles2::GetProgramInfoCHROMIUM>();
    if (c) {
      c->Init(program, bucket_id);
    }
  }

  void CreateStreamTextureCHROMIUM(
      GLuint client_id, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::CreateStreamTextureCHROMIUM* c =
        GetCmdSpace<gles2::CreateStreamTextureCHROMIUM>();
    if (c) {
      c->Init(client_id, result_shm_id, result_shm_offset);
    }
  }

  void DestroyStreamTextureCHROMIUM(GLuint texture) {
    gles2::DestroyStreamTextureCHROMIUM* c =
        GetCmdSpace<gles2::DestroyStreamTextureCHROMIUM>();
    if (c) {
      c->Init(texture);
    }
  }

  void GetTranslatedShaderSourceANGLE(GLuint shader, uint32 bucket_id) {
    gles2::GetTranslatedShaderSourceANGLE* c =
        GetCmdSpace<gles2::GetTranslatedShaderSourceANGLE>();
    if (c) {
      c->Init(shader, bucket_id);
    }
  }

  void PostSubBufferCHROMIUM(GLint x, GLint y, GLint width, GLint height) {
    gles2::PostSubBufferCHROMIUM* c =
        GetCmdSpace<gles2::PostSubBufferCHROMIUM>();
    if (c) {
      c->Init(x, y, width, height);
    }
  }

  void TexImageIOSurface2DCHROMIUM(
      GLenum target, GLsizei width, GLsizei height, GLuint ioSurfaceId,
      GLuint plane) {
    gles2::TexImageIOSurface2DCHROMIUM* c =
        GetCmdSpace<gles2::TexImageIOSurface2DCHROMIUM>();
    if (c) {
      c->Init(target, width, height, ioSurfaceId, plane);
    }
  }

  void CopyTextureCHROMIUM(
      GLenum target, GLenum source_id, GLenum dest_id, GLint level,
      GLint internalformat) {
    gles2::CopyTextureCHROMIUM* c = GetCmdSpace<gles2::CopyTextureCHROMIUM>();
    if (c) {
      c->Init(target, source_id, dest_id, level, internalformat);
    }
  }

  void DrawArraysInstancedANGLE(
      GLenum mode, GLint first, GLsizei count, GLsizei primcount) {
    gles2::DrawArraysInstancedANGLE* c =
        GetCmdSpace<gles2::DrawArraysInstancedANGLE>();
    if (c) {
      c->Init(mode, first, count, primcount);
    }
  }

  void DrawElementsInstancedANGLE(
      GLenum mode, GLsizei count, GLenum type, GLuint index_offset,
      GLsizei primcount) {
    gles2::DrawElementsInstancedANGLE* c =
        GetCmdSpace<gles2::DrawElementsInstancedANGLE>();
    if (c) {
      c->Init(mode, count, type, index_offset, primcount);
    }
  }

  void VertexAttribDivisorANGLE(GLuint index, GLuint divisor) {
    gles2::VertexAttribDivisorANGLE* c =
        GetCmdSpace<gles2::VertexAttribDivisorANGLE>();
    if (c) {
      c->Init(index, divisor);
    }
  }

  void GenMailboxCHROMIUM(GLuint bucket_id) {
    gles2::GenMailboxCHROMIUM* c = GetCmdSpace<gles2::GenMailboxCHROMIUM>();
    if (c) {
      c->Init(bucket_id);
    }
  }

  void ProduceTextureCHROMIUM(
      GLenum target, uint32 mailbox_shm_id, uint32 mailbox_shm_offset) {
    gles2::ProduceTextureCHROMIUM* c =
        GetCmdSpace<gles2::ProduceTextureCHROMIUM>();
    if (c) {
      c->Init(target, mailbox_shm_id, mailbox_shm_offset);
    }
  }

  void ProduceTextureCHROMIUMImmediate(GLenum target, const GLbyte* mailbox) {
    const uint32 size = gles2::ProduceTextureCHROMIUMImmediate::ComputeSize();
    gles2::ProduceTextureCHROMIUMImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::ProduceTextureCHROMIUMImmediate>(
            size);
    if (c) {
      c->Init(target, mailbox);
    }
  }

  void ConsumeTextureCHROMIUM(
      GLenum target, uint32 mailbox_shm_id, uint32 mailbox_shm_offset) {
    gles2::ConsumeTextureCHROMIUM* c =
        GetCmdSpace<gles2::ConsumeTextureCHROMIUM>();
    if (c) {
      c->Init(target, mailbox_shm_id, mailbox_shm_offset);
    }
  }

  void ConsumeTextureCHROMIUMImmediate(GLenum target, const GLbyte* mailbox) {
    const uint32 size = gles2::ConsumeTextureCHROMIUMImmediate::ComputeSize();
    gles2::ConsumeTextureCHROMIUMImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::ConsumeTextureCHROMIUMImmediate>(
            size);
    if (c) {
      c->Init(target, mailbox);
    }
  }

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_AUTOGEN_H_

