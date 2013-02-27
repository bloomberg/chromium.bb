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
    uint32 immediate_data_size, const gles2::cmds::ActiveTexture& c) {
  GLenum texture = static_cast<GLenum>(c.texture);
  DoActiveTexture(texture);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleAttachShader(
    uint32 immediate_data_size, const gles2::cmds::AttachShader& c) {
  GLuint program = c.program;
  GLuint shader = c.shader;
  DoAttachShader(program, shader);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindBuffer(
    uint32 immediate_data_size, const gles2::cmds::BindBuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint buffer = c.buffer;
  if (!validators_->buffer_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glBindBuffer", target, "target");
    return error::kNoError;
  }
  DoBindBuffer(target, buffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindFramebuffer(
    uint32 immediate_data_size, const gles2::cmds::BindFramebuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint framebuffer = c.framebuffer;
  if (!validators_->frame_buffer_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glBindFramebuffer", target, "target");
    return error::kNoError;
  }
  DoBindFramebuffer(target, framebuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindRenderbuffer(
    uint32 immediate_data_size, const gles2::cmds::BindRenderbuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint renderbuffer = c.renderbuffer;
  if (!validators_->render_buffer_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glBindRenderbuffer", target, "target");
    return error::kNoError;
  }
  DoBindRenderbuffer(target, renderbuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindTexture(
    uint32 immediate_data_size, const gles2::cmds::BindTexture& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLuint texture = c.texture;
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glBindTexture", target, "target");
    return error::kNoError;
  }
  DoBindTexture(target, texture);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendColor(
    uint32 immediate_data_size, const gles2::cmds::BlendColor& c) {
  GLclampf red = static_cast<GLclampf>(c.red);
  GLclampf green = static_cast<GLclampf>(c.green);
  GLclampf blue = static_cast<GLclampf>(c.blue);
  GLclampf alpha = static_cast<GLclampf>(c.alpha);
  if (state_.blend_color_red != red ||
      state_.blend_color_green != green ||
      state_.blend_color_blue != blue ||
      state_.blend_color_alpha != alpha) {
    state_.blend_color_red = red;
    state_.blend_color_green = green;
    state_.blend_color_blue = blue;
    state_.blend_color_alpha = alpha;
    glBlendColor(red, green, blue, alpha);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendEquation(
    uint32 immediate_data_size, const gles2::cmds::BlendEquation& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  if (!validators_->equation.IsValid(mode)) {
    SetGLErrorInvalidEnum("glBlendEquation", mode, "mode");
    return error::kNoError;
  }
  if (state_.blend_equation_rgb != mode ||
      state_.blend_equation_alpha != mode) {
    state_.blend_equation_rgb = mode;
    state_.blend_equation_alpha = mode;
    glBlendEquation(mode);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendEquationSeparate(
    uint32 immediate_data_size, const gles2::cmds::BlendEquationSeparate& c) {
  GLenum modeRGB = static_cast<GLenum>(c.modeRGB);
  GLenum modeAlpha = static_cast<GLenum>(c.modeAlpha);
  if (!validators_->equation.IsValid(modeRGB)) {
    SetGLErrorInvalidEnum("glBlendEquationSeparate", modeRGB, "modeRGB");
    return error::kNoError;
  }
  if (!validators_->equation.IsValid(modeAlpha)) {
    SetGLErrorInvalidEnum("glBlendEquationSeparate", modeAlpha, "modeAlpha");
    return error::kNoError;
  }
  if (state_.blend_equation_rgb != modeRGB ||
      state_.blend_equation_alpha != modeAlpha) {
    state_.blend_equation_rgb = modeRGB;
    state_.blend_equation_alpha = modeAlpha;
    glBlendEquationSeparate(modeRGB, modeAlpha);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendFunc(
    uint32 immediate_data_size, const gles2::cmds::BlendFunc& c) {
  GLenum sfactor = static_cast<GLenum>(c.sfactor);
  GLenum dfactor = static_cast<GLenum>(c.dfactor);
  if (!validators_->src_blend_factor.IsValid(sfactor)) {
    SetGLErrorInvalidEnum("glBlendFunc", sfactor, "sfactor");
    return error::kNoError;
  }
  if (!validators_->dst_blend_factor.IsValid(dfactor)) {
    SetGLErrorInvalidEnum("glBlendFunc", dfactor, "dfactor");
    return error::kNoError;
  }
  if (state_.blend_source_rgb != sfactor ||
      state_.blend_dest_rgb != dfactor ||
      state_.blend_source_alpha != sfactor ||
      state_.blend_dest_alpha != dfactor) {
    state_.blend_source_rgb = sfactor;
    state_.blend_dest_rgb = dfactor;
    state_.blend_source_alpha = sfactor;
    state_.blend_dest_alpha = dfactor;
    glBlendFunc(sfactor, dfactor);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendFuncSeparate(
    uint32 immediate_data_size, const gles2::cmds::BlendFuncSeparate& c) {
  GLenum srcRGB = static_cast<GLenum>(c.srcRGB);
  GLenum dstRGB = static_cast<GLenum>(c.dstRGB);
  GLenum srcAlpha = static_cast<GLenum>(c.srcAlpha);
  GLenum dstAlpha = static_cast<GLenum>(c.dstAlpha);
  if (!validators_->src_blend_factor.IsValid(srcRGB)) {
    SetGLErrorInvalidEnum("glBlendFuncSeparate", srcRGB, "srcRGB");
    return error::kNoError;
  }
  if (!validators_->dst_blend_factor.IsValid(dstRGB)) {
    SetGLErrorInvalidEnum("glBlendFuncSeparate", dstRGB, "dstRGB");
    return error::kNoError;
  }
  if (!validators_->src_blend_factor.IsValid(srcAlpha)) {
    SetGLErrorInvalidEnum("glBlendFuncSeparate", srcAlpha, "srcAlpha");
    return error::kNoError;
  }
  if (!validators_->dst_blend_factor.IsValid(dstAlpha)) {
    SetGLErrorInvalidEnum("glBlendFuncSeparate", dstAlpha, "dstAlpha");
    return error::kNoError;
  }
  if (state_.blend_source_rgb != srcRGB ||
      state_.blend_dest_rgb != dstRGB ||
      state_.blend_source_alpha != srcAlpha ||
      state_.blend_dest_alpha != dstAlpha) {
    state_.blend_source_rgb = srcRGB;
    state_.blend_dest_rgb = dstRGB;
    state_.blend_source_alpha = srcAlpha;
    state_.blend_dest_alpha = dstAlpha;
    glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBufferSubData(
    uint32 immediate_data_size, const gles2::cmds::BufferSubData& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32 data_size = size;
  const void* data = GetSharedMemoryAs<const void*>(
      c.data_shm_id, c.data_shm_offset, data_size);
  if (!validators_->buffer_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glBufferSubData", target, "target");
    return error::kNoError;
  }
  if (size < 0) {
    SetGLError(GL_INVALID_VALUE, "glBufferSubData", "size < 0");
    return error::kNoError;
  }
  if (data == NULL) {
    return error::kOutOfBounds;
  }
  DoBufferSubData(target, offset, size, data);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBufferSubDataImmediate(
    uint32 immediate_data_size, const gles2::cmds::BufferSubDataImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32 data_size = size;
  const void* data = GetImmediateDataAs<const void*>(
      c, data_size, immediate_data_size);
  if (!validators_->buffer_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glBufferSubData", target, "target");
    return error::kNoError;
  }
  if (size < 0) {
    SetGLError(GL_INVALID_VALUE, "glBufferSubData", "size < 0");
    return error::kNoError;
  }
  if (data == NULL) {
    return error::kOutOfBounds;
  }
  DoBufferSubData(target, offset, size, data);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCheckFramebufferStatus(
    uint32 immediate_data_size, const gles2::cmds::CheckFramebufferStatus& c) {
  GLenum target = static_cast<GLenum>(c.target);
  typedef cmds::CheckFramebufferStatus::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  if (!validators_->frame_buffer_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glCheckFramebufferStatus", target, "target");
    return error::kNoError;
  }
  *result_dst = DoCheckFramebufferStatus(target);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClear(
    uint32 immediate_data_size, const gles2::cmds::Clear& c) {
  if (ShouldDeferDraws())
    return error::kDeferCommandUntilLater;
  GLbitfield mask = static_cast<GLbitfield>(c.mask);
  DoClear(mask);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClearColor(
    uint32 immediate_data_size, const gles2::cmds::ClearColor& c) {
  GLclampf red = static_cast<GLclampf>(c.red);
  GLclampf green = static_cast<GLclampf>(c.green);
  GLclampf blue = static_cast<GLclampf>(c.blue);
  GLclampf alpha = static_cast<GLclampf>(c.alpha);
  if (state_.color_clear_red != red ||
      state_.color_clear_green != green ||
      state_.color_clear_blue != blue ||
      state_.color_clear_alpha != alpha) {
    state_.color_clear_red = red;
    state_.color_clear_green = green;
    state_.color_clear_blue = blue;
    state_.color_clear_alpha = alpha;
    glClearColor(red, green, blue, alpha);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClearDepthf(
    uint32 immediate_data_size, const gles2::cmds::ClearDepthf& c) {
  GLclampf depth = static_cast<GLclampf>(c.depth);
  if (state_.depth_clear != depth) {
    state_.depth_clear = depth;
    glClearDepth(depth);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClearStencil(
    uint32 immediate_data_size, const gles2::cmds::ClearStencil& c) {
  GLint s = static_cast<GLint>(c.s);
  if (state_.stencil_clear != s) {
    state_.stencil_clear = s;
    glClearStencil(s);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleColorMask(
    uint32 immediate_data_size, const gles2::cmds::ColorMask& c) {
  GLboolean red = static_cast<GLboolean>(c.red);
  GLboolean green = static_cast<GLboolean>(c.green);
  GLboolean blue = static_cast<GLboolean>(c.blue);
  GLboolean alpha = static_cast<GLboolean>(c.alpha);
  if (state_.color_mask_red != red ||
      state_.color_mask_green != green ||
      state_.color_mask_blue != blue ||
      state_.color_mask_alpha != alpha) {
    state_.color_mask_red = red;
    state_.color_mask_green = green;
    state_.color_mask_blue = blue;
    state_.color_mask_alpha = alpha;
    clear_state_dirty_ = true;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCompileShader(
    uint32 immediate_data_size, const gles2::cmds::CompileShader& c) {
  GLuint shader = c.shader;
  DoCompileShader(shader);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCompressedTexSubImage2D(
    uint32 immediate_data_size,
    const gles2::cmds::CompressedTexSubImage2D& c) {
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
    SetGLErrorInvalidEnum("glCompressedTexSubImage2D", target, "target");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D", "height < 0");
    return error::kNoError;
  }
  if (!validators_->compressed_texture_format.IsValid(format)) {
    SetGLErrorInvalidEnum("glCompressedTexSubImage2D", format, "format");
    return error::kNoError;
  }
  if (imageSize < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D", "imageSize < 0");
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
    const gles2::cmds::CompressedTexSubImage2DImmediate& c) {
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
    SetGLErrorInvalidEnum("glCompressedTexSubImage2D", target, "target");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D", "height < 0");
    return error::kNoError;
  }
  if (!validators_->compressed_texture_format.IsValid(format)) {
    SetGLErrorInvalidEnum("glCompressedTexSubImage2D", format, "format");
    return error::kNoError;
  }
  if (imageSize < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D", "imageSize < 0");
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
    uint32 immediate_data_size, const gles2::cmds::CopyTexImage2D& c) {
  if (ShouldDeferReads())
    return error::kDeferCommandUntilLater;
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glCopyTexImage2D", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_internal_format.IsValid(internalformat)) {
    SetGLErrorInvalidEnum("glCopyTexImage2D", internalformat,
    "internalformat");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexImage2D", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexImage2D", "height < 0");
    return error::kNoError;
  }
  if (!validators_->texture_border.IsValid(border)) {
    SetGLError(
        GL_INVALID_VALUE, "glCopyTexImage2D", "border GL_INVALID_VALUE");
    return error::kNoError;
  }
  DoCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCopyTexSubImage2D(
    uint32 immediate_data_size, const gles2::cmds::CopyTexSubImage2D& c) {
  if (ShouldDeferReads())
    return error::kDeferCommandUntilLater;
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glCopyTexSubImage2D", target, "target");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexSubImage2D", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyTexSubImage2D", "height < 0");
    return error::kNoError;
  }
  DoCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCreateProgram(
    uint32 immediate_data_size, const gles2::cmds::CreateProgram& c) {
  uint32 client_id = c.client_id;
  if (!CreateProgramHelper(client_id)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCreateShader(
    uint32 immediate_data_size, const gles2::cmds::CreateShader& c) {
  GLenum type = static_cast<GLenum>(c.type);
  if (!validators_->shader_type.IsValid(type)) {
    SetGLErrorInvalidEnum("glCreateShader", type, "type");
    return error::kNoError;
  }
  uint32 client_id = c.client_id;
  if (!CreateShaderHelper(type, client_id)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCullFace(
    uint32 immediate_data_size, const gles2::cmds::CullFace& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  if (!validators_->face_type.IsValid(mode)) {
    SetGLErrorInvalidEnum("glCullFace", mode, "mode");
    return error::kNoError;
  }
  if (state_.cull_mode != mode) {
    state_.cull_mode = mode;
    glCullFace(mode);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteBuffers(
    uint32 immediate_data_size, const gles2::cmds::DeleteBuffers& c) {
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
    uint32 immediate_data_size, const gles2::cmds::DeleteBuffersImmediate& c) {
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
    uint32 immediate_data_size, const gles2::cmds::DeleteFramebuffers& c) {
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
    uint32 immediate_data_size,
    const gles2::cmds::DeleteFramebuffersImmediate& c) {
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
    uint32 immediate_data_size, const gles2::cmds::DeleteRenderbuffers& c) {
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
    uint32 immediate_data_size,
    const gles2::cmds::DeleteRenderbuffersImmediate& c) {
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
    uint32 immediate_data_size, const gles2::cmds::DeleteTextures& c) {
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
    uint32 immediate_data_size,
    const gles2::cmds::DeleteTexturesImmediate& c) {
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
    uint32 immediate_data_size, const gles2::cmds::DepthFunc& c) {
  GLenum func = static_cast<GLenum>(c.func);
  if (!validators_->cmp_function.IsValid(func)) {
    SetGLErrorInvalidEnum("glDepthFunc", func, "func");
    return error::kNoError;
  }
  if (state_.depth_func != func) {
    state_.depth_func = func;
    glDepthFunc(func);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDepthMask(
    uint32 immediate_data_size, const gles2::cmds::DepthMask& c) {
  GLboolean flag = static_cast<GLboolean>(c.flag);
  if (state_.depth_mask != flag) {
    state_.depth_mask = flag;
    clear_state_dirty_ = true;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDepthRangef(
    uint32 immediate_data_size, const gles2::cmds::DepthRangef& c) {
  GLclampf zNear = static_cast<GLclampf>(c.zNear);
  GLclampf zFar = static_cast<GLclampf>(c.zFar);
  DoDepthRangef(zNear, zFar);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDetachShader(
    uint32 immediate_data_size, const gles2::cmds::DetachShader& c) {
  GLuint program = c.program;
  GLuint shader = c.shader;
  DoDetachShader(program, shader);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDisable(
    uint32 immediate_data_size, const gles2::cmds::Disable& c) {
  GLenum cap = static_cast<GLenum>(c.cap);
  if (!validators_->capability.IsValid(cap)) {
    SetGLErrorInvalidEnum("glDisable", cap, "cap");
    return error::kNoError;
  }
  DoDisable(cap);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDisableVertexAttribArray(
    uint32 immediate_data_size,
    const gles2::cmds::DisableVertexAttribArray& c) {
  GLuint index = static_cast<GLuint>(c.index);
  DoDisableVertexAttribArray(index);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleEnable(
    uint32 immediate_data_size, const gles2::cmds::Enable& c) {
  GLenum cap = static_cast<GLenum>(c.cap);
  if (!validators_->capability.IsValid(cap)) {
    SetGLErrorInvalidEnum("glEnable", cap, "cap");
    return error::kNoError;
  }
  DoEnable(cap);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleEnableVertexAttribArray(
    uint32 immediate_data_size,
    const gles2::cmds::EnableVertexAttribArray& c) {
  GLuint index = static_cast<GLuint>(c.index);
  DoEnableVertexAttribArray(index);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFinish(
    uint32 immediate_data_size, const gles2::cmds::Finish& c) {
  DoFinish();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFlush(
    uint32 immediate_data_size, const gles2::cmds::Flush& c) {
  DoFlush();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFramebufferRenderbuffer(
    uint32 immediate_data_size,
    const gles2::cmds::FramebufferRenderbuffer& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum renderbuffertarget = static_cast<GLenum>(c.renderbuffertarget);
  GLuint renderbuffer = c.renderbuffer;
  if (!validators_->frame_buffer_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glFramebufferRenderbuffer", target, "target");
    return error::kNoError;
  }
  if (!validators_->attachment.IsValid(attachment)) {
    SetGLErrorInvalidEnum("glFramebufferRenderbuffer", attachment,
    "attachment");
    return error::kNoError;
  }
  if (!validators_->render_buffer_target.IsValid(renderbuffertarget)) {
    SetGLErrorInvalidEnum("glFramebufferRenderbuffer", renderbuffertarget,
    "renderbuffertarget");
    return error::kNoError;
  }
  DoFramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFramebufferTexture2D(
    uint32 immediate_data_size, const gles2::cmds::FramebufferTexture2D& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum textarget = static_cast<GLenum>(c.textarget);
  GLuint texture = c.texture;
  GLint level = static_cast<GLint>(c.level);
  if (!validators_->frame_buffer_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glFramebufferTexture2D", target, "target");
    return error::kNoError;
  }
  if (!validators_->attachment.IsValid(attachment)) {
    SetGLErrorInvalidEnum("glFramebufferTexture2D", attachment, "attachment");
    return error::kNoError;
  }
  if (!validators_->texture_target.IsValid(textarget)) {
    SetGLErrorInvalidEnum("glFramebufferTexture2D", textarget, "textarget");
    return error::kNoError;
  }
  if (!validators_->zero_only.IsValid(level)) {
    SetGLError(
        GL_INVALID_VALUE, "glFramebufferTexture2D", "level GL_INVALID_VALUE");
    return error::kNoError;
  }
  DoFramebufferTexture2D(target, attachment, textarget, texture, level);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFrontFace(
    uint32 immediate_data_size, const gles2::cmds::FrontFace& c) {
  GLenum mode = static_cast<GLenum>(c.mode);
  if (!validators_->face_mode.IsValid(mode)) {
    SetGLErrorInvalidEnum("glFrontFace", mode, "mode");
    return error::kNoError;
  }
  if (state_.front_face != mode) {
    state_.front_face = mode;
    glFrontFace(mode);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenBuffers(
    uint32 immediate_data_size, const gles2::cmds::GenBuffers& c) {
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
    uint32 immediate_data_size, const gles2::cmds::GenBuffersImmediate& c) {
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
    uint32 immediate_data_size, const gles2::cmds::GenerateMipmap& c) {
  GLenum target = static_cast<GLenum>(c.target);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glGenerateMipmap", target, "target");
    return error::kNoError;
  }
  DoGenerateMipmap(target);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenFramebuffers(
    uint32 immediate_data_size, const gles2::cmds::GenFramebuffers& c) {
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
    uint32 immediate_data_size,
    const gles2::cmds::GenFramebuffersImmediate& c) {
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
    uint32 immediate_data_size, const gles2::cmds::GenRenderbuffers& c) {
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
    uint32 immediate_data_size,
    const gles2::cmds::GenRenderbuffersImmediate& c) {
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
    uint32 immediate_data_size, const gles2::cmds::GenTextures& c) {
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
    uint32 immediate_data_size, const gles2::cmds::GenTexturesImmediate& c) {
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
    uint32 immediate_data_size, const gles2::cmds::GetBooleanv& c) {
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetBooleanv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLboolean* params = result ? result->GetData() : NULL;
  if (!validators_->g_l_state.IsValid(pname)) {
    SetGLErrorInvalidEnum("glGetBooleanv", pname, "pname");
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
    SetGLError(error, "", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetBufferParameteriv(
    uint32 immediate_data_size, const gles2::cmds::GetBufferParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetBufferParameteriv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->buffer_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glGetBufferParameteriv", target, "target");
    return error::kNoError;
  }
  if (!validators_->buffer_parameter.IsValid(pname)) {
    SetGLErrorInvalidEnum("glGetBufferParameteriv", pname, "pname");
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
    SetGLError(error, "", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetError(
    uint32 immediate_data_size, const gles2::cmds::GetError& c) {
  typedef cmds::GetError::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = GetGLError();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetFloatv(
    uint32 immediate_data_size, const gles2::cmds::GetFloatv& c) {
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetFloatv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLfloat* params = result ? result->GetData() : NULL;
  if (!validators_->g_l_state.IsValid(pname)) {
    SetGLErrorInvalidEnum("glGetFloatv", pname, "pname");
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
    SetGLError(error, "", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetFramebufferAttachmentParameteriv(
    uint32 immediate_data_size,
    const gles2::cmds::GetFramebufferAttachmentParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetFramebufferAttachmentParameteriv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->frame_buffer_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glGetFramebufferAttachmentParameteriv", target,
    "target");
    return error::kNoError;
  }
  if (!validators_->attachment.IsValid(attachment)) {
    SetGLErrorInvalidEnum("glGetFramebufferAttachmentParameteriv", attachment,
    "attachment");
    return error::kNoError;
  }
  if (!validators_->frame_buffer_parameter.IsValid(pname)) {
    SetGLErrorInvalidEnum("glGetFramebufferAttachmentParameteriv", pname,
    "pname");
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
    SetGLError(error, "", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetIntegerv(
    uint32 immediate_data_size, const gles2::cmds::GetIntegerv& c) {
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetIntegerv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->g_l_state.IsValid(pname)) {
    SetGLErrorInvalidEnum("glGetIntegerv", pname, "pname");
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
    SetGLError(error, "", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetProgramiv(
    uint32 immediate_data_size, const gles2::cmds::GetProgramiv& c) {
  GLuint program = c.program;
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetProgramiv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->program_parameter.IsValid(pname)) {
    SetGLErrorInvalidEnum("glGetProgramiv", pname, "pname");
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
    SetGLError(error, "", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetRenderbufferParameteriv(
    uint32 immediate_data_size,
    const gles2::cmds::GetRenderbufferParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetRenderbufferParameteriv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->render_buffer_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glGetRenderbufferParameteriv", target, "target");
    return error::kNoError;
  }
  if (!validators_->render_buffer_parameter.IsValid(pname)) {
    SetGLErrorInvalidEnum("glGetRenderbufferParameteriv", pname, "pname");
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
    SetGLError(error, "", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetShaderiv(
    uint32 immediate_data_size, const gles2::cmds::GetShaderiv& c) {
  GLuint shader = c.shader;
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetShaderiv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->shader_parameter.IsValid(pname)) {
    SetGLErrorInvalidEnum("glGetShaderiv", pname, "pname");
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
    SetGLError(error, "", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetTexParameterfv(
    uint32 immediate_data_size, const gles2::cmds::GetTexParameterfv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetTexParameterfv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLfloat* params = result ? result->GetData() : NULL;
  if (!validators_->get_tex_param_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glGetTexParameterfv", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLErrorInvalidEnum("glGetTexParameterfv", pname, "pname");
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
    SetGLError(error, "", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetTexParameteriv(
    uint32 immediate_data_size, const gles2::cmds::GetTexParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetTexParameteriv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->get_tex_param_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glGetTexParameteriv", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLErrorInvalidEnum("glGetTexParameteriv", pname, "pname");
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
    SetGLError(error, "", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetVertexAttribfv(
    uint32 immediate_data_size, const gles2::cmds::GetVertexAttribfv& c) {
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetVertexAttribfv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLfloat* params = result ? result->GetData() : NULL;
  if (!validators_->vertex_attribute.IsValid(pname)) {
    SetGLErrorInvalidEnum("glGetVertexAttribfv", pname, "pname");
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
    SetGLError(error, "", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetVertexAttribiv(
    uint32 immediate_data_size, const gles2::cmds::GetVertexAttribiv& c) {
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetVertexAttribiv::Result Result;
  GLsizei num_values = 0;
  GetNumValuesReturnedForGLGet(pname, &num_values);
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : NULL;
  if (!validators_->vertex_attribute.IsValid(pname)) {
    SetGLErrorInvalidEnum("glGetVertexAttribiv", pname, "pname");
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
    SetGLError(error, "", "");
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleHint(
    uint32 immediate_data_size, const gles2::cmds::Hint& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum mode = static_cast<GLenum>(c.mode);
  if (!validators_->hint_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glHint", target, "target");
    return error::kNoError;
  }
  if (!validators_->hint_mode.IsValid(mode)) {
    SetGLErrorInvalidEnum("glHint", mode, "mode");
    return error::kNoError;
  }
  DoHint(target, mode);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsBuffer(
    uint32 immediate_data_size, const gles2::cmds::IsBuffer& c) {
  GLuint buffer = c.buffer;
  typedef cmds::IsBuffer::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsBuffer(buffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsEnabled(
    uint32 immediate_data_size, const gles2::cmds::IsEnabled& c) {
  GLenum cap = static_cast<GLenum>(c.cap);
  typedef cmds::IsEnabled::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  if (!validators_->capability.IsValid(cap)) {
    SetGLErrorInvalidEnum("glIsEnabled", cap, "cap");
    return error::kNoError;
  }
  *result_dst = DoIsEnabled(cap);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsFramebuffer(
    uint32 immediate_data_size, const gles2::cmds::IsFramebuffer& c) {
  GLuint framebuffer = c.framebuffer;
  typedef cmds::IsFramebuffer::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsFramebuffer(framebuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsProgram(
    uint32 immediate_data_size, const gles2::cmds::IsProgram& c) {
  GLuint program = c.program;
  typedef cmds::IsProgram::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsProgram(program);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsRenderbuffer(
    uint32 immediate_data_size, const gles2::cmds::IsRenderbuffer& c) {
  GLuint renderbuffer = c.renderbuffer;
  typedef cmds::IsRenderbuffer::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsRenderbuffer(renderbuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsShader(
    uint32 immediate_data_size, const gles2::cmds::IsShader& c) {
  GLuint shader = c.shader;
  typedef cmds::IsShader::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsShader(shader);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsTexture(
    uint32 immediate_data_size, const gles2::cmds::IsTexture& c) {
  GLuint texture = c.texture;
  typedef cmds::IsTexture::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsTexture(texture);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleLineWidth(
    uint32 immediate_data_size, const gles2::cmds::LineWidth& c) {
  GLfloat width = static_cast<GLfloat>(c.width);
  if (width <= 0.0f) {
    SetGLError(GL_INVALID_VALUE, "LineWidth", "width out of range");
    return error::kNoError;
  }
  if (state_.line_width != width) {
    state_.line_width = width;
    glLineWidth(width);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleLinkProgram(
    uint32 immediate_data_size, const gles2::cmds::LinkProgram& c) {
  GLuint program = c.program;
  DoLinkProgram(program);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePolygonOffset(
    uint32 immediate_data_size, const gles2::cmds::PolygonOffset& c) {
  GLfloat factor = static_cast<GLfloat>(c.factor);
  GLfloat units = static_cast<GLfloat>(c.units);
  if (state_.polygon_offset_factor != factor ||
      state_.polygon_offset_units != units) {
    state_.polygon_offset_factor = factor;
    state_.polygon_offset_units = units;
    glPolygonOffset(factor, units);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleReleaseShaderCompiler(
    uint32 immediate_data_size, const gles2::cmds::ReleaseShaderCompiler& c) {
  DoReleaseShaderCompiler();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleRenderbufferStorage(
    uint32 immediate_data_size, const gles2::cmds::RenderbufferStorage& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->render_buffer_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glRenderbufferStorage", target, "target");
    return error::kNoError;
  }
  if (!validators_->render_buffer_format.IsValid(internalformat)) {
    SetGLErrorInvalidEnum("glRenderbufferStorage", internalformat,
    "internalformat");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorage", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glRenderbufferStorage", "height < 0");
    return error::kNoError;
  }
  DoRenderbufferStorage(target, internalformat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSampleCoverage(
    uint32 immediate_data_size, const gles2::cmds::SampleCoverage& c) {
  GLclampf value = static_cast<GLclampf>(c.value);
  GLboolean invert = static_cast<GLboolean>(c.invert);
  DoSampleCoverage(value, invert);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleScissor(
    uint32 immediate_data_size, const gles2::cmds::Scissor& c) {
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glScissor", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glScissor", "height < 0");
    return error::kNoError;
  }
  if (state_.scissor_x != x ||
      state_.scissor_y != y ||
      state_.scissor_width != width ||
      state_.scissor_height != height) {
    state_.scissor_x = x;
    state_.scissor_y = y;
    state_.scissor_width = width;
    state_.scissor_height = height;
    glScissor(x, y, width, height);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilFunc(
    uint32 immediate_data_size, const gles2::cmds::StencilFunc& c) {
  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  if (!validators_->cmp_function.IsValid(func)) {
    SetGLErrorInvalidEnum("glStencilFunc", func, "func");
    return error::kNoError;
  }
  if (state_.stencil_front_func != func ||
      state_.stencil_front_ref != ref ||
      state_.stencil_front_mask != mask ||
      state_.stencil_back_func != func ||
      state_.stencil_back_ref != ref ||
      state_.stencil_back_mask != mask) {
    state_.stencil_front_func = func;
    state_.stencil_front_ref = ref;
    state_.stencil_front_mask = mask;
    state_.stencil_back_func = func;
    state_.stencil_back_ref = ref;
    state_.stencil_back_mask = mask;
    glStencilFunc(func, ref, mask);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilFuncSeparate(
    uint32 immediate_data_size, const gles2::cmds::StencilFuncSeparate& c) {
  GLenum face = static_cast<GLenum>(c.face);
  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  if (!validators_->face_type.IsValid(face)) {
    SetGLErrorInvalidEnum("glStencilFuncSeparate", face, "face");
    return error::kNoError;
  }
  if (!validators_->cmp_function.IsValid(func)) {
    SetGLErrorInvalidEnum("glStencilFuncSeparate", func, "func");
    return error::kNoError;
  }
  bool changed = false;
  if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
    changed |= state_.stencil_front_func != func ||
        state_.stencil_front_ref != ref ||
        state_.stencil_front_mask != mask;
  }
  if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
    changed |= state_.stencil_back_func != func ||
        state_.stencil_back_ref != ref ||
        state_.stencil_back_mask != mask;
  }
  if (changed) {
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
      state_.stencil_front_func = func;
      state_.stencil_front_ref = ref;
      state_.stencil_front_mask = mask;
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
      state_.stencil_back_func = func;
      state_.stencil_back_ref = ref;
      state_.stencil_back_mask = mask;
    }
    glStencilFuncSeparate(face, func, ref, mask);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilMask(
    uint32 immediate_data_size, const gles2::cmds::StencilMask& c) {
  GLuint mask = static_cast<GLuint>(c.mask);
  if (state_.stencil_front_writemask != mask ||
      state_.stencil_back_writemask != mask) {
    state_.stencil_front_writemask = mask;
    state_.stencil_back_writemask = mask;
    clear_state_dirty_ = true;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilMaskSeparate(
    uint32 immediate_data_size, const gles2::cmds::StencilMaskSeparate& c) {
  GLenum face = static_cast<GLenum>(c.face);
  GLuint mask = static_cast<GLuint>(c.mask);
  if (!validators_->face_type.IsValid(face)) {
    SetGLErrorInvalidEnum("glStencilMaskSeparate", face, "face");
    return error::kNoError;
  }
  bool changed = false;
  if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
    changed |= state_.stencil_front_writemask != mask;
  }
  if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
    changed |= state_.stencil_back_writemask != mask;
  }
  if (changed) {
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
      state_.stencil_front_writemask = mask;
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
      state_.stencil_back_writemask = mask;
    }
    clear_state_dirty_ = true;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilOp(
    uint32 immediate_data_size, const gles2::cmds::StencilOp& c) {
  GLenum fail = static_cast<GLenum>(c.fail);
  GLenum zfail = static_cast<GLenum>(c.zfail);
  GLenum zpass = static_cast<GLenum>(c.zpass);
  if (!validators_->stencil_op.IsValid(fail)) {
    SetGLErrorInvalidEnum("glStencilOp", fail, "fail");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(zfail)) {
    SetGLErrorInvalidEnum("glStencilOp", zfail, "zfail");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(zpass)) {
    SetGLErrorInvalidEnum("glStencilOp", zpass, "zpass");
    return error::kNoError;
  }
  if (state_.stencil_front_fail_op != fail ||
      state_.stencil_front_z_fail_op != zfail ||
      state_.stencil_front_z_pass_op != zpass ||
      state_.stencil_back_fail_op != fail ||
      state_.stencil_back_z_fail_op != zfail ||
      state_.stencil_back_z_pass_op != zpass) {
    state_.stencil_front_fail_op = fail;
    state_.stencil_front_z_fail_op = zfail;
    state_.stencil_front_z_pass_op = zpass;
    state_.stencil_back_fail_op = fail;
    state_.stencil_back_z_fail_op = zfail;
    state_.stencil_back_z_pass_op = zpass;
    glStencilOp(fail, zfail, zpass);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilOpSeparate(
    uint32 immediate_data_size, const gles2::cmds::StencilOpSeparate& c) {
  GLenum face = static_cast<GLenum>(c.face);
  GLenum fail = static_cast<GLenum>(c.fail);
  GLenum zfail = static_cast<GLenum>(c.zfail);
  GLenum zpass = static_cast<GLenum>(c.zpass);
  if (!validators_->face_type.IsValid(face)) {
    SetGLErrorInvalidEnum("glStencilOpSeparate", face, "face");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(fail)) {
    SetGLErrorInvalidEnum("glStencilOpSeparate", fail, "fail");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(zfail)) {
    SetGLErrorInvalidEnum("glStencilOpSeparate", zfail, "zfail");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(zpass)) {
    SetGLErrorInvalidEnum("glStencilOpSeparate", zpass, "zpass");
    return error::kNoError;
  }
  bool changed = false;
  if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
    changed |= state_.stencil_front_fail_op != fail ||
        state_.stencil_front_z_fail_op != zfail ||
        state_.stencil_front_z_pass_op != zpass;
  }
  if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
    changed |= state_.stencil_back_fail_op != fail ||
        state_.stencil_back_z_fail_op != zfail ||
        state_.stencil_back_z_pass_op != zpass;
  }
  if (changed) {
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
      state_.stencil_front_fail_op = fail;
      state_.stencil_front_z_fail_op = zfail;
      state_.stencil_front_z_pass_op = zpass;
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
      state_.stencil_back_fail_op = fail;
      state_.stencil_back_z_fail_op = zfail;
      state_.stencil_back_z_pass_op = zpass;
    }
    glStencilOpSeparate(face, fail, zfail, zpass);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameterf(
    uint32 immediate_data_size, const gles2::cmds::TexParameterf& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat param = static_cast<GLfloat>(c.param);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glTexParameterf", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLErrorInvalidEnum("glTexParameterf", pname, "pname");
    return error::kNoError;
  }
  DoTexParameterf(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameterfv(
    uint32 immediate_data_size, const gles2::cmds::TexParameterfv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLfloat), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLfloat* params = GetSharedMemoryAs<const GLfloat*>(
      c.params_shm_id, c.params_shm_offset, data_size);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glTexParameterfv", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLErrorInvalidEnum("glTexParameterfv", pname, "pname");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  DoTexParameterfv(target, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameterfvImmediate(
    uint32 immediate_data_size,
    const gles2::cmds::TexParameterfvImmediate& c) {
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
    SetGLErrorInvalidEnum("glTexParameterfv", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLErrorInvalidEnum("glTexParameterfv", pname, "pname");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  DoTexParameterfv(target, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameteri(
    uint32 immediate_data_size, const gles2::cmds::TexParameteri& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint param = static_cast<GLint>(c.param);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glTexParameteri", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLErrorInvalidEnum("glTexParameteri", pname, "pname");
    return error::kNoError;
  }
  DoTexParameteri(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameteriv(
    uint32 immediate_data_size, const gles2::cmds::TexParameteriv& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLint), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLint* params = GetSharedMemoryAs<const GLint*>(
      c.params_shm_id, c.params_shm_offset, data_size);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glTexParameteriv", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLErrorInvalidEnum("glTexParameteriv", pname, "pname");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  DoTexParameteriv(target, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameterivImmediate(
    uint32 immediate_data_size,
    const gles2::cmds::TexParameterivImmediate& c) {
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
    SetGLErrorInvalidEnum("glTexParameteriv", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    SetGLErrorInvalidEnum("glTexParameteriv", pname, "pname");
    return error::kNoError;
  }
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  DoTexParameteriv(target, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1f(
    uint32 immediate_data_size, const gles2::cmds::Uniform1f& c) {
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat temp[1] = { x, };
  DoUniform1fv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1fv(
    uint32 immediate_data_size, const gles2::cmds::Uniform1fv& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform1fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform1i& c) {
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  DoUniform1i(location, x);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1iv(
    uint32 immediate_data_size, const gles2::cmds::Uniform1iv& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform1ivImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform2f& c) {
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat temp[2] = { x, y, };
  DoUniform2fv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2fv(
    uint32 immediate_data_size, const gles2::cmds::Uniform2fv& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform2fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform2i& c) {
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint temp[2] = { x, y, };
  DoUniform2iv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2iv(
    uint32 immediate_data_size, const gles2::cmds::Uniform2iv& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform2ivImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform3f& c) {
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat temp[3] = { x, y, z, };
  DoUniform3fv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3fv(
    uint32 immediate_data_size, const gles2::cmds::Uniform3fv& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform3fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform3i& c) {
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  GLint temp[3] = { x, y, z, };
  DoUniform3iv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3iv(
    uint32 immediate_data_size, const gles2::cmds::Uniform3iv& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform3ivImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform4f& c) {
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat w = static_cast<GLfloat>(c.w);
  GLfloat temp[4] = { x, y, z, w, };
  DoUniform4fv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4fv(
    uint32 immediate_data_size, const gles2::cmds::Uniform4fv& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform4fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform4i& c) {
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  GLint w = static_cast<GLint>(c.w);
  GLint temp[4] = { x, y, z, w, };
  DoUniform4iv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4iv(
    uint32 immediate_data_size, const gles2::cmds::Uniform4iv& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::Uniform4ivImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
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
    uint32 immediate_data_size, const gles2::cmds::UniformMatrix2fv& c) {
  GLint location = static_cast<GLint>(c.location);
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
        GL_INVALID_VALUE, "glUniformMatrix2fv", "transpose GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (value == NULL) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix2fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix2fvImmediate(
    uint32 immediate_data_size,
    const gles2::cmds::UniformMatrix2fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
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
        GL_INVALID_VALUE, "glUniformMatrix2fv", "transpose GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (value == NULL) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix2fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix3fv(
    uint32 immediate_data_size, const gles2::cmds::UniformMatrix3fv& c) {
  GLint location = static_cast<GLint>(c.location);
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
        GL_INVALID_VALUE, "glUniformMatrix3fv", "transpose GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (value == NULL) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix3fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix3fvImmediate(
    uint32 immediate_data_size,
    const gles2::cmds::UniformMatrix3fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
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
        GL_INVALID_VALUE, "glUniformMatrix3fv", "transpose GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (value == NULL) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix3fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix4fv(
    uint32 immediate_data_size, const gles2::cmds::UniformMatrix4fv& c) {
  GLint location = static_cast<GLint>(c.location);
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
        GL_INVALID_VALUE, "glUniformMatrix4fv", "transpose GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (value == NULL) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix4fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix4fvImmediate(
    uint32 immediate_data_size,
    const gles2::cmds::UniformMatrix4fvImmediate& c) {
  GLint location = static_cast<GLint>(c.location);
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
        GL_INVALID_VALUE, "glUniformMatrix4fv", "transpose GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (value == NULL) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix4fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUseProgram(
    uint32 immediate_data_size, const gles2::cmds::UseProgram& c) {
  GLuint program = c.program;
  DoUseProgram(program);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleValidateProgram(
    uint32 immediate_data_size, const gles2::cmds::ValidateProgram& c) {
  GLuint program = c.program;
  DoValidateProgram(program);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib1f(
    uint32 immediate_data_size, const gles2::cmds::VertexAttrib1f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  DoVertexAttrib1f(indx, x);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib1fv(
    uint32 immediate_data_size, const gles2::cmds::VertexAttrib1fv& c) {
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
    uint32 immediate_data_size,
    const gles2::cmds::VertexAttrib1fvImmediate& c) {
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
    uint32 immediate_data_size, const gles2::cmds::VertexAttrib2f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  DoVertexAttrib2f(indx, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib2fv(
    uint32 immediate_data_size, const gles2::cmds::VertexAttrib2fv& c) {
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
    uint32 immediate_data_size,
    const gles2::cmds::VertexAttrib2fvImmediate& c) {
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
    uint32 immediate_data_size, const gles2::cmds::VertexAttrib3f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  DoVertexAttrib3f(indx, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib3fv(
    uint32 immediate_data_size, const gles2::cmds::VertexAttrib3fv& c) {
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
    uint32 immediate_data_size,
    const gles2::cmds::VertexAttrib3fvImmediate& c) {
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
    uint32 immediate_data_size, const gles2::cmds::VertexAttrib4f& c) {
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat w = static_cast<GLfloat>(c.w);
  DoVertexAttrib4f(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib4fv(
    uint32 immediate_data_size, const gles2::cmds::VertexAttrib4fv& c) {
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
    uint32 immediate_data_size,
    const gles2::cmds::VertexAttrib4fvImmediate& c) {
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
    uint32 immediate_data_size, const gles2::cmds::Viewport& c) {
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glViewport", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glViewport", "height < 0");
    return error::kNoError;
  }
  DoViewport(x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlitFramebufferEXT(
    uint32 immediate_data_size, const gles2::cmds::BlitFramebufferEXT& c) {
  if (ShouldDeferDraws() || ShouldDeferReads())
    return error::kDeferCommandUntilLater;
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
    SetGLErrorInvalidEnum("glBlitFramebufferEXT", filter, "filter");
    return error::kNoError;
  }
  DoBlitFramebufferEXT(
      srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleRenderbufferStorageMultisampleEXT(
    uint32 immediate_data_size,
    const gles2::cmds::RenderbufferStorageMultisampleEXT& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei samples = static_cast<GLsizei>(c.samples);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->render_buffer_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glRenderbufferStorageMultisampleEXT", target,
    "target");
    return error::kNoError;
  }
  if (samples < 0) {
    SetGLError(
        GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT", "samples < 0");
    return error::kNoError;
  }
  if (!validators_->render_buffer_format.IsValid(internalformat)) {
    SetGLErrorInvalidEnum("glRenderbufferStorageMultisampleEXT", internalformat,
    "internalformat");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(
        GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(
        GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT", "height < 0");
    return error::kNoError;
  }
  DoRenderbufferStorageMultisample(
      target, samples, internalformat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexStorage2DEXT(
    uint32 immediate_data_size, const gles2::cmds::TexStorage2DEXT& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei levels = static_cast<GLsizei>(c.levels);
  GLenum internalFormat = static_cast<GLenum>(c.internalFormat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glTexStorage2DEXT", target, "target");
    return error::kNoError;
  }
  if (levels < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage2DEXT", "levels < 0");
    return error::kNoError;
  }
  if (!validators_->texture_internal_format_storage.IsValid(internalFormat)) {
    SetGLErrorInvalidEnum("glTexStorage2DEXT", internalFormat,
    "internalFormat");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage2DEXT", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage2DEXT", "height < 0");
    return error::kNoError;
  }
  DoTexStorage2DEXT(target, levels, internalFormat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenQueriesEXT(
    uint32 immediate_data_size, const gles2::cmds::GenQueriesEXT& c) {
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
    uint32 immediate_data_size, const gles2::cmds::GenQueriesEXTImmediate& c) {
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
    uint32 immediate_data_size, const gles2::cmds::DeleteQueriesEXT& c) {
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
    uint32 immediate_data_size,
    const gles2::cmds::DeleteQueriesEXTImmediate& c) {
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

error::Error GLES2DecoderImpl::HandleInsertEventMarkerEXT(
  uint32 immediate_data_size, const gles2::cmds::InsertEventMarkerEXT& c) {
  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string str;
  if (!bucket->GetAsString(&str)) {
    return error::kInvalidArguments;
  }
  DoInsertEventMarkerEXT(0, str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePushGroupMarkerEXT(
  uint32 immediate_data_size, const gles2::cmds::PushGroupMarkerEXT& c) {
  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string str;
  if (!bucket->GetAsString(&str)) {
    return error::kInvalidArguments;
  }
  DoPushGroupMarkerEXT(0, str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePopGroupMarkerEXT(
    uint32 immediate_data_size, const gles2::cmds::PopGroupMarkerEXT& c) {
  DoPopGroupMarkerEXT();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenVertexArraysOES(
    uint32 immediate_data_size, const gles2::cmds::GenVertexArraysOES& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* arrays = GetSharedMemoryAs<GLuint*>(
      c.arrays_shm_id, c.arrays_shm_offset, data_size);
  if (arrays == NULL) {
    return error::kOutOfBounds;
  }
  if (!GenVertexArraysOESHelper(n, arrays)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenVertexArraysOESImmediate(
    uint32 immediate_data_size,
    const gles2::cmds::GenVertexArraysOESImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  GLuint* arrays = GetImmediateDataAs<GLuint*>(
      c, data_size, immediate_data_size);
  if (arrays == NULL) {
    return error::kOutOfBounds;
  }
  if (!GenVertexArraysOESHelper(n, arrays)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteVertexArraysOES(
    uint32 immediate_data_size, const gles2::cmds::DeleteVertexArraysOES& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* arrays = GetSharedMemoryAs<const GLuint*>(
      c.arrays_shm_id, c.arrays_shm_offset, data_size);
  if (arrays == NULL) {
    return error::kOutOfBounds;
  }
  DeleteVertexArraysOESHelper(n, arrays);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteVertexArraysOESImmediate(
    uint32 immediate_data_size,
    const gles2::cmds::DeleteVertexArraysOESImmediate& c) {
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32 data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* arrays = GetImmediateDataAs<const GLuint*>(
      c, data_size, immediate_data_size);
  if (arrays == NULL) {
    return error::kOutOfBounds;
  }
  DeleteVertexArraysOESHelper(n, arrays);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsVertexArrayOES(
    uint32 immediate_data_size, const gles2::cmds::IsVertexArrayOES& c) {
  GLuint array = c.array;
  typedef cmds::IsVertexArrayOES::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsVertexArrayOES(array);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindVertexArrayOES(
    uint32 immediate_data_size, const gles2::cmds::BindVertexArrayOES& c) {
  GLuint array = c.array;
  DoBindVertexArrayOES(array);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetMaxValueInBufferCHROMIUM(
    uint32 immediate_data_size,
    const gles2::cmds::GetMaxValueInBufferCHROMIUM& c) {
  GLuint buffer_id = c.buffer_id;
  GLsizei count = static_cast<GLsizei>(c.count);
  GLenum type = static_cast<GLenum>(c.type);
  GLuint offset = static_cast<GLuint>(c.offset);
  typedef cmds::GetMaxValueInBufferCHROMIUM::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetMaxValueInBufferCHROMIUM", "count < 0");
    return error::kNoError;
  }
  if (!validators_->get_max_index_type.IsValid(type)) {
    SetGLErrorInvalidEnum("glGetMaxValueInBufferCHROMIUM", type, "type");
    return error::kNoError;
  }
  *result_dst = DoGetMaxValueInBufferCHROMIUM(buffer_id, count, type, offset);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexImageIOSurface2DCHROMIUM(
    uint32 immediate_data_size,
    const gles2::cmds::TexImageIOSurface2DCHROMIUM& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLuint ioSurfaceId = static_cast<GLuint>(c.ioSurfaceId);
  GLuint plane = static_cast<GLuint>(c.plane);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glTexImageIOSurface2DCHROMIUM", target, "target");
    return error::kNoError;
  }
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexImageIOSurface2DCHROMIUM", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    SetGLError(
        GL_INVALID_VALUE, "glTexImageIOSurface2DCHROMIUM", "height < 0");
    return error::kNoError;
  }
  DoTexImageIOSurface2DCHROMIUM(target, width, height, ioSurfaceId, plane);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCopyTextureCHROMIUM(
    uint32 immediate_data_size, const gles2::cmds::CopyTextureCHROMIUM& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLenum source_id = static_cast<GLenum>(c.source_id);
  GLenum dest_id = static_cast<GLenum>(c.dest_id);
  GLint level = static_cast<GLint>(c.level);
  GLint internalformat = static_cast<GLint>(c.internalformat);
  if (!validators_->texture_internal_format.IsValid(internalformat)) {
    SetGLError(
        GL_INVALID_VALUE, "glCopyTextureCHROMIUM", "internalformat GL_INVALID_VALUE");  // NOLINT
    return error::kNoError;
  }
  DoCopyTextureCHROMIUM(target, source_id, dest_id, level, internalformat);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleProduceTextureCHROMIUM(
    uint32 immediate_data_size, const gles2::cmds::ProduceTextureCHROMIUM& c) {
  GLenum target = static_cast<GLenum>(c.target);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLbyte), 64, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLbyte* mailbox = GetSharedMemoryAs<const GLbyte*>(
      c.mailbox_shm_id, c.mailbox_shm_offset, data_size);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glProduceTextureCHROMIUM", target, "target");
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
    const gles2::cmds::ProduceTextureCHROMIUMImmediate& c) {
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
    SetGLErrorInvalidEnum("glProduceTextureCHROMIUM", target, "target");
    return error::kNoError;
  }
  if (mailbox == NULL) {
    return error::kOutOfBounds;
  }
  DoProduceTextureCHROMIUM(target, mailbox);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleConsumeTextureCHROMIUM(
    uint32 immediate_data_size, const gles2::cmds::ConsumeTextureCHROMIUM& c) {
  GLenum target = static_cast<GLenum>(c.target);
  uint32 data_size;
  if (!ComputeDataSize(1, sizeof(GLbyte), 64, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLbyte* mailbox = GetSharedMemoryAs<const GLbyte*>(
      c.mailbox_shm_id, c.mailbox_shm_offset, data_size);
  if (!validators_->texture_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glConsumeTextureCHROMIUM", target, "target");
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
    const gles2::cmds::ConsumeTextureCHROMIUMImmediate& c) {
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
    SetGLErrorInvalidEnum("glConsumeTextureCHROMIUM", target, "target");
    return error::kNoError;
  }
  if (mailbox == NULL) {
    return error::kOutOfBounds;
  }
  DoConsumeTextureCHROMIUM(target, mailbox);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindTexImage2DCHROMIUM(
    uint32 immediate_data_size, const gles2::cmds::BindTexImage2DCHROMIUM& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint imageId = static_cast<GLint>(c.imageId);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glBindTexImage2DCHROMIUM", target, "target");
    return error::kNoError;
  }
  DoBindTexImage2DCHROMIUM(target, imageId);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleReleaseTexImage2DCHROMIUM(
    uint32 immediate_data_size,
    const gles2::cmds::ReleaseTexImage2DCHROMIUM& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLint imageId = static_cast<GLint>(c.imageId);
  if (!validators_->texture_bind_target.IsValid(target)) {
    SetGLErrorInvalidEnum("glReleaseTexImage2DCHROMIUM", target, "target");
    return error::kNoError;
  }
  DoReleaseTexImage2DCHROMIUM(target, imageId);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTraceEndCHROMIUM(
    uint32 immediate_data_size, const gles2::cmds::TraceEndCHROMIUM& c) {
  DoTraceEndCHROMIUM();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDiscardFramebufferEXT(
    uint32 immediate_data_size, const gles2::cmds::DiscardFramebufferEXT& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLenum), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  const GLenum* attachments = GetSharedMemoryAs<const GLenum*>(
      c.attachments_shm_id, c.attachments_shm_offset, data_size);
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glDiscardFramebufferEXT", "count < 0");
    return error::kNoError;
  }
  if (attachments == NULL) {
    return error::kOutOfBounds;
  }
  DoDiscardFramebufferEXT(target, count, attachments);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDiscardFramebufferEXTImmediate(
    uint32 immediate_data_size,
    const gles2::cmds::DiscardFramebufferEXTImmediate& c) {
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32 data_size;
  if (!ComputeDataSize(count, sizeof(GLenum), 1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  const GLenum* attachments = GetImmediateDataAs<const GLenum*>(
      c, data_size, immediate_data_size);
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glDiscardFramebufferEXT", "count < 0");
    return error::kNoError;
  }
  if (attachments == NULL) {
    return error::kOutOfBounds;
  }
  DoDiscardFramebufferEXT(target, count, attachments);
  return error::kNoError;
}


bool GLES2DecoderImpl::SetCapabilityState(GLenum cap, bool enabled) {
  switch (cap) {
    case GL_BLEND:
      state_.enable_flags.blend = enabled;
      return true;
    case GL_CULL_FACE:
      state_.enable_flags.cull_face = enabled;
      return true;
    case GL_DEPTH_TEST:
      if (state_.enable_flags.depth_test != enabled) {
        state_.enable_flags.depth_test = enabled;
        clear_state_dirty_ = true;
      }
      return false;
    case GL_DITHER:
      state_.enable_flags.dither = enabled;
      return true;
    case GL_POLYGON_OFFSET_FILL:
      state_.enable_flags.polygon_offset_fill = enabled;
      return true;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      state_.enable_flags.sample_alpha_to_coverage = enabled;
      return true;
    case GL_SAMPLE_COVERAGE:
      state_.enable_flags.sample_coverage = enabled;
      return true;
    case GL_SCISSOR_TEST:
      if (state_.enable_flags.scissor_test != enabled) {
        state_.enable_flags.scissor_test = enabled;
        clear_state_dirty_ = true;
      }
      return false;
    case GL_STENCIL_TEST:
      if (state_.enable_flags.stencil_test != enabled) {
        state_.enable_flags.stencil_test = enabled;
        clear_state_dirty_ = true;
      }
      return false;
    default:
      NOTREACHED();
      return false;
  }
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_AUTOGEN_H_

