// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

// It is included by gles2_cmd_decoder.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_AUTOGEN_H_

error::Error GLES2DecoderImpl::HandleActiveTexture(
    uint32 immediate_data_size, const gles2::ActiveTexture& c) {
  GLenum texture = static_cast<GLenum>(c.texture);
  DoActiveTexture(texture);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleAttachShader(
    uint32 immediate_data_size, const gles2::AttachShader& c) {
  GLuint program = c.program;
  GLuint shader = c.shader;
  DoAttachShader(program, shader);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindBuffer(
    uint32 immediate_data_size, const gles2::BindBuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint buffer = c.buffer;
  if (!validators_->buffer_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glBindBuffer: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  DoBindBuffer(target, buffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindFramebuffer(
    uint32 immediate_data_size, const gles2::BindFramebuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint framebuffer = c.framebuffer;
  if (!validators_->frame_buffer_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glBindFramebuffer: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  DoBindFramebuffer(target, framebuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindRenderbuffer(
    uint32 immediate_data_size, const gles2::BindRenderbuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint renderbuffer = c.renderbuffer;
  if (!validators_->render_buffer_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glBindRenderbuffer: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  DoBindRenderbuffer(target, renderbuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindTexture(
    uint32 immediate_data_size, const gles2::BindTexture& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint texture = c.texture;
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glBindTexture: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  DoBindTexture(target, texture);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendColor(
    uint32 immediate_data_size, const gles2::BlendColor& c) {
  GLclampf red = static_cast<GLclampf>(c.red);
  GLclampf green = static_cast<GLclampf>(c.green);
  GLclampf blue = static_cast<GLclampf>(c.blue);
  GLclampf alpha = static_cast<GLclampf>(c.alpha);
  glBlendColor(red, green, blue, alpha);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendEquation(
    uint32 immediate_data_size, const gles2::BlendEquation& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  if (!validators_->equation.IsValid(mode)) {
    SetGLError(GL_INVALID_ENUM, "glBlendEquation: mode GL_INVALID_ENUM");
    return error::kNoError;
  }
  glBlendEquation(mode);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendEquationSeparate(
    uint32 immediate_data_size, const gles2::BlendEquationSeparate& c) {
  GLenum modeRGB = static_cast<GLenum>(c.modeRGB);
  GLenum modeAlpha = static_cast<GLenum>(c.modeAlpha);
  if (!validators_->equation.IsValid(modeRGB)) {
    SetGLError(
        GL_INVALID_ENUM, "glBlendEquationSeparate: modeRGB GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->equation.IsValid(modeAlpha)) {
    SetGLError(
        GL_INVALID_ENUM, "glBlendEquationSeparate: modeAlpha GL_INVALID_ENUM");
    return error::kNoError;
  }
  glBlendEquationSeparate(modeRGB, modeAlpha);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendFunc(
    uint32 immediate_data_size, const gles2::BlendFunc& c) {
  GLenum sfactor = static_cast<GLenum>(c.sfactor);
  GLenum dfactor = static_cast<GLenum>(c.dfactor);
  if (!validators_->src_blend_factor.IsValid(sfactor)) {
    SetGLError(GL_INVALID_ENUM, "glBlendFunc: sfactor GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->dst_blend_factor.IsValid(dfactor)) {
    SetGLError(GL_INVALID_ENUM, "glBlendFunc: dfactor GL_INVALID_ENUM");
    return error::kNoError;
  }
  glBlendFunc(sfactor, dfactor);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendFuncSeparate(
    uint32 immediate_data_size, const gles2::BlendFuncSeparate& c) {
  GLenum srcRGB = static_cast<GLenum>(c.srcRGB);
  GLenum dstRGB = static_cast<GLenum>(c.dstRGB);
  GLenum srcAlpha = static_cast<GLenum>(c.srcAlpha);
  GLenum dstAlpha = static_cast<GLenum>(c.dstAlpha);
  if (!validators_->src_blend_factor.IsValid(srcRGB)) {
    SetGLError(GL_INVALID_ENUM, "glBlendFuncSeparate: srcRGB GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->dst_blend_factor.IsValid(dstRGB)) {
    SetGLError(GL_INVALID_ENUM, "glBlendFuncSeparate: dstRGB GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->src_blend_factor.IsValid(srcAlpha)) {
    SetGLError(
        GL_INVALID_ENUM, "glBlendFuncSeparate: srcAlpha GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->dst_blend_factor.IsValid(dstAlpha)) {
    SetGLError(
        GL_INVALID_ENUM, "glBlendFuncSeparate: dstAlpha GL_INVALID_ENUM");
    return error::kNoError;
  }
  glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBufferSubData(
    uint32 immediate_data_size, const gles2::BufferSubData& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32 data_size = size;
  const void* data = GetSharedMemoryAs<const void*>(
      c.data_shm_id, c.data_shm_offset, data_size);
  if (!validators_->buffer_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glBufferSubData: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (size < 0) {
    SetGLError(GL_INVALID_VALUE, "glBufferSubData: size < 0");
    return error::kNoError;
  }
  if (data == NULL) {
    return error::kOutOfBounds;
  }
  DoBufferSubData(target, offset, size, data);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBufferSubDataImmediate(
    uint32 immediate_data_size, const gles2::BufferSubDataImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32 data_size = size;
  const void* data = GetImmediateDataAs<const void*>(
      c, data_size, immediate_data_size);
  if (!validators_->buffer_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glBufferSubData: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (size < 0) {
    SetGLError(GL_INVALID_VALUE, "glBufferSubData: size < 0");
    return error::kNoError;
  }
  if (data == NULL) {
    return error::kOutOfBounds;
  }
  DoBufferSubData(target, offset, size, data);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCheckFramebufferStatus(
    uint32 immediate_data_size, const gles2::CheckFramebufferStatus& c) {
  GLenum target = static_cast<GLenum>(c.target);
  typedef CheckFramebufferStatus::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  if (!validators_->frame_buffer_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM, "glCheckFramebufferStatus: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  *result_dst = DoCheckFramebufferStatus(target);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClear(
    uint32 immediate_data_size, const gles2::Clear& c) {
  GLbitfield mask = static_cast<GLbitfield>(c.mask);
  DoClear(mask);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClearColor(
    uint32 immediate_data_size, const gles2::ClearColor& c) {
  GLclampf red = static_cast<GLclampf>(c.red);
  GLclampf green = static_cast<GLclampf>(c.green);
  GLclampf blue = static_cast<GLclampf>(c.blue);
  GLclampf alpha = static_cast<GLclampf>(c.alpha);
  DoClearColor(red, green, blue, alpha);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClearDepthf(
    uint32 immediate_data_size, const gles2::ClearDepthf& c) {
  GLclampf depth = static_cast<GLclampf>(c.depth);
  DoClearDepthf(depth);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClearStencil(
    uint32 immediate_data_size, const gles2::ClearStencil& c) {
  GLint s = static_cast<GLint>(c.s);
  DoClearStencil(s);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleColorMask(
    uint32 immediate_data_size, const gles2::ColorMask& c) {
  GLboolean red = static_cast<GLboolean>(c.red);
  GLboolean green = static_cast<GLboolean>(c.green);
  GLboolean blue = static_cast<GLboolean>(c.blue);
  GLboolean alpha = static_cast<GLboolean>(c.alpha);
  DoColorMask(red, green, blue, alpha);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCompileShader(
    uint32 immediate_data_size, const gles2::CompileShader& c) {
  GLuint shader = c.shader;
  DoCompileShader(shader);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCompressedTexSubImage2D(
    uint32 immediate_data_size, const gles2::CompressedTexSubImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLsizei imageSize = static_cast<GLsizei>(c.imageSize);
  uint32 data_size = imageSize;
  const void* data = GetSharedMemoryAs<const void*>(
      c.data_shm_id, c.data_shm_offset, data_size);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM, "glCompressedTexSubImage2D: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D: width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D: height < 0");
    return error::kNoError;
  }
  if (!validators_->compressed_texture_format.IsValid(format)) {
    SetGLError(
        GL_INVALID_ENUM, "glCompressedTexSubImage2D: format GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (imageSize < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D: imageSize < 0");
    return error::kNoError;
  }
  if (data == NULL) {
    return error::kOutOfBounds;
  }
  DoCompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCompressedTexSubImage2DImmediate(
    uint32 immediate_data_size,
    const gles2::CompressedTexSubImage2DImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLsizei imageSize = static_cast<GLsizei>(c.imageSize);
  uint32 data_size = imageSize;
  const void* data = GetImmediateDataAs<const void*>(
      c, data_size, immediate_data_size);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM, "glCompressedTexSubImage2D: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D: width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D: height < 0");
    return error::kNoError;
  }
  if (!validators_->compressed_texture_format.IsValid(format)) {
    SetGLError(
        GL_INVALID_ENUM, "glCompressedTexSubImage2D: format GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (imageSize < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D: imageSize < 0");
    return error::kNoError;
  }
  if (data == NULL) {
    return error::kOutOfBounds;
  }
  DoCompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCopyTexImage2D(
    uint32 immediate_data_size, const gles2::CopyTexImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glCopyTexImage2D: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->texture_internal_format.IsValid(internalformat)) {
    SetGLError(
        GL_INVALID_ENUM, "glCopyTexImage2D: internalformat GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexImage2D: width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexImage2D: height < 0");
    return error::kNoError;
  }
  if (!validators_->texture_border.IsValid(border)) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexImage2D: border GL_INVALID_VALUE");
    return error::kNoError;
  }
  DoCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCopyTexSubImage2D(
    uint32 immediate_data_size, const gles2::CopyTexSubImage2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glCopyTexSubImage2D: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexSubImage2D: width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexSubImage2D: height < 0");
    return error::kNoError;
  }
  DoCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCreateProgram(
    uint32 immediate_data_size, const gles2::CreateProgram& c) {
  uint32 client_id = c.client_id;
  if (!CreateProgramHelper(client_id)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCreateShader(
    uint32 immediate_data_size, const gles2::CreateShader& c) {
  GLenum type = static_cast<GLenum>(c.type);
  if (!validators_->shader_type.IsValid(type)) {
    SetGLError(GL_INVALID_ENUM, "glCreateShader: type GL_INVALID_ENUM");
    return error::kNoError;
  }
  uint32 client_id = c.client_id;
  if (!CreateShaderHelper(type, client_id)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCullFace(
    uint32 immediate_data_size, const gles2::CullFace& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  if (!validators_->face_type.IsValid(mode)) {
    SetGLError(GL_INVALID_ENUM, "glCullFace: mode GL_INVALID_ENUM");
    return error::kNoError;
  }
  glCullFace(mode);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteBuffers(
    uint32 immediate_data_size, const gles2::DeleteBuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* buffers = GetSharedMemoryAs<const GLuint*>(
      c.buffers_shm_id, c.buffers_shm_offset, data_size);
  if (buffers == NULL) {
    return error::kOutOfBounds;
  }
  DeleteBuffersHelper(n, buffers);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteBuffersImmediate(
    uint32 immediate_data_size, const gles2::DeleteBuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* buffers = GetImmediateDataAs<const GLuint*>(
      c, data_size, immediate_data_size);
  if (buffers == NULL) {
    return error::kOutOfBounds;
  }
  DeleteBuffersHelper(n, buffers);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteFramebuffers(
    uint32 immediate_data_size, const gles2::DeleteFramebuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* framebuffers = GetSharedMemoryAs<const GLuint*>(
      c.framebuffers_shm_id, c.framebuffers_shm_offset, data_size);
  if (framebuffers == NULL) {
    return error::kOutOfBounds;
  }
  DeleteFramebuffersHelper(n, framebuffers);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteFramebuffersImmediate(
    uint32 immediate_data_size, const gles2::DeleteFramebuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* framebuffers = GetImmediateDataAs<const GLuint*>(
      c, data_size, immediate_data_size);
  if (framebuffers == NULL) {
    return error::kOutOfBounds;
  }
  DeleteFramebuffersHelper(n, framebuffers);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteRenderbuffers(
    uint32 immediate_data_size, const gles2::DeleteRenderbuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* renderbuffers = GetSharedMemoryAs<const GLuint*>(
      c.renderbuffers_shm_id, c.renderbuffers_shm_offset, data_size);
  if (renderbuffers == NULL) {
    return error::kOutOfBounds;
  }
  DeleteRenderbuffersHelper(n, renderbuffers);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteRenderbuffersImmediate(
    uint32 immediate_data_size, const gles2::DeleteRenderbuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* renderbuffers = GetImmediateDataAs<const GLuint*>(
      c, data_size, immediate_data_size);
  if (renderbuffers == NULL) {
    return error::kOutOfBounds;
  }
  DeleteRenderbuffersHelper(n, renderbuffers);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteTextures(
    uint32 immediate_data_size, const gles2::DeleteTextures& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* textures = GetSharedMemoryAs<const GLuint*>(
      c.textures_shm_id, c.textures_shm_offset, data_size);
  if (textures == NULL) {
    return error::kOutOfBounds;
  }
  DeleteTexturesHelper(n, textures);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteTexturesImmediate(
    uint32 immediate_data_size, const gles2::DeleteTexturesImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* textures = GetImmediateDataAs<const GLuint*>(
      c, data_size, immediate_data_size);
  if (textures == NULL) {
    return error::kOutOfBounds;
  }
  DeleteTexturesHelper(n, textures);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDepthFunc(
    uint32 immediate_data_size, const gles2::DepthFunc& c) {
  GLenum func = static_cast<GLenum>(c.func);
  if (!validators_->cmp_function.IsValid(func)) {
    SetGLError(GL_INVALID_ENUM, "glDepthFunc: func GL_INVALID_ENUM");
    return error::kNoError;
  }
  glDepthFunc(func);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDepthMask(
    uint32 immediate_data_size, const gles2::DepthMask& c) {
  GLboolean flag = static_cast<GLboolean>(c.flag);
  DoDepthMask(flag);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDepthRangef(
    uint32 immediate_data_size, const gles2::DepthRangef& c) {
  GLclampf zNear = static_cast<GLclampf>(c.zNear);
  GLclampf zFar = static_cast<GLclampf>(c.zFar);
  glDepthRange(zNear, zFar);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDetachShader(
    uint32 immediate_data_size, const gles2::DetachShader& c) {
  GLuint program = c.program;
  GLuint shader = c.shader;
  DoDetachShader(program, shader);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDisable(
    uint32 immediate_data_size, const gles2::Disable& c) {
  GLenum cap = static_cast<GLenum>(c.cap);
  if (!validators_->capability.IsValid(cap)) {
    SetGLError(GL_INVALID_ENUM, "glDisable: cap GL_INVALID_ENUM");
    return error::kNoError;
  }
  DoDisable(cap);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDisableVertexAttribArray(
    uint32 immediate_data_size, const gles2::DisableVertexAttribArray& c) {
  GLuint index = static_cast<GLuint>(c.index);
  DoDisableVertexAttribArray(index);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleEnable(
    uint32 immediate_data_size, const gles2::Enable& c) {
  GLenum cap = static_cast<GLenum>(c.cap);
  if (!validators_->capability.IsValid(cap)) {
    SetGLError(GL_INVALID_ENUM, "glEnable: cap GL_INVALID_ENUM");
    return error::kNoError;
  }
  DoEnable(cap);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleEnableVertexAttribArray(
    uint32 immediate_data_size, const gles2::EnableVertexAttribArray& c) {
  GLuint index = static_cast<GLuint>(c.index);
  DoEnableVertexAttribArray(index);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFinish(
    uint32 immediate_data_size, const gles2::Finish& c) {
  DoFinish();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFlush(
    uint32 immediate_data_size, const gles2::Flush& c) {
  DoFlush();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFramebufferRenderbuffer(
    uint32 immediate_data_size, const gles2::FramebufferRenderbuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum renderbuffertarget = static_cast<GLenum>(c.renderbuffertarget);
  GLuint renderbuffer = c.renderbuffer;
  if (!validators_->frame_buffer_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM, "glFramebufferRenderbuffer: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->attachment.IsValid(attachment)) {
    SetGLError(
        GL_INVALID_ENUM,
        "glFramebufferRenderbuffer: attachment GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->render_buffer_target.IsValid(renderbuffertarget)) {
    SetGLError(
        GL_INVALID_ENUM,
        "glFramebufferRenderbuffer: renderbuffertarget GL_INVALID_ENUM");
    return error::kNoError;
  }
  DoFramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFramebufferTexture2D(
    uint32 immediate_data_size, const gles2::FramebufferTexture2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum textarget = static_cast<GLenum>(c.textarget);
  GLuint texture = c.texture;
  GLint level = static_cast<GLint>(c.level);
  if (!validators_->frame_buffer_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM, "glFramebufferTexture2D: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->attachment.IsValid(attachment)) {
    SetGLError(
        GL_INVALID_ENUM, "glFramebufferTexture2D: attachment GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->texture_target.IsValid(textarget)) {
    SetGLError(
        GL_INVALID_ENUM, "glFramebufferTexture2D: textarget GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->zero_only.IsValid(level)) {
    SetGLError(
        GL_INVALID_VALUE, "glFramebufferTexture2D: level GL_INVALID_VALUE");
    return error::kNoError;
  }
  DoFramebufferTexture2D(target, attachment, textarget, texture, level);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFrontFace(
    uint32 immediate_data_size, const gles2::FrontFace& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  if (!validators_->face_mode.IsValid(mode)) {
    SetGLError(GL_INVALID_ENUM, "glFrontFace: mode GL_INVALID_ENUM");
    return error::kNoError;
  }
  glFrontFace(mode);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenBuffers(
    uint32 immediate_data_size, const gles2::GenBuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* buffers = GetSharedMemoryAs<GLuint*>(
      c.buffers_shm_id, c.buffers_shm_offset, data_size);
  if (buffers == NULL) {
    return error::kOutOfBounds;
  }
  if (!GenBuffersHelper(n, buffers)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenBuffersImmediate(
    uint32 immediate_data_size, const gles2::GenBuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* buffers = GetImmediateDataAs<GLuint*>(
      c, data_size, immediate_data_size);
  if (buffers == NULL) {
    return error::kOutOfBounds;
  }
  if (!GenBuffersHelper(n, buffers)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenerateMipmap(
    uint32 immediate_data_size, const gles2::GenerateMipmap& c) {
  GLenum target = static_cast<GLenum>(c.target);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glGenerateMipmap: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  DoGenerateMipmap(target);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenFramebuffers(
    uint32 immediate_data_size, const gles2::GenFramebuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* framebuffers = GetSharedMemoryAs<GLuint*>(
      c.framebuffers_shm_id, c.framebuffers_shm_offset, data_size);
  if (framebuffers == NULL) {
    return error::kOutOfBounds;
  }
  if (!GenFramebuffersHelper(n, framebuffers)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenFramebuffersImmediate(
    uint32 immediate_data_size, const gles2::GenFramebuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* framebuffers = GetImmediateDataAs<GLuint*>(
      c, data_size, immediate_data_size);
  if (framebuffers == NULL) {
    return error::kOutOfBounds;
  }
  if (!GenFramebuffersHelper(n, framebuffers)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenRenderbuffers(
    uint32 immediate_data_size, const gles2::GenRenderbuffers& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* renderbuffers = GetSharedMemoryAs<GLuint*>(
      c.renderbuffers_shm_id, c.renderbuffers_shm_offset, data_size);
  if (renderbuffers == NULL) {
    return error::kOutOfBounds;
  }
  if (!GenRenderbuffersHelper(n, renderbuffers)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenRenderbuffersImmediate(
    uint32 immediate_data_size, const gles2::GenRenderbuffersImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* renderbuffers = GetImmediateDataAs<GLuint*>(
      c, data_size, immediate_data_size);
  if (renderbuffers == NULL) {
    return error::kOutOfBounds;
  }
  if (!GenRenderbuffersHelper(n, renderbuffers)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenTextures(
    uint32 immediate_data_size, const gles2::GenTextures& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* textures = GetSharedMemoryAs<GLuint*>(
      c.textures_shm_id, c.textures_shm_offset, data_size);
  if (textures == NULL) {
    return error::kOutOfBounds;
  }
  if (!GenTexturesHelper(n, textures)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenTexturesImmediate(
    uint32 immediate_data_size, const gles2::GenTexturesImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* textures = GetImmediateDataAs<GLuint*>(
      c, data_size, immediate_data_size);
  if (textures == NULL) {
    return error::kOutOfBounds;
  }
  if (!GenTexturesHelper(n, textures)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetBooleanv(
    uint32 immediate_data_size, const gles2::GetBooleanv& c) {
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef GetBooleanv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLboolean* params = result ? result->GetData() : NULL;
  if (!validators_->g_l_state.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glGetBooleanv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  CopyRealGLErrorsToWrapper();
  DoGetBooleanv(pname, params);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetBufferParameteriv(
    uint32 immediate_data_size, const gles2::GetBufferParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef GetBufferParameteriv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->buffer_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM, "glGetBufferParameteriv: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->buffer_parameter.IsValid(pname)) {
    SetGLError(
        GL_INVALID_ENUM, "glGetBufferParameteriv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  CopyRealGLErrorsToWrapper();
  glGetBufferParameteriv(target, pname, params);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetError(
    uint32 immediate_data_size, const gles2::GetError& c) {
  typedef GetError::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = GetGLError();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetFloatv(
    uint32 immediate_data_size, const gles2::GetFloatv& c) {
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef GetFloatv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLfloat* params = result ? result->GetData() : NULL;
  if (!validators_->g_l_state.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glGetFloatv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  CopyRealGLErrorsToWrapper();
  DoGetFloatv(pname, params);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetFramebufferAttachmentParameteriv(
    uint32 immediate_data_size,
    const gles2::GetFramebufferAttachmentParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef GetFramebufferAttachmentParameteriv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->frame_buffer_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM,
        "glGetFramebufferAttachmentParameteriv: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->attachment.IsValid(attachment)) {
    SetGLError(
        GL_INVALID_ENUM,
        "glGetFramebufferAttachmentParameteriv: attachment GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->frame_buffer_parameter.IsValid(pname)) {
    SetGLError(
        GL_INVALID_ENUM,
        "glGetFramebufferAttachmentParameteriv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  CopyRealGLErrorsToWrapper();
  DoGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetIntegerv(
    uint32 immediate_data_size, const gles2::GetIntegerv& c) {
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef GetIntegerv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->g_l_state.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glGetIntegerv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  CopyRealGLErrorsToWrapper();
  DoGetIntegerv(pname, params);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetProgramiv(
    uint32 immediate_data_size, const gles2::GetProgramiv& c) {
  GLuint program = c.program;
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef GetProgramiv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->program_parameter.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glGetProgramiv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  CopyRealGLErrorsToWrapper();
  DoGetProgramiv(program, pname, params);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetRenderbufferParameteriv(
    uint32 immediate_data_size, const gles2::GetRenderbufferParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef GetRenderbufferParameteriv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->render_buffer_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM,
        "glGetRenderbufferParameteriv: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->render_buffer_parameter.IsValid(pname)) {
    SetGLError(
        GL_INVALID_ENUM,
        "glGetRenderbufferParameteriv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  CopyRealGLErrorsToWrapper();
  DoGetRenderbufferParameteriv(target, pname, params);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetShaderiv(
    uint32 immediate_data_size, const gles2::GetShaderiv& c) {
  GLuint shader = c.shader;
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef GetShaderiv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->shader_parameter.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glGetShaderiv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  CopyRealGLErrorsToWrapper();
  DoGetShaderiv(shader, pname, params);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetTexParameterfv(
    uint32 immediate_data_size, const gles2::GetTexParameterfv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef GetTexParameterfv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLfloat* params = result ? result->GetData() : NULL;
  if (!validators_->get_tex_param_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glGetTexParameterfv: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glGetTexParameterfv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  CopyRealGLErrorsToWrapper();
  glGetTexParameterfv(target, pname, params);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetTexParameteriv(
    uint32 immediate_data_size, const gles2::GetTexParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef GetTexParameteriv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->get_tex_param_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glGetTexParameteriv: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glGetTexParameteriv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  CopyRealGLErrorsToWrapper();
  glGetTexParameteriv(target, pname, params);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetVertexAttribfv(
    uint32 immediate_data_size, const gles2::GetVertexAttribfv& c) {
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef GetVertexAttribfv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLfloat* params = result ? result->GetData() : NULL;
  if (!validators_->vertex_attribute.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glGetVertexAttribfv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  CopyRealGLErrorsToWrapper();
  DoGetVertexAttribfv(index, pname, params);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetVertexAttribiv(
    uint32 immediate_data_size, const gles2::GetVertexAttribiv& c) {
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef GetVertexAttribiv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->vertex_attribute.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glGetVertexAttribiv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  CopyRealGLErrorsToWrapper();
  DoGetVertexAttribiv(index, pname, params);
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  } else {
    SetGLError(error, NULL);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleHint(
    uint32 immediate_data_size, const gles2::Hint& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum mode = static_cast<GLenum>(c.mode);
  if (!validators_->hint_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glHint: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->hint_mode.IsValid(mode)) {
    SetGLError(GL_INVALID_ENUM, "glHint: mode GL_INVALID_ENUM");
    return error::kNoError;
  }
  glHint(target, mode);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsBuffer(
    uint32 immediate_data_size, const gles2::IsBuffer& c) {
  GLuint buffer = c.buffer;
  typedef IsBuffer::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsBuffer(buffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsEnabled(
    uint32 immediate_data_size, const gles2::IsEnabled& c) {
  GLenum cap = static_cast<GLenum>(c.cap);
  typedef IsEnabled::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  if (!validators_->capability.IsValid(cap)) {
    SetGLError(GL_INVALID_ENUM, "glIsEnabled: cap GL_INVALID_ENUM");
    return error::kNoError;
  }
  *result_dst = glIsEnabled(cap);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsFramebuffer(
    uint32 immediate_data_size, const gles2::IsFramebuffer& c) {
  GLuint framebuffer = c.framebuffer;
  typedef IsFramebuffer::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsFramebuffer(framebuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsProgram(
    uint32 immediate_data_size, const gles2::IsProgram& c) {
  GLuint program = c.program;
  typedef IsProgram::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsProgram(program);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsRenderbuffer(
    uint32 immediate_data_size, const gles2::IsRenderbuffer& c) {
  GLuint renderbuffer = c.renderbuffer;
  typedef IsRenderbuffer::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsRenderbuffer(renderbuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsShader(
    uint32 immediate_data_size, const gles2::IsShader& c) {
  GLuint shader = c.shader;
  typedef IsShader::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsShader(shader);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsTexture(
    uint32 immediate_data_size, const gles2::IsTexture& c) {
  GLuint texture = c.texture;
  typedef IsTexture::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsTexture(texture);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleLineWidth(
    uint32 immediate_data_size, const gles2::LineWidth& c) {
  GLfloat width = static_cast<GLfloat>(c.width);
  glLineWidth(width);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleLinkProgram(
    uint32 immediate_data_size, const gles2::LinkProgram& c) {
  GLuint program = c.program;
  DoLinkProgram(program);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePolygonOffset(
    uint32 immediate_data_size, const gles2::PolygonOffset& c) {
  GLfloat factor = static_cast<GLfloat>(c.factor);
  GLfloat units = static_cast<GLfloat>(c.units);
  glPolygonOffset(factor, units);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleReleaseShaderCompiler(
    uint32 immediate_data_size, const gles2::ReleaseShaderCompiler& c) {
  DoReleaseShaderCompiler();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleRenderbufferStorage(
    uint32 immediate_data_size, const gles2::RenderbufferStorage& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->render_buffer_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM, "glRenderbufferStorage: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->render_buffer_format.IsValid(internalformat)) {
    SetGLError(
        GL_INVALID_ENUM,
        "glRenderbufferStorage: internalformat GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorage: width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorage: height < 0");
    return error::kNoError;
  }
  DoRenderbufferStorage(target, internalformat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSampleCoverage(
    uint32 immediate_data_size, const gles2::SampleCoverage& c) {
  GLclampf value = static_cast<GLclampf>(c.value);
  GLboolean invert = static_cast<GLboolean>(c.invert);
  glSampleCoverage(value, invert);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleScissor(
    uint32 immediate_data_size, const gles2::Scissor& c) {
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glScissor: width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glScissor: height < 0");
    return error::kNoError;
  }
  glScissor(x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilFunc(
    uint32 immediate_data_size, const gles2::StencilFunc& c) {
  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  if (!validators_->cmp_function.IsValid(func)) {
    SetGLError(GL_INVALID_ENUM, "glStencilFunc: func GL_INVALID_ENUM");
    return error::kNoError;
  }
  glStencilFunc(func, ref, mask);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilFuncSeparate(
    uint32 immediate_data_size, const gles2::StencilFuncSeparate& c) {
  GLenum face = static_cast<GLenum>(c.face);
  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  if (!validators_->face_type.IsValid(face)) {
    SetGLError(GL_INVALID_ENUM, "glStencilFuncSeparate: face GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->cmp_function.IsValid(func)) {
    SetGLError(GL_INVALID_ENUM, "glStencilFuncSeparate: func GL_INVALID_ENUM");
    return error::kNoError;
  }
  glStencilFuncSeparate(face, func, ref, mask);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilMask(
    uint32 immediate_data_size, const gles2::StencilMask& c) {
  GLuint mask = static_cast<GLuint>(c.mask);
  DoStencilMask(mask);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilMaskSeparate(
    uint32 immediate_data_size, const gles2::StencilMaskSeparate& c) {
  GLenum face = static_cast<GLenum>(c.face);
  GLuint mask = static_cast<GLuint>(c.mask);
  if (!validators_->face_type.IsValid(face)) {
    SetGLError(GL_INVALID_ENUM, "glStencilMaskSeparate: face GL_INVALID_ENUM");
    return error::kNoError;
  }
  DoStencilMaskSeparate(face, mask);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilOp(
    uint32 immediate_data_size, const gles2::StencilOp& c) {
  GLenum fail = static_cast<GLenum>(c.fail);
  GLenum zfail = static_cast<GLenum>(c.zfail);
  GLenum zpass = static_cast<GLenum>(c.zpass);
  if (!validators_->stencil_op.IsValid(fail)) {
    SetGLError(GL_INVALID_ENUM, "glStencilOp: fail GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(zfail)) {
    SetGLError(GL_INVALID_ENUM, "glStencilOp: zfail GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(zpass)) {
    SetGLError(GL_INVALID_ENUM, "glStencilOp: zpass GL_INVALID_ENUM");
    return error::kNoError;
  }
  glStencilOp(fail, zfail, zpass);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilOpSeparate(
    uint32 immediate_data_size, const gles2::StencilOpSeparate& c) {
  GLenum face = static_cast<GLenum>(c.face);
  GLenum fail = static_cast<GLenum>(c.fail);
  GLenum zfail = static_cast<GLenum>(c.zfail);
  GLenum zpass = static_cast<GLenum>(c.zpass);
  if (!validators_->face_type.IsValid(face)) {
    SetGLError(GL_INVALID_ENUM, "glStencilOpSeparate: face GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(fail)) {
    SetGLError(GL_INVALID_ENUM, "glStencilOpSeparate: fail GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(zfail)) {
    SetGLError(GL_INVALID_ENUM, "glStencilOpSeparate: zfail GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(zpass)) {
    SetGLError(GL_INVALID_ENUM, "glStencilOpSeparate: zpass GL_INVALID_ENUM");
    return error::kNoError;
  }
  glStencilOpSeparate(face, fail, zfail, zpass);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameterf(
    uint32 immediate_data_size, const gles2::TexParameterf& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat param = static_cast<GLfloat>(c.param);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glTexParameterf: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glTexParameterf: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  DoTexParameterf(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameterfv(
    uint32 immediate_data_size, const gles2::TexParameterfv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLfloat), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLfloat* params = GetSharedMemoryAs<const GLfloat*>(
      c.params_shm_id, c.params_shm_offset, data_size);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glTexParameterfv: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glTexParameterfv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  DoTexParameterfv(target, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameterfvImmediate(
    uint32 immediate_data_size, const gles2::TexParameterfvImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLfloat), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLfloat* params = GetImmediateDataAs<const GLfloat*>(
      c, data_size, immediate_data_size);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glTexParameterfv: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glTexParameterfv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  DoTexParameterfv(target, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameteri(
    uint32 immediate_data_size, const gles2::TexParameteri& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint param = static_cast<GLint>(c.param);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glTexParameteri: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glTexParameteri: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  DoTexParameteri(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameteriv(
    uint32 immediate_data_size, const gles2::TexParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLint), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLint* params = GetSharedMemoryAs<const GLint*>(
      c.params_shm_id, c.params_shm_offset, data_size);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glTexParameteriv: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glTexParameteriv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  DoTexParameteriv(target, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameterivImmediate(
    uint32 immediate_data_size, const gles2::TexParameterivImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLint), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLint* params = GetImmediateDataAs<const GLint*>(
      c, data_size, immediate_data_size);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glTexParameteriv: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLError(GL_INVALID_ENUM, "glTexParameteriv: pname GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  DoTexParameteriv(target, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1f(
    uint32 immediate_data_size, const gles2::Uniform1f& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat temp[1] = { x, };
  DoUniform1fv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1fv(
    uint32 immediate_data_size, const gles2::Uniform1fv& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLfloat* v = GetSharedMemoryAs<const GLfloat*>(
      c.v_shm_id, c.v_shm_offset, data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform1fv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1fvImmediate(
    uint32 immediate_data_size, const gles2::Uniform1fvImmediate& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLfloat* v = GetImmediateDataAs<const GLfloat*>(
      c, data_size, immediate_data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform1fv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1i(
    uint32 immediate_data_size, const gles2::Uniform1i& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLint x = static_cast<GLint>(c.x);
  DoUniform1i(location, x);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1iv(
    uint32 immediate_data_size, const gles2::Uniform1iv& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLint), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLint* v = GetSharedMemoryAs<const GLint*>(
      c.v_shm_id, c.v_shm_offset, data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform1iv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1ivImmediate(
    uint32 immediate_data_size, const gles2::Uniform1ivImmediate& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLint), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLint* v = GetImmediateDataAs<const GLint*>(
      c, data_size, immediate_data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform1iv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2f(
    uint32 immediate_data_size, const gles2::Uniform2f& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat temp[2] = { x, y, };
  DoUniform2fv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2fv(
    uint32 immediate_data_size, const gles2::Uniform2fv& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 2, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLfloat* v = GetSharedMemoryAs<const GLfloat*>(
      c.v_shm_id, c.v_shm_offset, data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform2fv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2fvImmediate(
    uint32 immediate_data_size, const gles2::Uniform2fvImmediate& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 2, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLfloat* v = GetImmediateDataAs<const GLfloat*>(
      c, data_size, immediate_data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform2fv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2i(
    uint32 immediate_data_size, const gles2::Uniform2i& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint temp[2] = { x, y, };
  DoUniform2iv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2iv(
    uint32 immediate_data_size, const gles2::Uniform2iv& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLint), 2, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLint* v = GetSharedMemoryAs<const GLint*>(
      c.v_shm_id, c.v_shm_offset, data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform2iv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2ivImmediate(
    uint32 immediate_data_size, const gles2::Uniform2ivImmediate& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLint), 2, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLint* v = GetImmediateDataAs<const GLint*>(
      c, data_size, immediate_data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform2iv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3f(
    uint32 immediate_data_size, const gles2::Uniform3f& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat temp[3] = { x, y, z, };
  DoUniform3fv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3fv(
    uint32 immediate_data_size, const gles2::Uniform3fv& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 3, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLfloat* v = GetSharedMemoryAs<const GLfloat*>(
      c.v_shm_id, c.v_shm_offset, data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform3fv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3fvImmediate(
    uint32 immediate_data_size, const gles2::Uniform3fvImmediate& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 3, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLfloat* v = GetImmediateDataAs<const GLfloat*>(
      c, data_size, immediate_data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform3fv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3i(
    uint32 immediate_data_size, const gles2::Uniform3i& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  GLint temp[3] = { x, y, z, };
  DoUniform3iv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3iv(
    uint32 immediate_data_size, const gles2::Uniform3iv& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLint), 3, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLint* v = GetSharedMemoryAs<const GLint*>(
      c.v_shm_id, c.v_shm_offset, data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform3iv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3ivImmediate(
    uint32 immediate_data_size, const gles2::Uniform3ivImmediate& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLint), 3, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLint* v = GetImmediateDataAs<const GLint*>(
      c, data_size, immediate_data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform3iv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4f(
    uint32 immediate_data_size, const gles2::Uniform4f& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat w = static_cast<GLfloat>(c.w);
  GLfloat temp[4] = { x, y, z, w, };
  DoUniform4fv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4fv(
    uint32 immediate_data_size, const gles2::Uniform4fv& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 4, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLfloat* v = GetSharedMemoryAs<const GLfloat*>(
      c.v_shm_id, c.v_shm_offset, data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform4fv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4fvImmediate(
    uint32 immediate_data_size, const gles2::Uniform4fvImmediate& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 4, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLfloat* v = GetImmediateDataAs<const GLfloat*>(
      c, data_size, immediate_data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform4fv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4i(
    uint32 immediate_data_size, const gles2::Uniform4i& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  GLint w = static_cast<GLint>(c.w);
  GLint temp[4] = { x, y, z, w, };
  DoUniform4iv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4iv(
    uint32 immediate_data_size, const gles2::Uniform4iv& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLint), 4, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLint* v = GetSharedMemoryAs<const GLint*>(
      c.v_shm_id, c.v_shm_offset, data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform4iv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4ivImmediate(
    uint32 immediate_data_size, const gles2::Uniform4ivImmediate& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLint), 4, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLint* v = GetImmediateDataAs<const GLint*>(
      c, data_size, immediate_data_size);
  if (v == NULL) {
    return error::kOutOfBounds;
  }
  DoUniform4iv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix2fv(
    uint32 immediate_data_size, const gles2::UniformMatrix2fv& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 4, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLfloat* value = GetSharedMemoryAs<const GLfloat*>(
      c.value_shm_id, c.value_shm_offset, data_size);
  if (!validators_->false_only.IsValid(transpose)) {
    SetGLError(
        GL_INVALID_VALUE, "glUniformMatrix2fv: transpose GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (value == NULL) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix2fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix2fvImmediate(
    uint32 immediate_data_size, const gles2::UniformMatrix2fvImmediate& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 4, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLfloat* value = GetImmediateDataAs<const GLfloat*>(
      c, data_size, immediate_data_size);
  if (!validators_->false_only.IsValid(transpose)) {
    SetGLError(
        GL_INVALID_VALUE, "glUniformMatrix2fv: transpose GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (value == NULL) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix2fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix3fv(
    uint32 immediate_data_size, const gles2::UniformMatrix3fv& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 9, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLfloat* value = GetSharedMemoryAs<const GLfloat*>(
      c.value_shm_id, c.value_shm_offset, data_size);
  if (!validators_->false_only.IsValid(transpose)) {
    SetGLError(
        GL_INVALID_VALUE, "glUniformMatrix3fv: transpose GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (value == NULL) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix3fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix3fvImmediate(
    uint32 immediate_data_size, const gles2::UniformMatrix3fvImmediate& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 9, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLfloat* value = GetImmediateDataAs<const GLfloat*>(
      c, data_size, immediate_data_size);
  if (!validators_->false_only.IsValid(transpose)) {
    SetGLError(
        GL_INVALID_VALUE, "glUniformMatrix3fv: transpose GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (value == NULL) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix3fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix4fv(
    uint32 immediate_data_size, const gles2::UniformMatrix4fv& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 16, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLfloat* value = GetSharedMemoryAs<const GLfloat*>(
      c.value_shm_id, c.value_shm_offset, data_size);
  if (!validators_->false_only.IsValid(transpose)) {
    SetGLError(
        GL_INVALID_VALUE, "glUniformMatrix4fv: transpose GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (value == NULL) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix4fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix4fvImmediate(
    uint32 immediate_data_size, const gles2::UniformMatrix4fvImmediate& c) {
  GLint location = program_manager()->UnswizzleLocation(
      static_cast<GLint>(c.location));
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLfloat), 16, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLfloat* value = GetImmediateDataAs<const GLfloat*>(
      c, data_size, immediate_data_size);
  if (!validators_->false_only.IsValid(transpose)) {
    SetGLError(
        GL_INVALID_VALUE, "glUniformMatrix4fv: transpose GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (value == NULL) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix4fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUseProgram(
    uint32 immediate_data_size, const gles2::UseProgram& c) {
  GLuint program = c.program;
  DoUseProgram(program);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleValidateProgram(
    uint32 immediate_data_size, const gles2::ValidateProgram& c) {
  GLuint program = c.program;
  DoValidateProgram(program);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib1f(
    uint32 immediate_data_size, const gles2::VertexAttrib1f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  DoVertexAttrib1f(indx, x);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib1fv(
    uint32 immediate_data_size, const gles2::VertexAttrib1fv& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLfloat), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLfloat* values = GetSharedMemoryAs<const GLfloat*>(
      c.values_shm_id, c.values_shm_offset, data_size);
  if (values == NULL) {
    return error::kOutOfBounds;
  }
  DoVertexAttrib1fv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib1fvImmediate(
    uint32 immediate_data_size, const gles2::VertexAttrib1fvImmediate& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLfloat), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLfloat* values = GetImmediateDataAs<const GLfloat*>(
      c, data_size, immediate_data_size);
  if (values == NULL) {
    return error::kOutOfBounds;
  }
  DoVertexAttrib1fv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib2f(
    uint32 immediate_data_size, const gles2::VertexAttrib2f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  DoVertexAttrib2f(indx, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib2fv(
    uint32 immediate_data_size, const gles2::VertexAttrib2fv& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLfloat), 2, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLfloat* values = GetSharedMemoryAs<const GLfloat*>(
      c.values_shm_id, c.values_shm_offset, data_size);
  if (values == NULL) {
    return error::kOutOfBounds;
  }
  DoVertexAttrib2fv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib2fvImmediate(
    uint32 immediate_data_size, const gles2::VertexAttrib2fvImmediate& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLfloat), 2, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLfloat* values = GetImmediateDataAs<const GLfloat*>(
      c, data_size, immediate_data_size);
  if (values == NULL) {
    return error::kOutOfBounds;
  }
  DoVertexAttrib2fv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib3f(
    uint32 immediate_data_size, const gles2::VertexAttrib3f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  DoVertexAttrib3f(indx, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib3fv(
    uint32 immediate_data_size, const gles2::VertexAttrib3fv& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLfloat), 3, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLfloat* values = GetSharedMemoryAs<const GLfloat*>(
      c.values_shm_id, c.values_shm_offset, data_size);
  if (values == NULL) {
    return error::kOutOfBounds;
  }
  DoVertexAttrib3fv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib3fvImmediate(
    uint32 immediate_data_size, const gles2::VertexAttrib3fvImmediate& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLfloat), 3, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLfloat* values = GetImmediateDataAs<const GLfloat*>(
      c, data_size, immediate_data_size);
  if (values == NULL) {
    return error::kOutOfBounds;
  }
  DoVertexAttrib3fv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib4f(
    uint32 immediate_data_size, const gles2::VertexAttrib4f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat w = static_cast<GLfloat>(c.w);
  DoVertexAttrib4f(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib4fv(
    uint32 immediate_data_size, const gles2::VertexAttrib4fv& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLfloat), 4, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLfloat* values = GetSharedMemoryAs<const GLfloat*>(
      c.values_shm_id, c.values_shm_offset, data_size);
  if (values == NULL) {
    return error::kOutOfBounds;
  }
  DoVertexAttrib4fv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib4fvImmediate(
    uint32 immediate_data_size, const gles2::VertexAttrib4fvImmediate& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLfloat), 4, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLfloat* values = GetImmediateDataAs<const GLfloat*>(
      c, data_size, immediate_data_size);
  if (values == NULL) {
    return error::kOutOfBounds;
  }
  DoVertexAttrib4fv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleViewport(
    uint32 immediate_data_size, const gles2::Viewport& c) {
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glViewport: width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glViewport: height < 0");
    return error::kNoError;
  }
  DoViewport(x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlitFramebufferEXT(
    uint32 immediate_data_size, const gles2::BlitFramebufferEXT& c) {
  GLint srcX0 = static_cast<GLint>(c.srcX0);
  GLint srcY0 = static_cast<GLint>(c.srcY0);
  GLint srcX1 = static_cast<GLint>(c.srcX1);
  GLint srcY1 = static_cast<GLint>(c.srcY1);
  GLint dstX0 = static_cast<GLint>(c.dstX0);
  GLint dstY0 = static_cast<GLint>(c.dstY0);
  GLint dstX1 = static_cast<GLint>(c.dstX1);
  GLint dstY1 = static_cast<GLint>(c.dstY1);
  GLbitfield mask = static_cast<GLbitfield>(c.mask);
  GLenum filter = static_cast<GLenum>(c.filter);
  if (!validators_->blit_filter.IsValid(filter)) {
    SetGLError(
        GL_INVALID_ENUM, "glBlitFramebufferEXT: filter GL_INVALID_ENUM");
    return error::kNoError;
  }
  DoBlitFramebufferEXT(
      srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleRenderbufferStorageMultisampleEXT(
    uint32 immediate_data_size,
    const gles2::RenderbufferStorageMultisampleEXT& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei samples = static_cast<GLsizei>(c.samples);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->render_buffer_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM,
        "glRenderbufferStorageMultisampleEXT: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (samples < 0) {
    SetGLError(
        GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT: samples < 0");
    return error::kNoError;
  }
  if (!validators_->render_buffer_format.IsValid(internalformat)) {
    SetGLError(
        GL_INVALID_ENUM,
        "glRenderbufferStorageMultisampleEXT: internalformat GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(
        GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT: width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(
        GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT: height < 0");
    return error::kNoError;
  }
  DoRenderbufferStorageMultisample(
      target, samples, internalformat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexStorage2DEXT(
    uint32 immediate_data_size, const gles2::TexStorage2DEXT& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei levels = static_cast<GLsizei>(c.levels);
  GLenum internalFormat = static_cast<GLenum>(c.internalFormat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLError(GL_INVALID_ENUM, "glTexStorage2DEXT: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (levels < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage2DEXT: levels < 0");
    return error::kNoError;
  }
  if (!validators_->texture_internal_format_storage.IsValid(internalFormat)) {
    SetGLError(
        GL_INVALID_ENUM, "glTexStorage2DEXT: internalFormat GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage2DEXT: width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage2DEXT: height < 0");
    return error::kNoError;
  }
  DoTexStorage2DEXT(target, levels, internalFormat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenQueriesEXT(
    uint32 immediate_data_size, const gles2::GenQueriesEXT& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* queries = GetSharedMemoryAs<GLuint*>(
      c.queries_shm_id, c.queries_shm_offset, data_size);
  if (queries == NULL) {
    return error::kOutOfBounds;
  }
  if (!GenQueriesEXTHelper(n, queries)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenQueriesEXTImmediate(
    uint32 immediate_data_size, const gles2::GenQueriesEXTImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* queries = GetImmediateDataAs<GLuint*>(
      c, data_size, immediate_data_size);
  if (queries == NULL) {
    return error::kOutOfBounds;
  }
  if (!GenQueriesEXTHelper(n, queries)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteQueriesEXT(
    uint32 immediate_data_size, const gles2::DeleteQueriesEXT& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* queries = GetSharedMemoryAs<const GLuint*>(
      c.queries_shm_id, c.queries_shm_offset, data_size);
  if (queries == NULL) {
    return error::kOutOfBounds;
  }
  DeleteQueriesEXTHelper(n, queries);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteQueriesEXTImmediate(
    uint32 immediate_data_size, const gles2::DeleteQueriesEXTImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* queries = GetImmediateDataAs<const GLuint*>(
      c, data_size, immediate_data_size);
  if (queries == NULL) {
    return error::kOutOfBounds;
  }
  DeleteQueriesEXTHelper(n, queries);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetMaxValueInBufferCHROMIUM(
    uint32 immediate_data_size, const gles2::GetMaxValueInBufferCHROMIUM& c) {
  GLuint buffer_id = c.buffer_id;
  GLsizei count = static_cast<GLsizei>(c.count);
  GLenum type = static_cast<GLenum>(c.type);
  GLuint offset = static_cast<GLuint>(c.offset);
  typedef GetMaxValueInBufferCHROMIUM::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetMaxValueInBufferCHROMIUM: count < 0");
    return error::kNoError;
  }
  if (!validators_->get_max_index_type.IsValid(type)) {
    SetGLError(
        GL_INVALID_ENUM,
        "glGetMaxValueInBufferCHROMIUM: type GL_INVALID_ENUM");
    return error::kNoError;
  }
  *result_dst = DoGetMaxValueInBufferCHROMIUM(buffer_id, count, type, offset);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexImageIOSurface2DCHROMIUM(
    uint32 immediate_data_size, const gles2::TexImageIOSurface2DCHROMIUM& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLuint ioSurfaceId = static_cast<GLuint>(c.ioSurfaceId);
  GLuint plane = static_cast<GLuint>(c.plane);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM,
        "glTexImageIOSurface2DCHROMIUM: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexImageIOSurface2DCHROMIUM: width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexImageIOSurface2DCHROMIUM: height < 0");
    return error::kNoError;
  }
  DoTexImageIOSurface2DCHROMIUM(target, width, height, ioSurfaceId, plane);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCopyTextureCHROMIUM(
    uint32 immediate_data_size, const gles2::CopyTextureCHROMIUM& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum source_id = static_cast<GLenum>(c.source_id);
  GLenum dest_id = static_cast<GLenum>(c.dest_id);
  GLint level = static_cast<GLint>(c.level);
  GLint internalformat = static_cast<GLint>(c.internalformat);
  if (!validators_->texture_internal_format.IsValid(internalformat)) {
    SetGLError(
        GL_INVALID_VALUE,
        "glCopyTextureCHROMIUM: internalformat GL_INVALID_VALUE");
    return error::kNoError;
  }
  DoCopyTextureCHROMIUM(target, source_id, dest_id, level, internalformat);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleProduceTextureCHROMIUM(
    uint32 immediate_data_size, const gles2::ProduceTextureCHROMIUM& c) {
  GLenum target = static_cast<GLenum>(c.target);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLbyte), 64, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLbyte* mailbox = GetSharedMemoryAs<const GLbyte*>(
      c.mailbox_shm_id, c.mailbox_shm_offset, data_size);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM, "glProduceTextureCHROMIUM: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (mailbox == NULL) {
    return error::kOutOfBounds;
  }
  DoProduceTextureCHROMIUM(target, mailbox);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleProduceTextureCHROMIUMImmediate(
    uint32 immediate_data_size,
    const gles2::ProduceTextureCHROMIUMImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLbyte), 64, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLbyte* mailbox = GetImmediateDataAs<const GLbyte*>(
      c, data_size, immediate_data_size);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM, "glProduceTextureCHROMIUM: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (mailbox == NULL) {
    return error::kOutOfBounds;
  }
  DoProduceTextureCHROMIUM(target, mailbox);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleConsumeTextureCHROMIUM(
    uint32 immediate_data_size, const gles2::ConsumeTextureCHROMIUM& c) {
  GLenum target = static_cast<GLenum>(c.target);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLbyte), 64, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLbyte* mailbox = GetSharedMemoryAs<const GLbyte*>(
      c.mailbox_shm_id, c.mailbox_shm_offset, data_size);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM, "glConsumeTextureCHROMIUM: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (mailbox == NULL) {
    return error::kOutOfBounds;
  }
  DoConsumeTextureCHROMIUM(target, mailbox);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleConsumeTextureCHROMIUMImmediate(
    uint32 immediate_data_size,
    const gles2::ConsumeTextureCHROMIUMImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLbyte), 64, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLbyte* mailbox = GetImmediateDataAs<const GLbyte*>(
      c, data_size, immediate_data_size);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLError(
        GL_INVALID_ENUM, "glConsumeTextureCHROMIUM: target GL_INVALID_ENUM");
    return error::kNoError;
  }
  if (mailbox == NULL) {
    return error::kOutOfBounds;
  }
  DoConsumeTextureCHROMIUM(target, mailbox);
  return error::kNoError;
}

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_AUTOGEN_H_

