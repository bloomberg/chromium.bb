// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_AUTOGEN_H_

  void ActiveTexture(GLenum texture) {
    gles2::cmds::ActiveTexture* c = GetCmdSpace<gles2::cmds::ActiveTexture>();
    if (c) {
      c->Init(texture);
    }
  }

  void AttachShader(GLuint program, GLuint shader) {
    gles2::cmds::AttachShader* c = GetCmdSpace<gles2::cmds::AttachShader>();
    if (c) {
      c->Init(program, shader);
    }
  }

  void BindAttribLocation(
      GLuint program, GLuint index, uint32 name_shm_id, uint32 name_shm_offset,
      uint32 data_size) {
    gles2::cmds::BindAttribLocation* c =
        GetCmdSpace<gles2::cmds::BindAttribLocation>();
    if (c) {
      c->Init(program, index, name_shm_id, name_shm_offset, data_size);
    }
  }

  void BindAttribLocationImmediate(
      GLuint program, GLuint index, const char* name) {
    const uint32 data_size = strlen(name);
    gles2::cmds::BindAttribLocationImmediate* c =
        GetImmediateCmdSpace<gles2::cmds::BindAttribLocationImmediate>(
            data_size);
    if (c) {
      c->Init(program, index, name, data_size);
    }
  }

  void BindAttribLocationBucket(
      GLuint program, GLuint index, uint32 name_bucket_id) {
    gles2::cmds::BindAttribLocationBucket* c =
        GetCmdSpace<gles2::cmds::BindAttribLocationBucket>();
    if (c) {
      c->Init(program, index, name_bucket_id);
    }
  }

  void BindBuffer(GLenum target, GLuint buffer) {
    gles2::cmds::BindBuffer* c = GetCmdSpace<gles2::cmds::BindBuffer>();
    if (c) {
      c->Init(target, buffer);
    }
  }

  void BindFramebuffer(GLenum target, GLuint framebuffer) {
    gles2::cmds::BindFramebuffer* c =
        GetCmdSpace<gles2::cmds::BindFramebuffer>();
    if (c) {
      c->Init(target, framebuffer);
    }
  }

  void BindRenderbuffer(GLenum target, GLuint renderbuffer) {
    gles2::cmds::BindRenderbuffer* c =
        GetCmdSpace<gles2::cmds::BindRenderbuffer>();
    if (c) {
      c->Init(target, renderbuffer);
    }
  }

  void BindTexture(GLenum target, GLuint texture) {
    gles2::cmds::BindTexture* c = GetCmdSpace<gles2::cmds::BindTexture>();
    if (c) {
      c->Init(target, texture);
    }
  }

  void BlendColor(
      GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    gles2::cmds::BlendColor* c = GetCmdSpace<gles2::cmds::BlendColor>();
    if (c) {
      c->Init(red, green, blue, alpha);
    }
  }

  void BlendEquation(GLenum mode) {
    gles2::cmds::BlendEquation* c = GetCmdSpace<gles2::cmds::BlendEquation>();
    if (c) {
      c->Init(mode);
    }
  }

  void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    gles2::cmds::BlendEquationSeparate* c =
        GetCmdSpace<gles2::cmds::BlendEquationSeparate>();
    if (c) {
      c->Init(modeRGB, modeAlpha);
    }
  }

  void BlendFunc(GLenum sfactor, GLenum dfactor) {
    gles2::cmds::BlendFunc* c = GetCmdSpace<gles2::cmds::BlendFunc>();
    if (c) {
      c->Init(sfactor, dfactor);
    }
  }

  void BlendFuncSeparate(
      GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    gles2::cmds::BlendFuncSeparate* c =
        GetCmdSpace<gles2::cmds::BlendFuncSeparate>();
    if (c) {
      c->Init(srcRGB, dstRGB, srcAlpha, dstAlpha);
    }
  }

  void BufferData(
      GLenum target, GLsizeiptr size, uint32 data_shm_id,
      uint32 data_shm_offset, GLenum usage) {
    gles2::cmds::BufferData* c = GetCmdSpace<gles2::cmds::BufferData>();
    if (c) {
      c->Init(target, size, data_shm_id, data_shm_offset, usage);
    }
  }

  void BufferDataImmediate(GLenum target, GLsizeiptr size, GLenum usage) {
    const uint32 s = 0;  // TODO(gman): compute correct size
    gles2::cmds::BufferDataImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::BufferDataImmediate>(s);
    if (c) {
      c->Init(target, size, usage);
    }
  }

  void BufferSubData(
      GLenum target, GLintptr offset, GLsizeiptr size, uint32 data_shm_id,
      uint32 data_shm_offset) {
    gles2::cmds::BufferSubData* c = GetCmdSpace<gles2::cmds::BufferSubData>();
    if (c) {
      c->Init(target, offset, size, data_shm_id, data_shm_offset);
    }
  }

  void BufferSubDataImmediate(
      GLenum target, GLintptr offset, GLsizeiptr size) {
    const uint32 s = 0;  // TODO(gman): compute correct size
    gles2::cmds::BufferSubDataImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::BufferSubDataImmediate>(s);
    if (c) {
      c->Init(target, offset, size);
    }
  }

  void CheckFramebufferStatus(
      GLenum target, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::CheckFramebufferStatus* c =
        GetCmdSpace<gles2::cmds::CheckFramebufferStatus>();
    if (c) {
      c->Init(target, result_shm_id, result_shm_offset);
    }
  }

  void Clear(GLbitfield mask) {
    gles2::cmds::Clear* c = GetCmdSpace<gles2::cmds::Clear>();
    if (c) {
      c->Init(mask);
    }
  }

  void ClearColor(
      GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    gles2::cmds::ClearColor* c = GetCmdSpace<gles2::cmds::ClearColor>();
    if (c) {
      c->Init(red, green, blue, alpha);
    }
  }

  void ClearDepthf(GLclampf depth) {
    gles2::cmds::ClearDepthf* c = GetCmdSpace<gles2::cmds::ClearDepthf>();
    if (c) {
      c->Init(depth);
    }
  }

  void ClearStencil(GLint s) {
    gles2::cmds::ClearStencil* c = GetCmdSpace<gles2::cmds::ClearStencil>();
    if (c) {
      c->Init(s);
    }
  }

  void ColorMask(
      GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    gles2::cmds::ColorMask* c = GetCmdSpace<gles2::cmds::ColorMask>();
    if (c) {
      c->Init(red, green, blue, alpha);
    }
  }

  void CompileShader(GLuint shader) {
    gles2::cmds::CompileShader* c = GetCmdSpace<gles2::cmds::CompileShader>();
    if (c) {
      c->Init(shader);
    }
  }

  void CompressedTexImage2D(
      GLenum target, GLint level, GLenum internalformat, GLsizei width,
      GLsizei height, GLint border, GLsizei imageSize, uint32 data_shm_id,
      uint32 data_shm_offset) {
    gles2::cmds::CompressedTexImage2D* c =
        GetCmdSpace<gles2::cmds::CompressedTexImage2D>();
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
    gles2::cmds::CompressedTexImage2DImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::CompressedTexImage2DImmediate>(s);  // NOLINT
    if (c) {
      c->Init(target, level, internalformat, width, height, border, imageSize);
    }
  }

  void CompressedTexImage2DBucket(
      GLenum target, GLint level, GLenum internalformat, GLsizei width,
      GLsizei height, GLint border, GLuint bucket_id) {
    gles2::cmds::CompressedTexImage2DBucket* c =
        GetCmdSpace<gles2::cmds::CompressedTexImage2DBucket>();
    if (c) {
      c->Init(target, level, internalformat, width, height, border, bucket_id);
    }
  }

  void CompressedTexSubImage2D(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLsizei imageSize, uint32 data_shm_id,
      uint32 data_shm_offset) {
    gles2::cmds::CompressedTexSubImage2D* c =
        GetCmdSpace<gles2::cmds::CompressedTexSubImage2D>();
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
    gles2::cmds::CompressedTexSubImage2DImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::CompressedTexSubImage2DImmediate>(s);  // NOLINT
    if (c) {
      c->Init(
          target, level, xoffset, yoffset, width, height, format, imageSize);
    }
  }

  void CompressedTexSubImage2DBucket(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLuint bucket_id) {
    gles2::cmds::CompressedTexSubImage2DBucket* c =
        GetCmdSpace<gles2::cmds::CompressedTexSubImage2DBucket>();
    if (c) {
      c->Init(
          target, level, xoffset, yoffset, width, height, format, bucket_id);
    }
  }

  void CopyTexImage2D(
      GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
      GLsizei width, GLsizei height, GLint border) {
    gles2::cmds::CopyTexImage2D* c =
        GetCmdSpace<gles2::cmds::CopyTexImage2D>();
    if (c) {
      c->Init(target, level, internalformat, x, y, width, height, border);
    }
  }

  void CopyTexSubImage2D(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x,
      GLint y, GLsizei width, GLsizei height) {
    gles2::cmds::CopyTexSubImage2D* c =
        GetCmdSpace<gles2::cmds::CopyTexSubImage2D>();
    if (c) {
      c->Init(target, level, xoffset, yoffset, x, y, width, height);
    }
  }

  void CreateProgram(uint32 client_id) {
    gles2::cmds::CreateProgram* c = GetCmdSpace<gles2::cmds::CreateProgram>();
    if (c) {
      c->Init(client_id);
    }
  }

  void CreateShader(GLenum type, uint32 client_id) {
    gles2::cmds::CreateShader* c = GetCmdSpace<gles2::cmds::CreateShader>();
    if (c) {
      c->Init(type, client_id);
    }
  }

  void CullFace(GLenum mode) {
    gles2::cmds::CullFace* c = GetCmdSpace<gles2::cmds::CullFace>();
    if (c) {
      c->Init(mode);
    }
  }

  void DeleteBuffers(
      GLsizei n, uint32 buffers_shm_id, uint32 buffers_shm_offset) {
    gles2::cmds::DeleteBuffers* c = GetCmdSpace<gles2::cmds::DeleteBuffers>();
    if (c) {
      c->Init(n, buffers_shm_id, buffers_shm_offset);
    }
  }

  void DeleteBuffersImmediate(GLsizei n, const GLuint* buffers) {
    const uint32 size = gles2::cmds::DeleteBuffersImmediate::ComputeSize(n);
    gles2::cmds::DeleteBuffersImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::DeleteBuffersImmediate>(
            size);
    if (c) {
      c->Init(n, buffers);
    }
  }

  void DeleteFramebuffers(
      GLsizei n, uint32 framebuffers_shm_id, uint32 framebuffers_shm_offset) {
    gles2::cmds::DeleteFramebuffers* c =
        GetCmdSpace<gles2::cmds::DeleteFramebuffers>();
    if (c) {
      c->Init(n, framebuffers_shm_id, framebuffers_shm_offset);
    }
  }

  void DeleteFramebuffersImmediate(GLsizei n, const GLuint* framebuffers) {
    const uint32 size =
        gles2::cmds::DeleteFramebuffersImmediate::ComputeSize(n);
    gles2::cmds::DeleteFramebuffersImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::DeleteFramebuffersImmediate>(
            size);
    if (c) {
      c->Init(n, framebuffers);
    }
  }

  void DeleteProgram(GLuint program) {
    gles2::cmds::DeleteProgram* c = GetCmdSpace<gles2::cmds::DeleteProgram>();
    if (c) {
      c->Init(program);
    }
  }

  void DeleteRenderbuffers(
      GLsizei n, uint32 renderbuffers_shm_id,
      uint32 renderbuffers_shm_offset) {
    gles2::cmds::DeleteRenderbuffers* c =
        GetCmdSpace<gles2::cmds::DeleteRenderbuffers>();
    if (c) {
      c->Init(n, renderbuffers_shm_id, renderbuffers_shm_offset);
    }
  }

  void DeleteRenderbuffersImmediate(GLsizei n, const GLuint* renderbuffers) {
    const uint32 size =
        gles2::cmds::DeleteRenderbuffersImmediate::ComputeSize(n);
    gles2::cmds::DeleteRenderbuffersImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::DeleteRenderbuffersImmediate>(size);  // NOLINT
    if (c) {
      c->Init(n, renderbuffers);
    }
  }

  void DeleteShader(GLuint shader) {
    gles2::cmds::DeleteShader* c = GetCmdSpace<gles2::cmds::DeleteShader>();
    if (c) {
      c->Init(shader);
    }
  }

  void DeleteTextures(
      GLsizei n, uint32 textures_shm_id, uint32 textures_shm_offset) {
    gles2::cmds::DeleteTextures* c =
        GetCmdSpace<gles2::cmds::DeleteTextures>();
    if (c) {
      c->Init(n, textures_shm_id, textures_shm_offset);
    }
  }

  void DeleteTexturesImmediate(GLsizei n, const GLuint* textures) {
    const uint32 size = gles2::cmds::DeleteTexturesImmediate::ComputeSize(n);
    gles2::cmds::DeleteTexturesImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::DeleteTexturesImmediate>(
            size);
    if (c) {
      c->Init(n, textures);
    }
  }

  void DepthFunc(GLenum func) {
    gles2::cmds::DepthFunc* c = GetCmdSpace<gles2::cmds::DepthFunc>();
    if (c) {
      c->Init(func);
    }
  }

  void DepthMask(GLboolean flag) {
    gles2::cmds::DepthMask* c = GetCmdSpace<gles2::cmds::DepthMask>();
    if (c) {
      c->Init(flag);
    }
  }

  void DepthRangef(GLclampf zNear, GLclampf zFar) {
    gles2::cmds::DepthRangef* c = GetCmdSpace<gles2::cmds::DepthRangef>();
    if (c) {
      c->Init(zNear, zFar);
    }
  }

  void DetachShader(GLuint program, GLuint shader) {
    gles2::cmds::DetachShader* c = GetCmdSpace<gles2::cmds::DetachShader>();
    if (c) {
      c->Init(program, shader);
    }
  }

  void Disable(GLenum cap) {
    gles2::cmds::Disable* c = GetCmdSpace<gles2::cmds::Disable>();
    if (c) {
      c->Init(cap);
    }
  }

  void DisableVertexAttribArray(GLuint index) {
    gles2::cmds::DisableVertexAttribArray* c =
        GetCmdSpace<gles2::cmds::DisableVertexAttribArray>();
    if (c) {
      c->Init(index);
    }
  }

  void DrawArrays(GLenum mode, GLint first, GLsizei count) {
    gles2::cmds::DrawArrays* c = GetCmdSpace<gles2::cmds::DrawArrays>();
    if (c) {
      c->Init(mode, first, count);
    }
  }

  void DrawElements(
      GLenum mode, GLsizei count, GLenum type, GLuint index_offset) {
    gles2::cmds::DrawElements* c = GetCmdSpace<gles2::cmds::DrawElements>();
    if (c) {
      c->Init(mode, count, type, index_offset);
    }
  }

  void Enable(GLenum cap) {
    gles2::cmds::Enable* c = GetCmdSpace<gles2::cmds::Enable>();
    if (c) {
      c->Init(cap);
    }
  }

  void EnableVertexAttribArray(GLuint index) {
    gles2::cmds::EnableVertexAttribArray* c =
        GetCmdSpace<gles2::cmds::EnableVertexAttribArray>();
    if (c) {
      c->Init(index);
    }
  }

  void Finish() {
    gles2::cmds::Finish* c = GetCmdSpace<gles2::cmds::Finish>();
    if (c) {
      c->Init();
    }
  }

  void Flush() {
    gles2::cmds::Flush* c = GetCmdSpace<gles2::cmds::Flush>();
    if (c) {
      c->Init();
    }
  }

  void FramebufferRenderbuffer(
      GLenum target, GLenum attachment, GLenum renderbuffertarget,
      GLuint renderbuffer) {
    gles2::cmds::FramebufferRenderbuffer* c =
        GetCmdSpace<gles2::cmds::FramebufferRenderbuffer>();
    if (c) {
      c->Init(target, attachment, renderbuffertarget, renderbuffer);
    }
  }

  void FramebufferTexture2D(
      GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
      GLint level) {
    gles2::cmds::FramebufferTexture2D* c =
        GetCmdSpace<gles2::cmds::FramebufferTexture2D>();
    if (c) {
      c->Init(target, attachment, textarget, texture, level);
    }
  }

  void FrontFace(GLenum mode) {
    gles2::cmds::FrontFace* c = GetCmdSpace<gles2::cmds::FrontFace>();
    if (c) {
      c->Init(mode);
    }
  }

  void GenBuffers(
      GLsizei n, uint32 buffers_shm_id, uint32 buffers_shm_offset) {
    gles2::cmds::GenBuffers* c = GetCmdSpace<gles2::cmds::GenBuffers>();
    if (c) {
      c->Init(n, buffers_shm_id, buffers_shm_offset);
    }
  }

  void GenBuffersImmediate(GLsizei n, GLuint* buffers) {
    const uint32 size = gles2::cmds::GenBuffersImmediate::ComputeSize(n);
    gles2::cmds::GenBuffersImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::GenBuffersImmediate>(size);
    if (c) {
      c->Init(n, buffers);
    }
  }

  void GenerateMipmap(GLenum target) {
    gles2::cmds::GenerateMipmap* c =
        GetCmdSpace<gles2::cmds::GenerateMipmap>();
    if (c) {
      c->Init(target);
    }
  }

  void GenFramebuffers(
      GLsizei n, uint32 framebuffers_shm_id, uint32 framebuffers_shm_offset) {
    gles2::cmds::GenFramebuffers* c =
        GetCmdSpace<gles2::cmds::GenFramebuffers>();
    if (c) {
      c->Init(n, framebuffers_shm_id, framebuffers_shm_offset);
    }
  }

  void GenFramebuffersImmediate(GLsizei n, GLuint* framebuffers) {
    const uint32 size = gles2::cmds::GenFramebuffersImmediate::ComputeSize(n);
    gles2::cmds::GenFramebuffersImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::GenFramebuffersImmediate>(
            size);
    if (c) {
      c->Init(n, framebuffers);
    }
  }

  void GenRenderbuffers(
      GLsizei n, uint32 renderbuffers_shm_id,
      uint32 renderbuffers_shm_offset) {
    gles2::cmds::GenRenderbuffers* c =
        GetCmdSpace<gles2::cmds::GenRenderbuffers>();
    if (c) {
      c->Init(n, renderbuffers_shm_id, renderbuffers_shm_offset);
    }
  }

  void GenRenderbuffersImmediate(GLsizei n, GLuint* renderbuffers) {
    const uint32 size = gles2::cmds::GenRenderbuffersImmediate::ComputeSize(n);
    gles2::cmds::GenRenderbuffersImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::GenRenderbuffersImmediate>(
            size);
    if (c) {
      c->Init(n, renderbuffers);
    }
  }

  void GenTextures(
      GLsizei n, uint32 textures_shm_id, uint32 textures_shm_offset) {
    gles2::cmds::GenTextures* c = GetCmdSpace<gles2::cmds::GenTextures>();
    if (c) {
      c->Init(n, textures_shm_id, textures_shm_offset);
    }
  }

  void GenTexturesImmediate(GLsizei n, GLuint* textures) {
    const uint32 size = gles2::cmds::GenTexturesImmediate::ComputeSize(n);
    gles2::cmds::GenTexturesImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::GenTexturesImmediate>(size);
    if (c) {
      c->Init(n, textures);
    }
  }

  void GetActiveAttrib(
      GLuint program, GLuint index, uint32 name_bucket_id, uint32 result_shm_id,
      uint32 result_shm_offset) {
    gles2::cmds::GetActiveAttrib* c =
        GetCmdSpace<gles2::cmds::GetActiveAttrib>();
    if (c) {
      c->Init(
          program, index, name_bucket_id, result_shm_id, result_shm_offset);
    }
  }

  void GetActiveUniform(
      GLuint program, GLuint index, uint32 name_bucket_id, uint32 result_shm_id,
      uint32 result_shm_offset) {
    gles2::cmds::GetActiveUniform* c =
        GetCmdSpace<gles2::cmds::GetActiveUniform>();
    if (c) {
      c->Init(
          program, index, name_bucket_id, result_shm_id, result_shm_offset);
    }
  }

  void GetAttachedShaders(
      GLuint program, uint32 result_shm_id, uint32 result_shm_offset,
      uint32 result_size) {
    gles2::cmds::GetAttachedShaders* c =
        GetCmdSpace<gles2::cmds::GetAttachedShaders>();
    if (c) {
      c->Init(program, result_shm_id, result_shm_offset, result_size);
    }
  }

  void GetBooleanv(
      GLenum pname, uint32 params_shm_id, uint32 params_shm_offset) {
    gles2::cmds::GetBooleanv* c = GetCmdSpace<gles2::cmds::GetBooleanv>();
    if (c) {
      c->Init(pname, params_shm_id, params_shm_offset);
    }
  }

  void GetBufferParameteriv(
      GLenum target, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::cmds::GetBufferParameteriv* c =
        GetCmdSpace<gles2::cmds::GetBufferParameteriv>();
    if (c) {
      c->Init(target, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetError(uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::GetError* c = GetCmdSpace<gles2::cmds::GetError>();
    if (c) {
      c->Init(result_shm_id, result_shm_offset);
    }
  }

  void GetFloatv(
      GLenum pname, uint32 params_shm_id, uint32 params_shm_offset) {
    gles2::cmds::GetFloatv* c = GetCmdSpace<gles2::cmds::GetFloatv>();
    if (c) {
      c->Init(pname, params_shm_id, params_shm_offset);
    }
  }

  void GetFramebufferAttachmentParameteriv(
      GLenum target, GLenum attachment, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::cmds::GetFramebufferAttachmentParameteriv* c =
        GetCmdSpace<gles2::cmds::GetFramebufferAttachmentParameteriv>();
    if (c) {
      c->Init(target, attachment, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetIntegerv(
      GLenum pname, uint32 params_shm_id, uint32 params_shm_offset) {
    gles2::cmds::GetIntegerv* c = GetCmdSpace<gles2::cmds::GetIntegerv>();
    if (c) {
      c->Init(pname, params_shm_id, params_shm_offset);
    }
  }

  void GetProgramiv(
      GLuint program, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::cmds::GetProgramiv* c = GetCmdSpace<gles2::cmds::GetProgramiv>();
    if (c) {
      c->Init(program, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetProgramInfoLog(GLuint program, uint32 bucket_id) {
    gles2::cmds::GetProgramInfoLog* c =
        GetCmdSpace<gles2::cmds::GetProgramInfoLog>();
    if (c) {
      c->Init(program, bucket_id);
    }
  }

  void GetRenderbufferParameteriv(
      GLenum target, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::cmds::GetRenderbufferParameteriv* c =
        GetCmdSpace<gles2::cmds::GetRenderbufferParameteriv>();
    if (c) {
      c->Init(target, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetShaderiv(
      GLuint shader, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::cmds::GetShaderiv* c = GetCmdSpace<gles2::cmds::GetShaderiv>();
    if (c) {
      c->Init(shader, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetShaderInfoLog(GLuint shader, uint32 bucket_id) {
    gles2::cmds::GetShaderInfoLog* c =
        GetCmdSpace<gles2::cmds::GetShaderInfoLog>();
    if (c) {
      c->Init(shader, bucket_id);
    }
  }

  void GetShaderPrecisionFormat(
      GLenum shadertype, GLenum precisiontype, uint32 result_shm_id,
      uint32 result_shm_offset) {
    gles2::cmds::GetShaderPrecisionFormat* c =
        GetCmdSpace<gles2::cmds::GetShaderPrecisionFormat>();
    if (c) {
      c->Init(shadertype, precisiontype, result_shm_id, result_shm_offset);
    }
  }

  void GetShaderSource(GLuint shader, uint32 bucket_id) {
    gles2::cmds::GetShaderSource* c =
        GetCmdSpace<gles2::cmds::GetShaderSource>();
    if (c) {
      c->Init(shader, bucket_id);
    }
  }

  void GetString(GLenum name, uint32 bucket_id) {
    gles2::cmds::GetString* c = GetCmdSpace<gles2::cmds::GetString>();
    if (c) {
      c->Init(name, bucket_id);
    }
  }

  void GetTexParameterfv(
      GLenum target, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::cmds::GetTexParameterfv* c =
        GetCmdSpace<gles2::cmds::GetTexParameterfv>();
    if (c) {
      c->Init(target, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetTexParameteriv(
      GLenum target, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::cmds::GetTexParameteriv* c =
        GetCmdSpace<gles2::cmds::GetTexParameteriv>();
    if (c) {
      c->Init(target, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetUniformfv(
      GLuint program, GLint location, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::cmds::GetUniformfv* c = GetCmdSpace<gles2::cmds::GetUniformfv>();
    if (c) {
      c->Init(program, location, params_shm_id, params_shm_offset);
    }
  }

  void GetUniformiv(
      GLuint program, GLint location, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::cmds::GetUniformiv* c = GetCmdSpace<gles2::cmds::GetUniformiv>();
    if (c) {
      c->Init(program, location, params_shm_id, params_shm_offset);
    }
  }

  void GetVertexAttribfv(
      GLuint index, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::cmds::GetVertexAttribfv* c =
        GetCmdSpace<gles2::cmds::GetVertexAttribfv>();
    if (c) {
      c->Init(index, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetVertexAttribiv(
      GLuint index, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::cmds::GetVertexAttribiv* c =
        GetCmdSpace<gles2::cmds::GetVertexAttribiv>();
    if (c) {
      c->Init(index, pname, params_shm_id, params_shm_offset);
    }
  }

  void GetVertexAttribPointerv(
      GLuint index, GLenum pname, uint32 pointer_shm_id,
      uint32 pointer_shm_offset) {
    gles2::cmds::GetVertexAttribPointerv* c =
        GetCmdSpace<gles2::cmds::GetVertexAttribPointerv>();
    if (c) {
      c->Init(index, pname, pointer_shm_id, pointer_shm_offset);
    }
  }

  void Hint(GLenum target, GLenum mode) {
    gles2::cmds::Hint* c = GetCmdSpace<gles2::cmds::Hint>();
    if (c) {
      c->Init(target, mode);
    }
  }

  void IsBuffer(
      GLuint buffer, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::IsBuffer* c = GetCmdSpace<gles2::cmds::IsBuffer>();
    if (c) {
      c->Init(buffer, result_shm_id, result_shm_offset);
    }
  }

  void IsEnabled(GLenum cap, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::IsEnabled* c = GetCmdSpace<gles2::cmds::IsEnabled>();
    if (c) {
      c->Init(cap, result_shm_id, result_shm_offset);
    }
  }

  void IsFramebuffer(
      GLuint framebuffer, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::IsFramebuffer* c = GetCmdSpace<gles2::cmds::IsFramebuffer>();
    if (c) {
      c->Init(framebuffer, result_shm_id, result_shm_offset);
    }
  }

  void IsProgram(
      GLuint program, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::IsProgram* c = GetCmdSpace<gles2::cmds::IsProgram>();
    if (c) {
      c->Init(program, result_shm_id, result_shm_offset);
    }
  }

  void IsRenderbuffer(
      GLuint renderbuffer, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::IsRenderbuffer* c =
        GetCmdSpace<gles2::cmds::IsRenderbuffer>();
    if (c) {
      c->Init(renderbuffer, result_shm_id, result_shm_offset);
    }
  }

  void IsShader(
      GLuint shader, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::IsShader* c = GetCmdSpace<gles2::cmds::IsShader>();
    if (c) {
      c->Init(shader, result_shm_id, result_shm_offset);
    }
  }

  void IsTexture(
      GLuint texture, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::IsTexture* c = GetCmdSpace<gles2::cmds::IsTexture>();
    if (c) {
      c->Init(texture, result_shm_id, result_shm_offset);
    }
  }

  void LineWidth(GLfloat width) {
    gles2::cmds::LineWidth* c = GetCmdSpace<gles2::cmds::LineWidth>();
    if (c) {
      c->Init(width);
    }
  }

  void LinkProgram(GLuint program) {
    gles2::cmds::LinkProgram* c = GetCmdSpace<gles2::cmds::LinkProgram>();
    if (c) {
      c->Init(program);
    }
  }

  void PixelStorei(GLenum pname, GLint param) {
    gles2::cmds::PixelStorei* c = GetCmdSpace<gles2::cmds::PixelStorei>();
    if (c) {
      c->Init(pname, param);
    }
  }

  void PolygonOffset(GLfloat factor, GLfloat units) {
    gles2::cmds::PolygonOffset* c = GetCmdSpace<gles2::cmds::PolygonOffset>();
    if (c) {
      c->Init(factor, units);
    }
  }

  void ReadPixels(
      GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
      GLenum type, uint32 pixels_shm_id, uint32 pixels_shm_offset,
      uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::ReadPixels* c = GetCmdSpace<gles2::cmds::ReadPixels>();
    if (c) {
      c->Init(
          x, y, width, height, format, type, pixels_shm_id, pixels_shm_offset,
          result_shm_id, result_shm_offset);
    }
  }

  void ReleaseShaderCompiler() {
    gles2::cmds::ReleaseShaderCompiler* c =
        GetCmdSpace<gles2::cmds::ReleaseShaderCompiler>();
    if (c) {
      c->Init();
    }
  }

  void RenderbufferStorage(
      GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    gles2::cmds::RenderbufferStorage* c =
        GetCmdSpace<gles2::cmds::RenderbufferStorage>();
    if (c) {
      c->Init(target, internalformat, width, height);
    }
  }

  void SampleCoverage(GLclampf value, GLboolean invert) {
    gles2::cmds::SampleCoverage* c =
        GetCmdSpace<gles2::cmds::SampleCoverage>();
    if (c) {
      c->Init(value, invert);
    }
  }

  void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    gles2::cmds::Scissor* c = GetCmdSpace<gles2::cmds::Scissor>();
    if (c) {
      c->Init(x, y, width, height);
    }
  }

  void ShaderBinary(
      GLsizei n, uint32 shaders_shm_id, uint32 shaders_shm_offset,
      GLenum binaryformat, uint32 binary_shm_id, uint32 binary_shm_offset,
      GLsizei length) {
    gles2::cmds::ShaderBinary* c = GetCmdSpace<gles2::cmds::ShaderBinary>();
    if (c) {
      c->Init(
          n, shaders_shm_id, shaders_shm_offset, binaryformat, binary_shm_id,
          binary_shm_offset, length);
    }
  }

  void ShaderSource(
      GLuint shader, uint32 data_shm_id, uint32 data_shm_offset,
      uint32 data_size) {
    gles2::cmds::ShaderSource* c = GetCmdSpace<gles2::cmds::ShaderSource>();
    if (c) {
      c->Init(shader, data_shm_id, data_shm_offset, data_size);
    }
  }

  void ShaderSourceImmediate(GLuint shader, uint32 data_size) {
    const uint32 s = 0;  // TODO(gman): compute correct size
    gles2::cmds::ShaderSourceImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::ShaderSourceImmediate>(s);
    if (c) {
      c->Init(shader, data_size);
    }
  }

  void ShaderSourceBucket(GLuint shader, uint32 data_bucket_id) {
    gles2::cmds::ShaderSourceBucket* c =
        GetCmdSpace<gles2::cmds::ShaderSourceBucket>();
    if (c) {
      c->Init(shader, data_bucket_id);
    }
  }

  void StencilFunc(GLenum func, GLint ref, GLuint mask) {
    gles2::cmds::StencilFunc* c = GetCmdSpace<gles2::cmds::StencilFunc>();
    if (c) {
      c->Init(func, ref, mask);
    }
  }

  void StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    gles2::cmds::StencilFuncSeparate* c =
        GetCmdSpace<gles2::cmds::StencilFuncSeparate>();
    if (c) {
      c->Init(face, func, ref, mask);
    }
  }

  void StencilMask(GLuint mask) {
    gles2::cmds::StencilMask* c = GetCmdSpace<gles2::cmds::StencilMask>();
    if (c) {
      c->Init(mask);
    }
  }

  void StencilMaskSeparate(GLenum face, GLuint mask) {
    gles2::cmds::StencilMaskSeparate* c =
        GetCmdSpace<gles2::cmds::StencilMaskSeparate>();
    if (c) {
      c->Init(face, mask);
    }
  }

  void StencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    gles2::cmds::StencilOp* c = GetCmdSpace<gles2::cmds::StencilOp>();
    if (c) {
      c->Init(fail, zfail, zpass);
    }
  }

  void StencilOpSeparate(
      GLenum face, GLenum fail, GLenum zfail, GLenum zpass) {
    gles2::cmds::StencilOpSeparate* c =
        GetCmdSpace<gles2::cmds::StencilOpSeparate>();
    if (c) {
      c->Init(face, fail, zfail, zpass);
    }
  }

  void TexImage2D(
      GLenum target, GLint level, GLint internalformat, GLsizei width,
      GLsizei height, GLint border, GLenum format, GLenum type,
      uint32 pixels_shm_id, uint32 pixels_shm_offset) {
    gles2::cmds::TexImage2D* c = GetCmdSpace<gles2::cmds::TexImage2D>();
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
    gles2::cmds::TexImage2DImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::TexImage2DImmediate>(s);
    if (c) {
      c->Init(
          target, level, internalformat, width, height, border, format, type);
    }
  }

  void TexParameterf(GLenum target, GLenum pname, GLfloat param) {
    gles2::cmds::TexParameterf* c = GetCmdSpace<gles2::cmds::TexParameterf>();
    if (c) {
      c->Init(target, pname, param);
    }
  }

  void TexParameterfv(
      GLenum target, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::cmds::TexParameterfv* c =
        GetCmdSpace<gles2::cmds::TexParameterfv>();
    if (c) {
      c->Init(target, pname, params_shm_id, params_shm_offset);
    }
  }

  void TexParameterfvImmediate(
      GLenum target, GLenum pname, const GLfloat* params) {
    const uint32 size = gles2::cmds::TexParameterfvImmediate::ComputeSize();
    gles2::cmds::TexParameterfvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::TexParameterfvImmediate>(
            size);
    if (c) {
      c->Init(target, pname, params);
    }
  }

  void TexParameteri(GLenum target, GLenum pname, GLint param) {
    gles2::cmds::TexParameteri* c = GetCmdSpace<gles2::cmds::TexParameteri>();
    if (c) {
      c->Init(target, pname, param);
    }
  }

  void TexParameteriv(
      GLenum target, GLenum pname, uint32 params_shm_id,
      uint32 params_shm_offset) {
    gles2::cmds::TexParameteriv* c =
        GetCmdSpace<gles2::cmds::TexParameteriv>();
    if (c) {
      c->Init(target, pname, params_shm_id, params_shm_offset);
    }
  }

  void TexParameterivImmediate(
      GLenum target, GLenum pname, const GLint* params) {
    const uint32 size = gles2::cmds::TexParameterivImmediate::ComputeSize();
    gles2::cmds::TexParameterivImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::TexParameterivImmediate>(
            size);
    if (c) {
      c->Init(target, pname, params);
    }
  }

  void TexSubImage2D(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLenum type, uint32 pixels_shm_id,
      uint32 pixels_shm_offset, GLboolean internal) {
    gles2::cmds::TexSubImage2D* c = GetCmdSpace<gles2::cmds::TexSubImage2D>();
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
    gles2::cmds::TexSubImage2DImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::TexSubImage2DImmediate>(s);
    if (c) {
      c->Init(
          target, level, xoffset, yoffset, width, height, format, type,
          internal);
    }
  }

  void Uniform1f(GLint location, GLfloat x) {
    gles2::cmds::Uniform1f* c = GetCmdSpace<gles2::cmds::Uniform1f>();
    if (c) {
      c->Init(location, x);
    }
  }

  void Uniform1fv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::cmds::Uniform1fv* c = GetCmdSpace<gles2::cmds::Uniform1fv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform1fvImmediate(GLint location, GLsizei count, const GLfloat* v) {
    const uint32 size = gles2::cmds::Uniform1fvImmediate::ComputeSize(count);
    gles2::cmds::Uniform1fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform1fvImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform1i(GLint location, GLint x) {
    gles2::cmds::Uniform1i* c = GetCmdSpace<gles2::cmds::Uniform1i>();
    if (c) {
      c->Init(location, x);
    }
  }

  void Uniform1iv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::cmds::Uniform1iv* c = GetCmdSpace<gles2::cmds::Uniform1iv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform1ivImmediate(GLint location, GLsizei count, const GLint* v) {
    const uint32 size = gles2::cmds::Uniform1ivImmediate::ComputeSize(count);
    gles2::cmds::Uniform1ivImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform1ivImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform2f(GLint location, GLfloat x, GLfloat y) {
    gles2::cmds::Uniform2f* c = GetCmdSpace<gles2::cmds::Uniform2f>();
    if (c) {
      c->Init(location, x, y);
    }
  }

  void Uniform2fv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::cmds::Uniform2fv* c = GetCmdSpace<gles2::cmds::Uniform2fv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform2fvImmediate(GLint location, GLsizei count, const GLfloat* v) {
    const uint32 size = gles2::cmds::Uniform2fvImmediate::ComputeSize(count);
    gles2::cmds::Uniform2fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform2fvImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform2i(GLint location, GLint x, GLint y) {
    gles2::cmds::Uniform2i* c = GetCmdSpace<gles2::cmds::Uniform2i>();
    if (c) {
      c->Init(location, x, y);
    }
  }

  void Uniform2iv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::cmds::Uniform2iv* c = GetCmdSpace<gles2::cmds::Uniform2iv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform2ivImmediate(GLint location, GLsizei count, const GLint* v) {
    const uint32 size = gles2::cmds::Uniform2ivImmediate::ComputeSize(count);
    gles2::cmds::Uniform2ivImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform2ivImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
    gles2::cmds::Uniform3f* c = GetCmdSpace<gles2::cmds::Uniform3f>();
    if (c) {
      c->Init(location, x, y, z);
    }
  }

  void Uniform3fv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::cmds::Uniform3fv* c = GetCmdSpace<gles2::cmds::Uniform3fv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform3fvImmediate(GLint location, GLsizei count, const GLfloat* v) {
    const uint32 size = gles2::cmds::Uniform3fvImmediate::ComputeSize(count);
    gles2::cmds::Uniform3fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform3fvImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform3i(GLint location, GLint x, GLint y, GLint z) {
    gles2::cmds::Uniform3i* c = GetCmdSpace<gles2::cmds::Uniform3i>();
    if (c) {
      c->Init(location, x, y, z);
    }
  }

  void Uniform3iv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::cmds::Uniform3iv* c = GetCmdSpace<gles2::cmds::Uniform3iv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform3ivImmediate(GLint location, GLsizei count, const GLint* v) {
    const uint32 size = gles2::cmds::Uniform3ivImmediate::ComputeSize(count);
    gles2::cmds::Uniform3ivImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform3ivImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    gles2::cmds::Uniform4f* c = GetCmdSpace<gles2::cmds::Uniform4f>();
    if (c) {
      c->Init(location, x, y, z, w);
    }
  }

  void Uniform4fv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::cmds::Uniform4fv* c = GetCmdSpace<gles2::cmds::Uniform4fv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform4fvImmediate(GLint location, GLsizei count, const GLfloat* v) {
    const uint32 size = gles2::cmds::Uniform4fvImmediate::ComputeSize(count);
    gles2::cmds::Uniform4fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform4fvImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void Uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
    gles2::cmds::Uniform4i* c = GetCmdSpace<gles2::cmds::Uniform4i>();
    if (c) {
      c->Init(location, x, y, z, w);
    }
  }

  void Uniform4iv(
      GLint location, GLsizei count, uint32 v_shm_id, uint32 v_shm_offset) {
    gles2::cmds::Uniform4iv* c = GetCmdSpace<gles2::cmds::Uniform4iv>();
    if (c) {
      c->Init(location, count, v_shm_id, v_shm_offset);
    }
  }

  void Uniform4ivImmediate(GLint location, GLsizei count, const GLint* v) {
    const uint32 size = gles2::cmds::Uniform4ivImmediate::ComputeSize(count);
    gles2::cmds::Uniform4ivImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform4ivImmediate>(size);
    if (c) {
      c->Init(location, count, v);
    }
  }

  void UniformMatrix2fv(
      GLint location, GLsizei count, GLboolean transpose, uint32 value_shm_id,
      uint32 value_shm_offset) {
    gles2::cmds::UniformMatrix2fv* c =
        GetCmdSpace<gles2::cmds::UniformMatrix2fv>();
    if (c) {
      c->Init(location, count, transpose, value_shm_id, value_shm_offset);
    }
  }

  void UniformMatrix2fvImmediate(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value) {
    const uint32 size =
        gles2::cmds::UniformMatrix2fvImmediate::ComputeSize(count);
    gles2::cmds::UniformMatrix2fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::UniformMatrix2fvImmediate>(
            size);
    if (c) {
      c->Init(location, count, transpose, value);
    }
  }

  void UniformMatrix3fv(
      GLint location, GLsizei count, GLboolean transpose, uint32 value_shm_id,
      uint32 value_shm_offset) {
    gles2::cmds::UniformMatrix3fv* c =
        GetCmdSpace<gles2::cmds::UniformMatrix3fv>();
    if (c) {
      c->Init(location, count, transpose, value_shm_id, value_shm_offset);
    }
  }

  void UniformMatrix3fvImmediate(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value) {
    const uint32 size =
        gles2::cmds::UniformMatrix3fvImmediate::ComputeSize(count);
    gles2::cmds::UniformMatrix3fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::UniformMatrix3fvImmediate>(
            size);
    if (c) {
      c->Init(location, count, transpose, value);
    }
  }

  void UniformMatrix4fv(
      GLint location, GLsizei count, GLboolean transpose, uint32 value_shm_id,
      uint32 value_shm_offset) {
    gles2::cmds::UniformMatrix4fv* c =
        GetCmdSpace<gles2::cmds::UniformMatrix4fv>();
    if (c) {
      c->Init(location, count, transpose, value_shm_id, value_shm_offset);
    }
  }

  void UniformMatrix4fvImmediate(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value) {
    const uint32 size =
        gles2::cmds::UniformMatrix4fvImmediate::ComputeSize(count);
    gles2::cmds::UniformMatrix4fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::UniformMatrix4fvImmediate>(
            size);
    if (c) {
      c->Init(location, count, transpose, value);
    }
  }

  void UseProgram(GLuint program) {
    gles2::cmds::UseProgram* c = GetCmdSpace<gles2::cmds::UseProgram>();
    if (c) {
      c->Init(program);
    }
  }

  void ValidateProgram(GLuint program) {
    gles2::cmds::ValidateProgram* c =
        GetCmdSpace<gles2::cmds::ValidateProgram>();
    if (c) {
      c->Init(program);
    }
  }

  void VertexAttrib1f(GLuint indx, GLfloat x) {
    gles2::cmds::VertexAttrib1f* c =
        GetCmdSpace<gles2::cmds::VertexAttrib1f>();
    if (c) {
      c->Init(indx, x);
    }
  }

  void VertexAttrib1fv(
      GLuint indx, uint32 values_shm_id, uint32 values_shm_offset) {
    gles2::cmds::VertexAttrib1fv* c =
        GetCmdSpace<gles2::cmds::VertexAttrib1fv>();
    if (c) {
      c->Init(indx, values_shm_id, values_shm_offset);
    }
  }

  void VertexAttrib1fvImmediate(GLuint indx, const GLfloat* values) {
    const uint32 size = gles2::cmds::VertexAttrib1fvImmediate::ComputeSize();
    gles2::cmds::VertexAttrib1fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::VertexAttrib1fvImmediate>(
            size);
    if (c) {
      c->Init(indx, values);
    }
  }

  void VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
    gles2::cmds::VertexAttrib2f* c =
        GetCmdSpace<gles2::cmds::VertexAttrib2f>();
    if (c) {
      c->Init(indx, x, y);
    }
  }

  void VertexAttrib2fv(
      GLuint indx, uint32 values_shm_id, uint32 values_shm_offset) {
    gles2::cmds::VertexAttrib2fv* c =
        GetCmdSpace<gles2::cmds::VertexAttrib2fv>();
    if (c) {
      c->Init(indx, values_shm_id, values_shm_offset);
    }
  }

  void VertexAttrib2fvImmediate(GLuint indx, const GLfloat* values) {
    const uint32 size = gles2::cmds::VertexAttrib2fvImmediate::ComputeSize();
    gles2::cmds::VertexAttrib2fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::VertexAttrib2fvImmediate>(
            size);
    if (c) {
      c->Init(indx, values);
    }
  }

  void VertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
    gles2::cmds::VertexAttrib3f* c =
        GetCmdSpace<gles2::cmds::VertexAttrib3f>();
    if (c) {
      c->Init(indx, x, y, z);
    }
  }

  void VertexAttrib3fv(
      GLuint indx, uint32 values_shm_id, uint32 values_shm_offset) {
    gles2::cmds::VertexAttrib3fv* c =
        GetCmdSpace<gles2::cmds::VertexAttrib3fv>();
    if (c) {
      c->Init(indx, values_shm_id, values_shm_offset);
    }
  }

  void VertexAttrib3fvImmediate(GLuint indx, const GLfloat* values) {
    const uint32 size = gles2::cmds::VertexAttrib3fvImmediate::ComputeSize();
    gles2::cmds::VertexAttrib3fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::VertexAttrib3fvImmediate>(
            size);
    if (c) {
      c->Init(indx, values);
    }
  }

  void VertexAttrib4f(
      GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    gles2::cmds::VertexAttrib4f* c =
        GetCmdSpace<gles2::cmds::VertexAttrib4f>();
    if (c) {
      c->Init(indx, x, y, z, w);
    }
  }

  void VertexAttrib4fv(
      GLuint indx, uint32 values_shm_id, uint32 values_shm_offset) {
    gles2::cmds::VertexAttrib4fv* c =
        GetCmdSpace<gles2::cmds::VertexAttrib4fv>();
    if (c) {
      c->Init(indx, values_shm_id, values_shm_offset);
    }
  }

  void VertexAttrib4fvImmediate(GLuint indx, const GLfloat* values) {
    const uint32 size = gles2::cmds::VertexAttrib4fvImmediate::ComputeSize();
    gles2::cmds::VertexAttrib4fvImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::VertexAttrib4fvImmediate>(
            size);
    if (c) {
      c->Init(indx, values);
    }
  }

  void VertexAttribPointer(
      GLuint indx, GLint size, GLenum type, GLboolean normalized,
      GLsizei stride, GLuint offset) {
    gles2::cmds::VertexAttribPointer* c =
        GetCmdSpace<gles2::cmds::VertexAttribPointer>();
    if (c) {
      c->Init(indx, size, type, normalized, stride, offset);
    }
  }

  void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    gles2::cmds::Viewport* c = GetCmdSpace<gles2::cmds::Viewport>();
    if (c) {
      c->Init(x, y, width, height);
    }
  }

  void BlitFramebufferEXT(
      GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
      GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    gles2::cmds::BlitFramebufferEXT* c =
        GetCmdSpace<gles2::cmds::BlitFramebufferEXT>();
    if (c) {
      c->Init(
          srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask,
          filter);
    }
  }

  void RenderbufferStorageMultisampleEXT(
      GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
      GLsizei height) {
    gles2::cmds::RenderbufferStorageMultisampleEXT* c =
        GetCmdSpace<gles2::cmds::RenderbufferStorageMultisampleEXT>();
    if (c) {
      c->Init(target, samples, internalformat, width, height);
    }
  }

  void TexStorage2DEXT(
      GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width,
      GLsizei height) {
    gles2::cmds::TexStorage2DEXT* c =
        GetCmdSpace<gles2::cmds::TexStorage2DEXT>();
    if (c) {
      c->Init(target, levels, internalFormat, width, height);
    }
  }

  void GenQueriesEXT(
      GLsizei n, uint32 queries_shm_id, uint32 queries_shm_offset) {
    gles2::cmds::GenQueriesEXT* c = GetCmdSpace<gles2::cmds::GenQueriesEXT>();
    if (c) {
      c->Init(n, queries_shm_id, queries_shm_offset);
    }
  }

  void GenQueriesEXTImmediate(GLsizei n, GLuint* queries) {
    const uint32 size = gles2::cmds::GenQueriesEXTImmediate::ComputeSize(n);
    gles2::cmds::GenQueriesEXTImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::GenQueriesEXTImmediate>(
            size);
    if (c) {
      c->Init(n, queries);
    }
  }

  void DeleteQueriesEXT(
      GLsizei n, uint32 queries_shm_id, uint32 queries_shm_offset) {
    gles2::cmds::DeleteQueriesEXT* c =
        GetCmdSpace<gles2::cmds::DeleteQueriesEXT>();
    if (c) {
      c->Init(n, queries_shm_id, queries_shm_offset);
    }
  }

  void DeleteQueriesEXTImmediate(GLsizei n, const GLuint* queries) {
    const uint32 size = gles2::cmds::DeleteQueriesEXTImmediate::ComputeSize(n);
    gles2::cmds::DeleteQueriesEXTImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::DeleteQueriesEXTImmediate>(
            size);
    if (c) {
      c->Init(n, queries);
    }
  }

  void BeginQueryEXT(
      GLenum target, GLuint id, uint32 sync_data_shm_id,
      uint32 sync_data_shm_offset) {
    gles2::cmds::BeginQueryEXT* c = GetCmdSpace<gles2::cmds::BeginQueryEXT>();
    if (c) {
      c->Init(target, id, sync_data_shm_id, sync_data_shm_offset);
    }
  }

  void EndQueryEXT(GLenum target, GLuint submit_count) {
    gles2::cmds::EndQueryEXT* c = GetCmdSpace<gles2::cmds::EndQueryEXT>();
    if (c) {
      c->Init(target, submit_count);
    }
  }

  void InsertEventMarkerEXT(GLuint bucket_id) {
    gles2::cmds::InsertEventMarkerEXT* c =
        GetCmdSpace<gles2::cmds::InsertEventMarkerEXT>();
    if (c) {
      c->Init(bucket_id);
    }
  }

  void PushGroupMarkerEXT(GLuint bucket_id) {
    gles2::cmds::PushGroupMarkerEXT* c =
        GetCmdSpace<gles2::cmds::PushGroupMarkerEXT>();
    if (c) {
      c->Init(bucket_id);
    }
  }

  void PopGroupMarkerEXT() {
    gles2::cmds::PopGroupMarkerEXT* c =
        GetCmdSpace<gles2::cmds::PopGroupMarkerEXT>();
    if (c) {
      c->Init();
    }
  }

  void GenVertexArraysOES(
      GLsizei n, uint32 arrays_shm_id, uint32 arrays_shm_offset) {
    gles2::cmds::GenVertexArraysOES* c =
        GetCmdSpace<gles2::cmds::GenVertexArraysOES>();
    if (c) {
      c->Init(n, arrays_shm_id, arrays_shm_offset);
    }
  }

  void GenVertexArraysOESImmediate(GLsizei n, GLuint* arrays) {
    const uint32 size =
        gles2::cmds::GenVertexArraysOESImmediate::ComputeSize(n);
    gles2::cmds::GenVertexArraysOESImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::GenVertexArraysOESImmediate>(
            size);
    if (c) {
      c->Init(n, arrays);
    }
  }

  void DeleteVertexArraysOES(
      GLsizei n, uint32 arrays_shm_id, uint32 arrays_shm_offset) {
    gles2::cmds::DeleteVertexArraysOES* c =
        GetCmdSpace<gles2::cmds::DeleteVertexArraysOES>();
    if (c) {
      c->Init(n, arrays_shm_id, arrays_shm_offset);
    }
  }

  void DeleteVertexArraysOESImmediate(GLsizei n, const GLuint* arrays) {
    const uint32 size =
        gles2::cmds::DeleteVertexArraysOESImmediate::ComputeSize(n);
    gles2::cmds::DeleteVertexArraysOESImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::DeleteVertexArraysOESImmediate>(size);  // NOLINT
    if (c) {
      c->Init(n, arrays);
    }
  }

  void IsVertexArrayOES(
      GLuint array, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::IsVertexArrayOES* c =
        GetCmdSpace<gles2::cmds::IsVertexArrayOES>();
    if (c) {
      c->Init(array, result_shm_id, result_shm_offset);
    }
  }

  void BindVertexArrayOES(GLuint array) {
    gles2::cmds::BindVertexArrayOES* c =
        GetCmdSpace<gles2::cmds::BindVertexArrayOES>();
    if (c) {
      c->Init(array);
    }
  }

  void SwapBuffers() {
    gles2::cmds::SwapBuffers* c = GetCmdSpace<gles2::cmds::SwapBuffers>();
    if (c) {
      c->Init();
    }
  }

  void GetMaxValueInBufferCHROMIUM(
      GLuint buffer_id, GLsizei count, GLenum type, GLuint offset,
      uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::GetMaxValueInBufferCHROMIUM* c =
        GetCmdSpace<gles2::cmds::GetMaxValueInBufferCHROMIUM>();
    if (c) {
      c->Init(
          buffer_id, count, type, offset, result_shm_id, result_shm_offset);
    }
  }

  void GenSharedIdsCHROMIUM(
      GLuint namespace_id, GLuint id_offset, GLsizei n, uint32 ids_shm_id,
      uint32 ids_shm_offset) {
    gles2::cmds::GenSharedIdsCHROMIUM* c =
        GetCmdSpace<gles2::cmds::GenSharedIdsCHROMIUM>();
    if (c) {
      c->Init(namespace_id, id_offset, n, ids_shm_id, ids_shm_offset);
    }
  }

  void DeleteSharedIdsCHROMIUM(
      GLuint namespace_id, GLsizei n, uint32 ids_shm_id,
      uint32 ids_shm_offset) {
    gles2::cmds::DeleteSharedIdsCHROMIUM* c =
        GetCmdSpace<gles2::cmds::DeleteSharedIdsCHROMIUM>();
    if (c) {
      c->Init(namespace_id, n, ids_shm_id, ids_shm_offset);
    }
  }

  void RegisterSharedIdsCHROMIUM(
      GLuint namespace_id, GLsizei n, uint32 ids_shm_id,
      uint32 ids_shm_offset) {
    gles2::cmds::RegisterSharedIdsCHROMIUM* c =
        GetCmdSpace<gles2::cmds::RegisterSharedIdsCHROMIUM>();
    if (c) {
      c->Init(namespace_id, n, ids_shm_id, ids_shm_offset);
    }
  }

  void EnableFeatureCHROMIUM(
      GLuint bucket_id, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::EnableFeatureCHROMIUM* c =
        GetCmdSpace<gles2::cmds::EnableFeatureCHROMIUM>();
    if (c) {
      c->Init(bucket_id, result_shm_id, result_shm_offset);
    }
  }

  void ResizeCHROMIUM(GLuint width, GLuint height) {
    gles2::cmds::ResizeCHROMIUM* c =
        GetCmdSpace<gles2::cmds::ResizeCHROMIUM>();
    if (c) {
      c->Init(width, height);
    }
  }

  void GetRequestableExtensionsCHROMIUM(uint32 bucket_id) {
    gles2::cmds::GetRequestableExtensionsCHROMIUM* c =
        GetCmdSpace<gles2::cmds::GetRequestableExtensionsCHROMIUM>();
    if (c) {
      c->Init(bucket_id);
    }
  }

  void RequestExtensionCHROMIUM(uint32 bucket_id) {
    gles2::cmds::RequestExtensionCHROMIUM* c =
        GetCmdSpace<gles2::cmds::RequestExtensionCHROMIUM>();
    if (c) {
      c->Init(bucket_id);
    }
  }

  void GetMultipleIntegervCHROMIUM(
      uint32 pnames_shm_id, uint32 pnames_shm_offset, GLuint count,
      uint32 results_shm_id, uint32 results_shm_offset, GLsizeiptr size) {
    gles2::cmds::GetMultipleIntegervCHROMIUM* c =
        GetCmdSpace<gles2::cmds::GetMultipleIntegervCHROMIUM>();
    if (c) {
      c->Init(
          pnames_shm_id, pnames_shm_offset, count, results_shm_id,
          results_shm_offset, size);
    }
  }

  void GetProgramInfoCHROMIUM(GLuint program, uint32 bucket_id) {
    gles2::cmds::GetProgramInfoCHROMIUM* c =
        GetCmdSpace<gles2::cmds::GetProgramInfoCHROMIUM>();
    if (c) {
      c->Init(program, bucket_id);
    }
  }

  void CreateStreamTextureCHROMIUM(
      GLuint client_id, uint32 result_shm_id, uint32 result_shm_offset) {
    gles2::cmds::CreateStreamTextureCHROMIUM* c =
        GetCmdSpace<gles2::cmds::CreateStreamTextureCHROMIUM>();
    if (c) {
      c->Init(client_id, result_shm_id, result_shm_offset);
    }
  }

  void DestroyStreamTextureCHROMIUM(GLuint texture) {
    gles2::cmds::DestroyStreamTextureCHROMIUM* c =
        GetCmdSpace<gles2::cmds::DestroyStreamTextureCHROMIUM>();
    if (c) {
      c->Init(texture);
    }
  }

  void GetTranslatedShaderSourceANGLE(GLuint shader, uint32 bucket_id) {
    gles2::cmds::GetTranslatedShaderSourceANGLE* c =
        GetCmdSpace<gles2::cmds::GetTranslatedShaderSourceANGLE>();
    if (c) {
      c->Init(shader, bucket_id);
    }
  }

  void PostSubBufferCHROMIUM(GLint x, GLint y, GLint width, GLint height) {
    gles2::cmds::PostSubBufferCHROMIUM* c =
        GetCmdSpace<gles2::cmds::PostSubBufferCHROMIUM>();
    if (c) {
      c->Init(x, y, width, height);
    }
  }

  void TexImageIOSurface2DCHROMIUM(
      GLenum target, GLsizei width, GLsizei height, GLuint ioSurfaceId,
      GLuint plane) {
    gles2::cmds::TexImageIOSurface2DCHROMIUM* c =
        GetCmdSpace<gles2::cmds::TexImageIOSurface2DCHROMIUM>();
    if (c) {
      c->Init(target, width, height, ioSurfaceId, plane);
    }
  }

  void CopyTextureCHROMIUM(
      GLenum target, GLenum source_id, GLenum dest_id, GLint level,
      GLint internalformat) {
    gles2::cmds::CopyTextureCHROMIUM* c =
        GetCmdSpace<gles2::cmds::CopyTextureCHROMIUM>();
    if (c) {
      c->Init(target, source_id, dest_id, level, internalformat);
    }
  }

  void DrawArraysInstancedANGLE(
      GLenum mode, GLint first, GLsizei count, GLsizei primcount) {
    gles2::cmds::DrawArraysInstancedANGLE* c =
        GetCmdSpace<gles2::cmds::DrawArraysInstancedANGLE>();
    if (c) {
      c->Init(mode, first, count, primcount);
    }
  }

  void DrawElementsInstancedANGLE(
      GLenum mode, GLsizei count, GLenum type, GLuint index_offset,
      GLsizei primcount) {
    gles2::cmds::DrawElementsInstancedANGLE* c =
        GetCmdSpace<gles2::cmds::DrawElementsInstancedANGLE>();
    if (c) {
      c->Init(mode, count, type, index_offset, primcount);
    }
  }

  void VertexAttribDivisorANGLE(GLuint index, GLuint divisor) {
    gles2::cmds::VertexAttribDivisorANGLE* c =
        GetCmdSpace<gles2::cmds::VertexAttribDivisorANGLE>();
    if (c) {
      c->Init(index, divisor);
    }
  }

  void GenMailboxCHROMIUM(GLuint bucket_id) {
    gles2::cmds::GenMailboxCHROMIUM* c =
        GetCmdSpace<gles2::cmds::GenMailboxCHROMIUM>();
    if (c) {
      c->Init(bucket_id);
    }
  }

  void ProduceTextureCHROMIUM(
      GLenum target, uint32 mailbox_shm_id, uint32 mailbox_shm_offset) {
    gles2::cmds::ProduceTextureCHROMIUM* c =
        GetCmdSpace<gles2::cmds::ProduceTextureCHROMIUM>();
    if (c) {
      c->Init(target, mailbox_shm_id, mailbox_shm_offset);
    }
  }

  void ProduceTextureCHROMIUMImmediate(GLenum target, const GLbyte* mailbox) {
    const uint32 size =
        gles2::cmds::ProduceTextureCHROMIUMImmediate::ComputeSize();
    gles2::cmds::ProduceTextureCHROMIUMImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::ProduceTextureCHROMIUMImmediate>(size);  // NOLINT
    if (c) {
      c->Init(target, mailbox);
    }
  }

  void ConsumeTextureCHROMIUM(
      GLenum target, uint32 mailbox_shm_id, uint32 mailbox_shm_offset) {
    gles2::cmds::ConsumeTextureCHROMIUM* c =
        GetCmdSpace<gles2::cmds::ConsumeTextureCHROMIUM>();
    if (c) {
      c->Init(target, mailbox_shm_id, mailbox_shm_offset);
    }
  }

  void ConsumeTextureCHROMIUMImmediate(GLenum target, const GLbyte* mailbox) {
    const uint32 size =
        gles2::cmds::ConsumeTextureCHROMIUMImmediate::ComputeSize();
    gles2::cmds::ConsumeTextureCHROMIUMImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::ConsumeTextureCHROMIUMImmediate>(size);  // NOLINT
    if (c) {
      c->Init(target, mailbox);
    }
  }

  void BindUniformLocationCHROMIUM(
      GLuint program, GLint location, uint32 name_shm_id,
      uint32 name_shm_offset, uint32 data_size) {
    gles2::cmds::BindUniformLocationCHROMIUM* c =
        GetCmdSpace<gles2::cmds::BindUniformLocationCHROMIUM>();
    if (c) {
      c->Init(program, location, name_shm_id, name_shm_offset, data_size);
    }
  }

  void BindUniformLocationCHROMIUMImmediate(
      GLuint program, GLint location, const char* name) {
    const uint32 data_size = strlen(name);
    gles2::cmds::BindUniformLocationCHROMIUMImmediate* c =
        GetImmediateCmdSpace<gles2::cmds::BindUniformLocationCHROMIUMImmediate>(
            data_size);
    if (c) {
      c->Init(program, location, name, data_size);
    }
  }

  void BindUniformLocationCHROMIUMBucket(
      GLuint program, GLint location, uint32 name_bucket_id) {
    gles2::cmds::BindUniformLocationCHROMIUMBucket* c =
        GetCmdSpace<gles2::cmds::BindUniformLocationCHROMIUMBucket>();
    if (c) {
      c->Init(program, location, name_bucket_id);
    }
  }

  void BindTexImage2DCHROMIUM(GLenum target, GLint imageId) {
    gles2::cmds::BindTexImage2DCHROMIUM* c =
        GetCmdSpace<gles2::cmds::BindTexImage2DCHROMIUM>();
    if (c) {
      c->Init(target, imageId);
    }
  }

  void ReleaseTexImage2DCHROMIUM(GLenum target, GLint imageId) {
    gles2::cmds::ReleaseTexImage2DCHROMIUM* c =
        GetCmdSpace<gles2::cmds::ReleaseTexImage2DCHROMIUM>();
    if (c) {
      c->Init(target, imageId);
    }
  }

  void TraceBeginCHROMIUM(GLuint bucket_id) {
    gles2::cmds::TraceBeginCHROMIUM* c =
        GetCmdSpace<gles2::cmds::TraceBeginCHROMIUM>();
    if (c) {
      c->Init(bucket_id);
    }
  }

  void TraceEndCHROMIUM() {
    gles2::cmds::TraceEndCHROMIUM* c =
        GetCmdSpace<gles2::cmds::TraceEndCHROMIUM>();
    if (c) {
      c->Init();
    }
  }

  void AsyncTexSubImage2DCHROMIUM(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLenum type, uint32 data_shm_id,
      uint32 data_shm_offset) {
    gles2::cmds::AsyncTexSubImage2DCHROMIUM* c =
        GetCmdSpace<gles2::cmds::AsyncTexSubImage2DCHROMIUM>();
    if (c) {
      c->Init(
          target, level, xoffset, yoffset, width, height, format, type,
          data_shm_id, data_shm_offset);
    }
  }

  void AsyncTexImage2DCHROMIUM(
      GLenum target, GLint level, GLint internalformat, GLsizei width,
      GLsizei height, GLint border, GLenum format, GLenum type,
      uint32 pixels_shm_id, uint32 pixels_shm_offset) {
    gles2::cmds::AsyncTexImage2DCHROMIUM* c =
        GetCmdSpace<gles2::cmds::AsyncTexImage2DCHROMIUM>();
    if (c) {
      c->Init(
          target, level, internalformat, width, height, border, format, type,
          pixels_shm_id, pixels_shm_offset);
    }
  }

  void DiscardFramebufferEXT(
      GLenum target, GLsizei count, uint32 attachments_shm_id,
      uint32 attachments_shm_offset) {
    gles2::cmds::DiscardFramebufferEXT* c =
        GetCmdSpace<gles2::cmds::DiscardFramebufferEXT>();
    if (c) {
      c->Init(target, count, attachments_shm_id, attachments_shm_offset);
    }
  }

  void DiscardFramebufferEXTImmediate(
      GLenum target, GLsizei count, const GLenum* attachments) {
    const uint32 size =
        gles2::cmds::DiscardFramebufferEXTImmediate::ComputeSize(count);
    gles2::cmds::DiscardFramebufferEXTImmediate* c =
        GetImmediateCmdSpaceTotalSize<gles2::cmds::DiscardFramebufferEXTImmediate>(size);  // NOLINT
    if (c) {
      c->Init(target, count, attachments);
    }
  }

  void LoseContextCHROMIUM(GLenum current, GLenum other) {
    gles2::cmds::LoseContextCHROMIUM* c =
        GetCmdSpace<gles2::cmds::LoseContextCHROMIUM>();
    if (c) {
      c->Init(current, other);
    }
  }

  void WaitSyncPointCHROMIUM(GLuint sync_point) {
    gles2::cmds::WaitSyncPointCHROMIUM* c =
        GetCmdSpace<gles2::cmds::WaitSyncPointCHROMIUM>();
    if (c) {
      c->Init(sync_point);
    }
  }

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_AUTOGEN_H_

