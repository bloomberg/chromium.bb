// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"

namespace gpu {
namespace gles2 {

error::Error GLES2DecoderPassthroughImpl::HandleActiveTexture(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ActiveTexture& c =
      *static_cast<const volatile gles2::cmds::ActiveTexture*>(cmd_data);
  GLenum texture = static_cast<GLenum>(c.texture);
  error::Error error = DoActiveTexture(texture);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleAttachShader(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::AttachShader& c =
      *static_cast<const volatile gles2::cmds::AttachShader*>(cmd_data);
  GLuint program = c.program;
  GLuint shader = c.shader;
  error::Error error = DoAttachShader(program, shader);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBindBuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindBuffer& c =
      *static_cast<const volatile gles2::cmds::BindBuffer*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint buffer = c.buffer;
  error::Error error = DoBindBuffer(target, buffer);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBindBufferBase(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindBufferBase& c =
      *static_cast<const volatile gles2::cmds::BindBufferBase*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint index = static_cast<GLuint>(c.index);
  GLuint buffer = c.buffer;
  error::Error error = DoBindBufferBase(target, index, buffer);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBindBufferRange(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindBufferRange& c =
      *static_cast<const volatile gles2::cmds::BindBufferRange*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint index = static_cast<GLuint>(c.index);
  GLuint buffer = c.buffer;
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  error::Error error = DoBindBufferRange(target, index, buffer, offset, size);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBindFramebuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindFramebuffer& c =
      *static_cast<const volatile gles2::cmds::BindFramebuffer*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint framebuffer = c.framebuffer;
  error::Error error = DoBindFramebuffer(target, framebuffer);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBindRenderbuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindRenderbuffer& c =
      *static_cast<const volatile gles2::cmds::BindRenderbuffer*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint renderbuffer = c.renderbuffer;
  error::Error error = DoBindRenderbuffer(target, renderbuffer);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBindSampler(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindSampler& c =
      *static_cast<const volatile gles2::cmds::BindSampler*>(cmd_data);
  GLuint unit = static_cast<GLuint>(c.unit);
  GLuint sampler = c.sampler;
  error::Error error = DoBindSampler(unit, sampler);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBindTexture(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindTexture& c =
      *static_cast<const volatile gles2::cmds::BindTexture*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint texture = c.texture;
  error::Error error = DoBindTexture(target, texture);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBindTransformFeedback(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindTransformFeedback& c =
      *static_cast<const volatile gles2::cmds::BindTransformFeedback*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint transformfeedback = c.transformfeedback;
  error::Error error = DoBindTransformFeedback(target, transformfeedback);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBlendColor(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BlendColor& c =
      *static_cast<const volatile gles2::cmds::BlendColor*>(cmd_data);
  GLclampf red = static_cast<GLclampf>(c.red);
  GLclampf green = static_cast<GLclampf>(c.green);
  GLclampf blue = static_cast<GLclampf>(c.blue);
  GLclampf alpha = static_cast<GLclampf>(c.alpha);
  error::Error error = DoBlendColor(red, green, blue, alpha);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBlendEquation(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BlendEquation& c =
      *static_cast<const volatile gles2::cmds::BlendEquation*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  error::Error error = DoBlendEquation(mode);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBlendEquationSeparate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BlendEquationSeparate& c =
      *static_cast<const volatile gles2::cmds::BlendEquationSeparate*>(
          cmd_data);
  GLenum modeRGB = static_cast<GLenum>(c.modeRGB);
  GLenum modeAlpha = static_cast<GLenum>(c.modeAlpha);
  error::Error error = DoBlendEquationSeparate(modeRGB, modeAlpha);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBlendFunc(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BlendFunc& c =
      *static_cast<const volatile gles2::cmds::BlendFunc*>(cmd_data);
  GLenum sfactor = static_cast<GLenum>(c.sfactor);
  GLenum dfactor = static_cast<GLenum>(c.dfactor);
  error::Error error = DoBlendFunc(sfactor, dfactor);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBlendFuncSeparate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BlendFuncSeparate& c =
      *static_cast<const volatile gles2::cmds::BlendFuncSeparate*>(cmd_data);
  GLenum srcRGB = static_cast<GLenum>(c.srcRGB);
  GLenum dstRGB = static_cast<GLenum>(c.dstRGB);
  GLenum srcAlpha = static_cast<GLenum>(c.srcAlpha);
  GLenum dstAlpha = static_cast<GLenum>(c.dstAlpha);
  error::Error error = DoBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBufferSubData(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BufferSubData& c =
      *static_cast<const volatile gles2::cmds::BufferSubData*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32_t data_size = size;
  const void* data = GetSharedMemoryAs<const void*>(
      c.data_shm_id, c.data_shm_offset, data_size);
  error::Error error = DoBufferSubData(target, offset, size, data);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCheckFramebufferStatus(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CheckFramebufferStatus& c =
      *static_cast<const volatile gles2::cmds::CheckFramebufferStatus*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  typedef cmds::CheckFramebufferStatus::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoCheckFramebufferStatus(target, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleClear(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Clear& c =
      *static_cast<const volatile gles2::cmds::Clear*>(cmd_data);
  GLbitfield mask = static_cast<GLbitfield>(c.mask);
  error::Error error = DoClear(mask);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleClearBufferfi(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ClearBufferfi& c =
      *static_cast<const volatile gles2::cmds::ClearBufferfi*>(cmd_data);
  GLenum buffer = static_cast<GLenum>(c.buffer);
  GLint drawbuffers = static_cast<GLint>(c.drawbuffers);
  GLfloat depth = static_cast<GLfloat>(c.depth);
  GLint stencil = static_cast<GLint>(c.stencil);
  error::Error error = DoClearBufferfi(buffer, drawbuffers, depth, stencil);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleClearBufferfvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ClearBufferfvImmediate& c =
      *static_cast<const volatile gles2::cmds::ClearBufferfvImmediate*>(
          cmd_data);
  GLenum buffer = static_cast<GLenum>(c.buffer);
  GLint drawbuffers = static_cast<GLint>(c.drawbuffers);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 4>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoClearBufferfv(buffer, drawbuffers, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleClearBufferivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ClearBufferivImmediate& c =
      *static_cast<const volatile gles2::cmds::ClearBufferivImmediate*>(
          cmd_data);
  GLenum buffer = static_cast<GLenum>(c.buffer);
  GLint drawbuffers = static_cast<GLint>(c.drawbuffers);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLint, 4>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* value = GetImmediateDataAs<volatile const GLint*>(
      c, data_size, immediate_data_size);
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoClearBufferiv(buffer, drawbuffers, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleClearBufferuivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ClearBufferuivImmediate& c =
      *static_cast<const volatile gles2::cmds::ClearBufferuivImmediate*>(
          cmd_data);
  GLenum buffer = static_cast<GLenum>(c.buffer);
  GLint drawbuffers = static_cast<GLint>(c.drawbuffers);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLuint, 4>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* value = GetImmediateDataAs<volatile const GLuint*>(
      c, data_size, immediate_data_size);
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoClearBufferuiv(buffer, drawbuffers, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleClearColor(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ClearColor& c =
      *static_cast<const volatile gles2::cmds::ClearColor*>(cmd_data);
  GLclampf red = static_cast<GLclampf>(c.red);
  GLclampf green = static_cast<GLclampf>(c.green);
  GLclampf blue = static_cast<GLclampf>(c.blue);
  GLclampf alpha = static_cast<GLclampf>(c.alpha);
  error::Error error = DoClearColor(red, green, blue, alpha);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleClearDepthf(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ClearDepthf& c =
      *static_cast<const volatile gles2::cmds::ClearDepthf*>(cmd_data);
  GLclampf depth = static_cast<GLclampf>(c.depth);
  error::Error error = DoClearDepthf(depth);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleClearStencil(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ClearStencil& c =
      *static_cast<const volatile gles2::cmds::ClearStencil*>(cmd_data);
  GLint s = static_cast<GLint>(c.s);
  error::Error error = DoClearStencil(s);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleColorMask(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ColorMask& c =
      *static_cast<const volatile gles2::cmds::ColorMask*>(cmd_data);
  GLboolean red = static_cast<GLboolean>(c.red);
  GLboolean green = static_cast<GLboolean>(c.green);
  GLboolean blue = static_cast<GLboolean>(c.blue);
  GLboolean alpha = static_cast<GLboolean>(c.alpha);
  error::Error error = DoColorMask(red, green, blue, alpha);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCompileShader(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CompileShader& c =
      *static_cast<const volatile gles2::cmds::CompileShader*>(cmd_data);
  GLuint shader = c.shader;
  error::Error error = DoCompileShader(shader);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCopyBufferSubData(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CopyBufferSubData& c =
      *static_cast<const volatile gles2::cmds::CopyBufferSubData*>(cmd_data);
  GLenum readtarget = static_cast<GLenum>(c.readtarget);
  GLenum writetarget = static_cast<GLenum>(c.writetarget);
  GLintptr readoffset = static_cast<GLintptr>(c.readoffset);
  GLintptr writeoffset = static_cast<GLintptr>(c.writeoffset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  error::Error error = DoCopyBufferSubData(readtarget, writetarget, readoffset,
                                           writeoffset, size);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCopyTexImage2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CopyTexImage2D& c =
      *static_cast<const volatile gles2::cmds::CopyTexImage2D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  error::Error error = DoCopyTexImage2D(target, level, internalformat, x, y,
                                        width, height, border);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCopyTexSubImage2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CopyTexSubImage2D& c =
      *static_cast<const volatile gles2::cmds::CopyTexSubImage2D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  error::Error error =
      DoCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCopyTexSubImage3D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CopyTexSubImage3D& c =
      *static_cast<const volatile gles2::cmds::CopyTexSubImage3D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint zoffset = static_cast<GLint>(c.zoffset);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  error::Error error = DoCopyTexSubImage3D(target, level, xoffset, yoffset,
                                           zoffset, x, y, width, height);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCullFace(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CullFace& c =
      *static_cast<const volatile gles2::cmds::CullFace*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  error::Error error = DoCullFace(mode);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDeleteBuffersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteBuffersImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteBuffersImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* buffers = GetImmediateDataAs<volatile const GLuint*>(
      c, data_size, immediate_data_size);
  if (buffers == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoDeleteBuffers(n, buffers);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDeleteFramebuffersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteFramebuffersImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteFramebuffersImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* framebuffers =
      GetImmediateDataAs<volatile const GLuint*>(c, data_size,
                                                 immediate_data_size);
  if (framebuffers == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoDeleteFramebuffers(n, framebuffers);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDeleteProgram(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteProgram& c =
      *static_cast<const volatile gles2::cmds::DeleteProgram*>(cmd_data);
  GLuint program = c.program;
  error::Error error = DoDeleteProgram(program);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDeleteRenderbuffersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteRenderbuffersImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteRenderbuffersImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* renderbuffers =
      GetImmediateDataAs<volatile const GLuint*>(c, data_size,
                                                 immediate_data_size);
  if (renderbuffers == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoDeleteRenderbuffers(n, renderbuffers);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDeleteSamplersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteSamplersImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteSamplersImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* samplers = GetImmediateDataAs<volatile const GLuint*>(
      c, data_size, immediate_data_size);
  if (samplers == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoDeleteSamplers(n, samplers);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDeleteSync(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteSync& c =
      *static_cast<const volatile gles2::cmds::DeleteSync*>(cmd_data);
  GLuint sync = c.sync;
  error::Error error = DoDeleteSync(sync);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDeleteShader(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteShader& c =
      *static_cast<const volatile gles2::cmds::DeleteShader*>(cmd_data);
  GLuint shader = c.shader;
  error::Error error = DoDeleteShader(shader);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDeleteTexturesImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteTexturesImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteTexturesImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* textures = GetImmediateDataAs<volatile const GLuint*>(
      c, data_size, immediate_data_size);
  if (textures == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoDeleteTextures(n, textures);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleDeleteTransformFeedbacksImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteTransformFeedbacksImmediate& c =
      *static_cast<
          const volatile gles2::cmds::DeleteTransformFeedbacksImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* ids = GetImmediateDataAs<volatile const GLuint*>(
      c, data_size, immediate_data_size);
  if (ids == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoDeleteTransformFeedbacks(n, ids);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDepthFunc(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DepthFunc& c =
      *static_cast<const volatile gles2::cmds::DepthFunc*>(cmd_data);
  GLenum func = static_cast<GLenum>(c.func);
  error::Error error = DoDepthFunc(func);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDepthMask(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DepthMask& c =
      *static_cast<const volatile gles2::cmds::DepthMask*>(cmd_data);
  GLboolean flag = static_cast<GLboolean>(c.flag);
  error::Error error = DoDepthMask(flag);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDepthRangef(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DepthRangef& c =
      *static_cast<const volatile gles2::cmds::DepthRangef*>(cmd_data);
  GLclampf zNear = static_cast<GLclampf>(c.zNear);
  GLclampf zFar = static_cast<GLclampf>(c.zFar);
  error::Error error = DoDepthRangef(zNear, zFar);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDetachShader(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DetachShader& c =
      *static_cast<const volatile gles2::cmds::DetachShader*>(cmd_data);
  GLuint program = c.program;
  GLuint shader = c.shader;
  error::Error error = DoDetachShader(program, shader);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDisable(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Disable& c =
      *static_cast<const volatile gles2::cmds::Disable*>(cmd_data);
  GLenum cap = static_cast<GLenum>(c.cap);
  error::Error error = DoDisable(cap);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDisableVertexAttribArray(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DisableVertexAttribArray& c =
      *static_cast<const volatile gles2::cmds::DisableVertexAttribArray*>(
          cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  error::Error error = DoDisableVertexAttribArray(index);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleEnable(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Enable& c =
      *static_cast<const volatile gles2::cmds::Enable*>(cmd_data);
  GLenum cap = static_cast<GLenum>(c.cap);
  error::Error error = DoEnable(cap);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleEnableVertexAttribArray(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::EnableVertexAttribArray& c =
      *static_cast<const volatile gles2::cmds::EnableVertexAttribArray*>(
          cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  error::Error error = DoEnableVertexAttribArray(index);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleFinish(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoFinish();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleFlush(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoFlush();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleFramebufferRenderbuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::FramebufferRenderbuffer& c =
      *static_cast<const volatile gles2::cmds::FramebufferRenderbuffer*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum renderbuffertarget = static_cast<GLenum>(c.renderbuffertarget);
  GLuint renderbuffer = c.renderbuffer;
  error::Error error = DoFramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleFramebufferTexture2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::FramebufferTexture2D& c =
      *static_cast<const volatile gles2::cmds::FramebufferTexture2D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum textarget = static_cast<GLenum>(c.textarget);
  GLuint texture = c.texture;
  GLint level = static_cast<GLint>(c.level);
  error::Error error =
      DoFramebufferTexture2D(target, attachment, textarget, texture, level);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleFramebufferTextureLayer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::FramebufferTextureLayer& c =
      *static_cast<const volatile gles2::cmds::FramebufferTextureLayer*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLuint texture = c.texture;
  GLint level = static_cast<GLint>(c.level);
  GLint layer = static_cast<GLint>(c.layer);
  error::Error error =
      DoFramebufferTextureLayer(target, attachment, texture, level, layer);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleFrontFace(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::FrontFace& c =
      *static_cast<const volatile gles2::cmds::FrontFace*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  error::Error error = DoFrontFace(mode);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGenBuffersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenBuffersImmediate& c =
      *static_cast<const volatile gles2::cmds::GenBuffersImmediate*>(cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* buffers =
      GetImmediateDataAs<volatile GLuint*>(c, data_size, immediate_data_size);
  if (buffers == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoGenBuffers(n, buffers);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGenerateMipmap(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenerateMipmap& c =
      *static_cast<const volatile gles2::cmds::GenerateMipmap*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  error::Error error = DoGenerateMipmap(target);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGenFramebuffersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenFramebuffersImmediate& c =
      *static_cast<const volatile gles2::cmds::GenFramebuffersImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* framebuffers =
      GetImmediateDataAs<volatile GLuint*>(c, data_size, immediate_data_size);
  if (framebuffers == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoGenFramebuffers(n, framebuffers);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGenRenderbuffersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenRenderbuffersImmediate& c =
      *static_cast<const volatile gles2::cmds::GenRenderbuffersImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* renderbuffers =
      GetImmediateDataAs<volatile GLuint*>(c, data_size, immediate_data_size);
  if (renderbuffers == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoGenRenderbuffers(n, renderbuffers);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGenSamplersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenSamplersImmediate& c =
      *static_cast<const volatile gles2::cmds::GenSamplersImmediate*>(cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* samplers =
      GetImmediateDataAs<volatile GLuint*>(c, data_size, immediate_data_size);
  if (samplers == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoGenSamplers(n, samplers);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGenTexturesImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenTexturesImmediate& c =
      *static_cast<const volatile gles2::cmds::GenTexturesImmediate*>(cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* textures =
      GetImmediateDataAs<volatile GLuint*>(c, data_size, immediate_data_size);
  if (textures == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoGenTextures(n, textures);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGenTransformFeedbacksImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenTransformFeedbacksImmediate& c =
      *static_cast<const volatile gles2::cmds::GenTransformFeedbacksImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* ids =
      GetImmediateDataAs<volatile GLuint*>(c, data_size, immediate_data_size);
  if (ids == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoGenTransformFeedbacks(n, ids);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetBooleanv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetBooleanv& c =
      *static_cast<const volatile gles2::cmds::GetBooleanv*>(cmd_data);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetBooleanv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLboolean* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error = DoGetBooleanv(pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetBufferParameteri64v(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetBufferParameteri64v& c =
      *static_cast<const volatile gles2::cmds::GetBufferParameteri64v*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetBufferParameteri64v::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLint64* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error =
      DoGetBufferParameteri64v(target, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetBufferParameteriv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetBufferParameteriv& c =
      *static_cast<const volatile gles2::cmds::GetBufferParameteriv*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetBufferParameteriv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error =
      DoGetBufferParameteriv(target, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetError(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetError& c =
      *static_cast<const volatile gles2::cmds::GetError*>(cmd_data);
  typedef cmds::GetError::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoGetError(result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetFloatv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetFloatv& c =
      *static_cast<const volatile gles2::cmds::GetFloatv*>(cmd_data);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetFloatv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLfloat* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error = DoGetFloatv(pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleGetFramebufferAttachmentParameteriv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetFramebufferAttachmentParameteriv& c =
      *static_cast<
          const volatile gles2::cmds::GetFramebufferAttachmentParameteriv*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetFramebufferAttachmentParameteriv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error = DoGetFramebufferAttachmentParameteriv(
      target, attachment, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetInteger64v(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetInteger64v& c =
      *static_cast<const volatile gles2::cmds::GetInteger64v*>(cmd_data);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetInteger64v::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLint64* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error = DoGetInteger64v(pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetIntegeri_v(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetIntegeri_v& c =
      *static_cast<const volatile gles2::cmds::GetIntegeri_v*>(cmd_data);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLuint index = static_cast<GLuint>(c.index);
  unsigned int buffer_size = 0;
  typedef cmds::GetIntegeri_v::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.data_shm_id, c.data_shm_offset, sizeof(Result), &buffer_size);
  GLint* data = result ? result->GetData() : NULL;
  if (data == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error = DoGetIntegeri_v(pname, index, bufsize, length, data);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetInteger64i_v(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetInteger64i_v& c =
      *static_cast<const volatile gles2::cmds::GetInteger64i_v*>(cmd_data);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLuint index = static_cast<GLuint>(c.index);
  unsigned int buffer_size = 0;
  typedef cmds::GetInteger64i_v::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.data_shm_id, c.data_shm_offset, sizeof(Result), &buffer_size);
  GLint64* data = result ? result->GetData() : NULL;
  if (data == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error = DoGetInteger64i_v(pname, index, bufsize, length, data);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetIntegerv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetIntegerv& c =
      *static_cast<const volatile gles2::cmds::GetIntegerv*>(cmd_data);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetIntegerv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error = DoGetIntegerv(pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetProgramiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetProgramiv& c =
      *static_cast<const volatile gles2::cmds::GetProgramiv*>(cmd_data);
  GLuint program = c.program;
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetProgramiv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error = DoGetProgramiv(program, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetRenderbufferParameteriv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetRenderbufferParameteriv& c =
      *static_cast<const volatile gles2::cmds::GetRenderbufferParameteriv*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetRenderbufferParameteriv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error =
      DoGetRenderbufferParameteriv(target, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetSamplerParameterfv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetSamplerParameterfv& c =
      *static_cast<const volatile gles2::cmds::GetSamplerParameterfv*>(
          cmd_data);
  GLuint sampler = c.sampler;
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetSamplerParameterfv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLfloat* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error =
      DoGetSamplerParameterfv(sampler, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetSamplerParameteriv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetSamplerParameteriv& c =
      *static_cast<const volatile gles2::cmds::GetSamplerParameteriv*>(
          cmd_data);
  GLuint sampler = c.sampler;
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetSamplerParameteriv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error =
      DoGetSamplerParameteriv(sampler, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetShaderiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetShaderiv& c =
      *static_cast<const volatile gles2::cmds::GetShaderiv*>(cmd_data);
  GLuint shader = c.shader;
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetShaderiv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error = DoGetShaderiv(shader, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetSynciv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetSynciv& c =
      *static_cast<const volatile gles2::cmds::GetSynciv*>(cmd_data);
  GLuint sync = static_cast<GLuint>(c.sync);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetSynciv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.values_shm_id, c.values_shm_offset, sizeof(Result), &buffer_size);
  GLint* values = result ? result->GetData() : NULL;
  if (values == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error = DoGetSynciv(sync, pname, bufsize, length, values);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetTexParameterfv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetTexParameterfv& c =
      *static_cast<const volatile gles2::cmds::GetTexParameterfv*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetTexParameterfv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLfloat* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error =
      DoGetTexParameterfv(target, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetTexParameteriv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetTexParameteriv& c =
      *static_cast<const volatile gles2::cmds::GetTexParameteriv*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetTexParameteriv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error =
      DoGetTexParameteriv(target, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetVertexAttribfv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetVertexAttribfv& c =
      *static_cast<const volatile gles2::cmds::GetVertexAttribfv*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetVertexAttribfv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLfloat* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error =
      DoGetVertexAttribfv(index, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetVertexAttribiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetVertexAttribiv& c =
      *static_cast<const volatile gles2::cmds::GetVertexAttribiv*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetVertexAttribiv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error =
      DoGetVertexAttribiv(index, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetVertexAttribIiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetVertexAttribIiv& c =
      *static_cast<const volatile gles2::cmds::GetVertexAttribIiv*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetVertexAttribIiv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error =
      DoGetVertexAttribIiv(index, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetVertexAttribIuiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetVertexAttribIuiv& c =
      *static_cast<const volatile gles2::cmds::GetVertexAttribIuiv*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  unsigned int buffer_size = 0;
  typedef cmds::GetVertexAttribIuiv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      c.params_shm_id, c.params_shm_offset, sizeof(Result), &buffer_size);
  GLuint* params = result ? result->GetData() : NULL;
  if (params == NULL) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei written_values = 0;
  GLsizei* length = &written_values;
  error::Error error =
      DoGetVertexAttribIuiv(index, pname, bufsize, length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (written_values > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(written_values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleHint(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Hint& c =
      *static_cast<const volatile gles2::cmds::Hint*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum mode = static_cast<GLenum>(c.mode);
  error::Error error = DoHint(target, mode);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleInvalidateFramebufferImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::InvalidateFramebufferImmediate& c =
      *static_cast<const volatile gles2::cmds::InvalidateFramebufferImmediate*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLenum, 1>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLenum* attachments =
      GetImmediateDataAs<volatile const GLenum*>(c, data_size,
                                                 immediate_data_size);
  if (attachments == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoInvalidateFramebuffer(target, count, attachments);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleInvalidateSubFramebufferImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::InvalidateSubFramebufferImmediate& c =
      *static_cast<
          const volatile gles2::cmds::InvalidateSubFramebufferImmediate*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLenum, 1>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLenum* attachments =
      GetImmediateDataAs<volatile const GLenum*>(c, data_size,
                                                 immediate_data_size);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (attachments == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoInvalidateSubFramebuffer(target, count, attachments, x,
                                                  y, width, height);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleIsBuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsBuffer& c =
      *static_cast<const volatile gles2::cmds::IsBuffer*>(cmd_data);
  GLuint buffer = c.buffer;
  typedef cmds::IsBuffer::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoIsBuffer(buffer, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleIsEnabled(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsEnabled& c =
      *static_cast<const volatile gles2::cmds::IsEnabled*>(cmd_data);
  GLenum cap = static_cast<GLenum>(c.cap);
  typedef cmds::IsEnabled::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoIsEnabled(cap, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleIsFramebuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsFramebuffer& c =
      *static_cast<const volatile gles2::cmds::IsFramebuffer*>(cmd_data);
  GLuint framebuffer = c.framebuffer;
  typedef cmds::IsFramebuffer::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoIsFramebuffer(framebuffer, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleIsProgram(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsProgram& c =
      *static_cast<const volatile gles2::cmds::IsProgram*>(cmd_data);
  GLuint program = c.program;
  typedef cmds::IsProgram::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoIsProgram(program, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleIsRenderbuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsRenderbuffer& c =
      *static_cast<const volatile gles2::cmds::IsRenderbuffer*>(cmd_data);
  GLuint renderbuffer = c.renderbuffer;
  typedef cmds::IsRenderbuffer::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoIsRenderbuffer(renderbuffer, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleIsSampler(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsSampler& c =
      *static_cast<const volatile gles2::cmds::IsSampler*>(cmd_data);
  GLuint sampler = c.sampler;
  typedef cmds::IsSampler::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoIsSampler(sampler, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleIsShader(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsShader& c =
      *static_cast<const volatile gles2::cmds::IsShader*>(cmd_data);
  GLuint shader = c.shader;
  typedef cmds::IsShader::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoIsShader(shader, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleIsSync(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsSync& c =
      *static_cast<const volatile gles2::cmds::IsSync*>(cmd_data);
  GLuint sync = c.sync;
  typedef cmds::IsSync::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoIsSync(sync, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleIsTexture(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsTexture& c =
      *static_cast<const volatile gles2::cmds::IsTexture*>(cmd_data);
  GLuint texture = c.texture;
  typedef cmds::IsTexture::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoIsTexture(texture, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleIsTransformFeedback(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsTransformFeedback& c =
      *static_cast<const volatile gles2::cmds::IsTransformFeedback*>(cmd_data);
  GLuint transformfeedback = c.transformfeedback;
  typedef cmds::IsTransformFeedback::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoIsTransformFeedback(transformfeedback, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleLineWidth(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::LineWidth& c =
      *static_cast<const volatile gles2::cmds::LineWidth*>(cmd_data);
  GLfloat width = static_cast<GLfloat>(c.width);
  error::Error error = DoLineWidth(width);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleLinkProgram(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::LinkProgram& c =
      *static_cast<const volatile gles2::cmds::LinkProgram*>(cmd_data);
  GLuint program = c.program;
  error::Error error = DoLinkProgram(program);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandlePauseTransformFeedback(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoPauseTransformFeedback();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandlePolygonOffset(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::PolygonOffset& c =
      *static_cast<const volatile gles2::cmds::PolygonOffset*>(cmd_data);
  GLfloat factor = static_cast<GLfloat>(c.factor);
  GLfloat units = static_cast<GLfloat>(c.units);
  error::Error error = DoPolygonOffset(factor, units);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleReadBuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ReadBuffer& c =
      *static_cast<const volatile gles2::cmds::ReadBuffer*>(cmd_data);
  GLenum src = static_cast<GLenum>(c.src);
  error::Error error = DoReadBuffer(src);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleReleaseShaderCompiler(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoReleaseShaderCompiler();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleRenderbufferStorage(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::RenderbufferStorage& c =
      *static_cast<const volatile gles2::cmds::RenderbufferStorage*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  error::Error error =
      DoRenderbufferStorage(target, internalformat, width, height);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleResumeTransformFeedback(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoResumeTransformFeedback();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleSampleCoverage(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SampleCoverage& c =
      *static_cast<const volatile gles2::cmds::SampleCoverage*>(cmd_data);
  GLclampf value = static_cast<GLclampf>(c.value);
  GLboolean invert = static_cast<GLboolean>(c.invert);
  error::Error error = DoSampleCoverage(value, invert);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleSamplerParameterf(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SamplerParameterf& c =
      *static_cast<const volatile gles2::cmds::SamplerParameterf*>(cmd_data);
  GLuint sampler = c.sampler;
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat param = static_cast<GLfloat>(c.param);
  error::Error error = DoSamplerParameterf(sampler, pname, param);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleSamplerParameterfvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SamplerParameterfvImmediate& c =
      *static_cast<const volatile gles2::cmds::SamplerParameterfvImmediate*>(
          cmd_data);
  GLuint sampler = c.sampler;
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 1>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* params = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoSamplerParameterfv(sampler, pname, params);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleSamplerParameteri(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SamplerParameteri& c =
      *static_cast<const volatile gles2::cmds::SamplerParameteri*>(cmd_data);
  GLuint sampler = c.sampler;
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint param = static_cast<GLint>(c.param);
  error::Error error = DoSamplerParameteri(sampler, pname, param);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleSamplerParameterivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SamplerParameterivImmediate& c =
      *static_cast<const volatile gles2::cmds::SamplerParameterivImmediate*>(
          cmd_data);
  GLuint sampler = c.sampler;
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLint, 1>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* params = GetImmediateDataAs<volatile const GLint*>(
      c, data_size, immediate_data_size);
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoSamplerParameteriv(sampler, pname, params);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleScissor(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Scissor& c =
      *static_cast<const volatile gles2::cmds::Scissor*>(cmd_data);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  error::Error error = DoScissor(x, y, width, height);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleShaderSourceBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ShaderSourceBucket& c =
      *static_cast<const volatile gles2::cmds::ShaderSourceBucket*>(cmd_data);
  GLuint shader = static_cast<GLuint>(c.shader);

  Bucket* bucket = GetBucket(c.str_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  std::vector<char*> strs;
  std::vector<GLint> len;
  if (!bucket->GetAsStrings(&count, &strs, &len)) {
    return error::kInvalidArguments;
  }
  const char** str =
      strs.size() > 0 ? const_cast<const char**>(&strs[0]) : NULL;
  const GLint* length =
      len.size() > 0 ? const_cast<const GLint*>(&len[0]) : NULL;
  (void)length;
  error::Error error = DoShaderSource(shader, count, str, length);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleStencilFunc(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::StencilFunc& c =
      *static_cast<const volatile gles2::cmds::StencilFunc*>(cmd_data);
  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  error::Error error = DoStencilFunc(func, ref, mask);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleStencilFuncSeparate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::StencilFuncSeparate& c =
      *static_cast<const volatile gles2::cmds::StencilFuncSeparate*>(cmd_data);
  GLenum face = static_cast<GLenum>(c.face);
  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  error::Error error = DoStencilFuncSeparate(face, func, ref, mask);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleStencilMask(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::StencilMask& c =
      *static_cast<const volatile gles2::cmds::StencilMask*>(cmd_data);
  GLuint mask = static_cast<GLuint>(c.mask);
  error::Error error = DoStencilMask(mask);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleStencilMaskSeparate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::StencilMaskSeparate& c =
      *static_cast<const volatile gles2::cmds::StencilMaskSeparate*>(cmd_data);
  GLenum face = static_cast<GLenum>(c.face);
  GLuint mask = static_cast<GLuint>(c.mask);
  error::Error error = DoStencilMaskSeparate(face, mask);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleStencilOp(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::StencilOp& c =
      *static_cast<const volatile gles2::cmds::StencilOp*>(cmd_data);
  GLenum fail = static_cast<GLenum>(c.fail);
  GLenum zfail = static_cast<GLenum>(c.zfail);
  GLenum zpass = static_cast<GLenum>(c.zpass);
  error::Error error = DoStencilOp(fail, zfail, zpass);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleStencilOpSeparate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::StencilOpSeparate& c =
      *static_cast<const volatile gles2::cmds::StencilOpSeparate*>(cmd_data);
  GLenum face = static_cast<GLenum>(c.face);
  GLenum fail = static_cast<GLenum>(c.fail);
  GLenum zfail = static_cast<GLenum>(c.zfail);
  GLenum zpass = static_cast<GLenum>(c.zpass);
  error::Error error = DoStencilOpSeparate(face, fail, zfail, zpass);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleTexParameterf(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexParameterf& c =
      *static_cast<const volatile gles2::cmds::TexParameterf*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat param = static_cast<GLfloat>(c.param);
  error::Error error = DoTexParameterf(target, pname, param);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleTexParameterfvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexParameterfvImmediate& c =
      *static_cast<const volatile gles2::cmds::TexParameterfvImmediate*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 1>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* params = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoTexParameterfv(target, pname, params);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleTexParameteri(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexParameteri& c =
      *static_cast<const volatile gles2::cmds::TexParameteri*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint param = static_cast<GLint>(c.param);
  error::Error error = DoTexParameteri(target, pname, param);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleTexParameterivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexParameterivImmediate& c =
      *static_cast<const volatile gles2::cmds::TexParameterivImmediate*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLint, 1>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* params = GetImmediateDataAs<volatile const GLint*>(
      c, data_size, immediate_data_size);
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoTexParameteriv(target, pname, params);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleTexStorage3D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexStorage3D& c =
      *static_cast<const volatile gles2::cmds::TexStorage3D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei levels = static_cast<GLsizei>(c.levels);
  GLenum internalFormat = static_cast<GLenum>(c.internalFormat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  error::Error error =
      DoTexStorage3D(target, levels, internalFormat, width, height, depth);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleTransformFeedbackVaryingsBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TransformFeedbackVaryingsBucket& c = *static_cast<
      const volatile gles2::cmds::TransformFeedbackVaryingsBucket*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);

  Bucket* bucket = GetBucket(c.varyings_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  std::vector<char*> strs;
  std::vector<GLint> len;
  if (!bucket->GetAsStrings(&count, &strs, &len)) {
    return error::kInvalidArguments;
  }
  const char** varyings =
      strs.size() > 0 ? const_cast<const char**>(&strs[0]) : NULL;
  const GLint* length =
      len.size() > 0 ? const_cast<const GLint*>(&len[0]) : NULL;
  (void)length;
  GLenum buffermode = static_cast<GLenum>(c.buffermode);
  error::Error error =
      DoTransformFeedbackVaryings(program, count, varyings, buffermode);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform1f(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform1f& c =
      *static_cast<const volatile gles2::cmds::Uniform1f*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  error::Error error = DoUniform1f(location, x);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform1fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform1fvImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform1fvImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 1>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* v = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniform1fv(location, count, v);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform1i(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform1i& c =
      *static_cast<const volatile gles2::cmds::Uniform1i*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  error::Error error = DoUniform1i(location, x);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform1ivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform1ivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform1ivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLint, 1>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* v = GetImmediateDataAs<volatile const GLint*>(
      c, data_size, immediate_data_size);
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniform1iv(location, count, v);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform1ui(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform1ui& c =
      *static_cast<const volatile gles2::cmds::Uniform1ui*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLuint x = static_cast<GLuint>(c.x);
  error::Error error = DoUniform1ui(location, x);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform1uivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform1uivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform1uivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLuint, 1>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* v = GetImmediateDataAs<volatile const GLuint*>(
      c, data_size, immediate_data_size);
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniform1uiv(location, count, v);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform2f(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform2f& c =
      *static_cast<const volatile gles2::cmds::Uniform2f*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  error::Error error = DoUniform2f(location, x, y);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform2fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform2fvImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform2fvImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 2>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* v = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniform2fv(location, count, v);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform2i(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform2i& c =
      *static_cast<const volatile gles2::cmds::Uniform2i*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  error::Error error = DoUniform2i(location, x, y);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform2ivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform2ivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform2ivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLint, 2>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* v = GetImmediateDataAs<volatile const GLint*>(
      c, data_size, immediate_data_size);
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniform2iv(location, count, v);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform2ui(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform2ui& c =
      *static_cast<const volatile gles2::cmds::Uniform2ui*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLuint x = static_cast<GLuint>(c.x);
  GLuint y = static_cast<GLuint>(c.y);
  error::Error error = DoUniform2ui(location, x, y);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform2uivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform2uivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform2uivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLuint, 2>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* v = GetImmediateDataAs<volatile const GLuint*>(
      c, data_size, immediate_data_size);
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniform2uiv(location, count, v);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform3f(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform3f& c =
      *static_cast<const volatile gles2::cmds::Uniform3f*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  error::Error error = DoUniform3f(location, x, y, z);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform3fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform3fvImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform3fvImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 3>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* v = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniform3fv(location, count, v);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform3i(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform3i& c =
      *static_cast<const volatile gles2::cmds::Uniform3i*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  error::Error error = DoUniform3i(location, x, y, z);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform3ivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform3ivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform3ivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLint, 3>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* v = GetImmediateDataAs<volatile const GLint*>(
      c, data_size, immediate_data_size);
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniform3iv(location, count, v);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform3ui(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform3ui& c =
      *static_cast<const volatile gles2::cmds::Uniform3ui*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLuint x = static_cast<GLuint>(c.x);
  GLuint y = static_cast<GLuint>(c.y);
  GLuint z = static_cast<GLuint>(c.z);
  error::Error error = DoUniform3ui(location, x, y, z);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform3uivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform3uivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform3uivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLuint, 3>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* v = GetImmediateDataAs<volatile const GLuint*>(
      c, data_size, immediate_data_size);
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniform3uiv(location, count, v);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform4f(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform4f& c =
      *static_cast<const volatile gles2::cmds::Uniform4f*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat w = static_cast<GLfloat>(c.w);
  error::Error error = DoUniform4f(location, x, y, z, w);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform4fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform4fvImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform4fvImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 4>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* v = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniform4fv(location, count, v);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform4i(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform4i& c =
      *static_cast<const volatile gles2::cmds::Uniform4i*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  GLint w = static_cast<GLint>(c.w);
  error::Error error = DoUniform4i(location, x, y, z, w);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform4ivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform4ivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform4ivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLint, 4>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* v = GetImmediateDataAs<volatile const GLint*>(
      c, data_size, immediate_data_size);
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniform4iv(location, count, v);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform4ui(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform4ui& c =
      *static_cast<const volatile gles2::cmds::Uniform4ui*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLuint x = static_cast<GLuint>(c.x);
  GLuint y = static_cast<GLuint>(c.y);
  GLuint z = static_cast<GLuint>(c.z);
  GLuint w = static_cast<GLuint>(c.w);
  error::Error error = DoUniform4ui(location, x, y, z, w);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniform4uivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform4uivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform4uivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLuint, 4>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* v = GetImmediateDataAs<volatile const GLuint*>(
      c, data_size, immediate_data_size);
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniform4uiv(location, count, v);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniformMatrix2fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UniformMatrix2fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix2fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t data_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 4>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniformMatrix2fv(location, count, transpose, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniformMatrix2x3fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UniformMatrix2x3fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix2x3fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t data_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 6>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniformMatrix2x3fv(location, count, transpose, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniformMatrix2x4fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UniformMatrix2x4fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix2x4fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t data_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 8>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniformMatrix2x4fv(location, count, transpose, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniformMatrix3fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UniformMatrix3fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix3fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t data_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 9>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniformMatrix3fv(location, count, transpose, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniformMatrix3x2fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UniformMatrix3x2fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix3x2fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t data_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 6>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniformMatrix3x2fv(location, count, transpose, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniformMatrix3x4fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UniformMatrix3x4fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix3x4fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t data_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 12>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniformMatrix3x4fv(location, count, transpose, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniformMatrix4fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UniformMatrix4fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix4fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t data_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 16>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniformMatrix4fv(location, count, transpose, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniformMatrix4x2fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UniformMatrix4x2fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix4x2fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t data_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 8>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniformMatrix4x2fv(location, count, transpose, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUniformMatrix4x3fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UniformMatrix4x3fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix4x3fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t data_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 12>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniformMatrix4x3fv(location, count, transpose, value);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleUseProgram(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UseProgram& c =
      *static_cast<const volatile gles2::cmds::UseProgram*>(cmd_data);
  GLuint program = c.program;
  error::Error error = DoUseProgram(program);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleValidateProgram(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ValidateProgram& c =
      *static_cast<const volatile gles2::cmds::ValidateProgram*>(cmd_data);
  GLuint program = c.program;
  error::Error error = DoValidateProgram(program);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttrib1f(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib1f& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib1f*>(cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  error::Error error = DoVertexAttrib1f(indx, x);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttrib1fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib1fvImmediate& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib1fvImmediate*>(
          cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 1>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* values = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (values == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoVertexAttrib1fv(indx, values);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttrib2f(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib2f& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib2f*>(cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  error::Error error = DoVertexAttrib2f(indx, x, y);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttrib2fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib2fvImmediate& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib2fvImmediate*>(
          cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 2>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* values = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (values == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoVertexAttrib2fv(indx, values);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttrib3f(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib3f& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib3f*>(cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  error::Error error = DoVertexAttrib3f(indx, x, y, z);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttrib3fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib3fvImmediate& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib3fvImmediate*>(
          cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 3>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* values = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (values == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoVertexAttrib3fv(indx, values);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttrib4f(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib4f& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib4f*>(cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat w = static_cast<GLfloat>(c.w);
  error::Error error = DoVertexAttrib4f(indx, x, y, z, w);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttrib4fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib4fvImmediate& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib4fvImmediate*>(
          cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 4>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* values = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (values == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoVertexAttrib4fv(indx, values);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttribI4i(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttribI4i& c =
      *static_cast<const volatile gles2::cmds::VertexAttribI4i*>(cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  GLint w = static_cast<GLint>(c.w);
  error::Error error = DoVertexAttribI4i(indx, x, y, z, w);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttribI4ivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttribI4ivImmediate& c =
      *static_cast<const volatile gles2::cmds::VertexAttribI4ivImmediate*>(
          cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLint, 4>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* values = GetImmediateDataAs<volatile const GLint*>(
      c, data_size, immediate_data_size);
  if (values == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoVertexAttribI4iv(indx, values);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttribI4ui(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttribI4ui& c =
      *static_cast<const volatile gles2::cmds::VertexAttribI4ui*>(cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  GLuint x = static_cast<GLuint>(c.x);
  GLuint y = static_cast<GLuint>(c.y);
  GLuint z = static_cast<GLuint>(c.z);
  GLuint w = static_cast<GLuint>(c.w);
  error::Error error = DoVertexAttribI4ui(indx, x, y, z, w);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttribI4uivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttribI4uivImmediate& c =
      *static_cast<const volatile gles2::cmds::VertexAttribI4uivImmediate*>(
          cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLuint, 4>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* values = GetImmediateDataAs<volatile const GLuint*>(
      c, data_size, immediate_data_size);
  if (values == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoVertexAttribI4uiv(indx, values);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleViewport(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Viewport& c =
      *static_cast<const volatile gles2::cmds::Viewport*>(cmd_data);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  error::Error error = DoViewport(x, y, width, height);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBlitFramebufferCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BlitFramebufferCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::BlitFramebufferCHROMIUM*>(
          cmd_data);
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
  error::Error error = DoBlitFramebufferCHROMIUM(
      srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleRenderbufferStorageMultisampleCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::RenderbufferStorageMultisampleCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::RenderbufferStorageMultisampleCHROMIUM*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei samples = static_cast<GLsizei>(c.samples);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  error::Error error = DoRenderbufferStorageMultisampleCHROMIUM(
      target, samples, internalformat, width, height);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleRenderbufferStorageMultisampleEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::RenderbufferStorageMultisampleEXT& c =
      *static_cast<
          const volatile gles2::cmds::RenderbufferStorageMultisampleEXT*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei samples = static_cast<GLsizei>(c.samples);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  error::Error error = DoRenderbufferStorageMultisampleEXT(
      target, samples, internalformat, width, height);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleFramebufferTexture2DMultisampleEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::FramebufferTexture2DMultisampleEXT& c =
      *static_cast<
          const volatile gles2::cmds::FramebufferTexture2DMultisampleEXT*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum textarget = static_cast<GLenum>(c.textarget);
  GLuint texture = c.texture;
  GLint level = static_cast<GLint>(c.level);
  GLsizei samples = static_cast<GLsizei>(c.samples);
  error::Error error = DoFramebufferTexture2DMultisampleEXT(
      target, attachment, textarget, texture, level, samples);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleTexStorage2DEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexStorage2DEXT& c =
      *static_cast<const volatile gles2::cmds::TexStorage2DEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei levels = static_cast<GLsizei>(c.levels);
  GLenum internalFormat = static_cast<GLenum>(c.internalFormat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  error::Error error =
      DoTexStorage2DEXT(target, levels, internalFormat, width, height);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGenQueriesEXTImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenQueriesEXTImmediate& c =
      *static_cast<const volatile gles2::cmds::GenQueriesEXTImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* queries =
      GetImmediateDataAs<volatile GLuint*>(c, data_size, immediate_data_size);
  if (queries == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoGenQueriesEXT(n, queries);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDeleteQueriesEXTImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteQueriesEXTImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteQueriesEXTImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* queries = GetImmediateDataAs<volatile const GLuint*>(
      c, data_size, immediate_data_size);
  if (queries == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoDeleteQueriesEXT(n, queries);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBeginTransformFeedback(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BeginTransformFeedback& c =
      *static_cast<const volatile gles2::cmds::BeginTransformFeedback*>(
          cmd_data);
  GLenum primitivemode = static_cast<GLenum>(c.primitivemode);
  error::Error error = DoBeginTransformFeedback(primitivemode);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleEndTransformFeedback(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoEndTransformFeedback();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandlePopGroupMarkerEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoPopGroupMarkerEXT();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGenVertexArraysOESImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenVertexArraysOESImmediate& c =
      *static_cast<const volatile gles2::cmds::GenVertexArraysOESImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* arrays =
      GetImmediateDataAs<volatile GLuint*>(c, data_size, immediate_data_size);
  if (arrays == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoGenVertexArraysOES(n, arrays);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDeleteVertexArraysOESImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteVertexArraysOESImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteVertexArraysOESImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* arrays = GetImmediateDataAs<volatile const GLuint*>(
      c, data_size, immediate_data_size);
  if (arrays == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoDeleteVertexArraysOES(n, arrays);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleIsVertexArrayOES(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsVertexArrayOES& c =
      *static_cast<const volatile gles2::cmds::IsVertexArrayOES*>(cmd_data);
  GLuint array = c.array;
  typedef cmds::IsVertexArrayOES::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoIsVertexArrayOES(array, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBindVertexArrayOES(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindVertexArrayOES& c =
      *static_cast<const volatile gles2::cmds::BindVertexArrayOES*>(cmd_data);
  GLuint array = c.array;
  error::Error error = DoBindVertexArrayOES(array);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleSwapBuffers(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoSwapBuffers();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetMaxValueInBufferCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetMaxValueInBufferCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GetMaxValueInBufferCHROMIUM*>(
          cmd_data);
  GLuint buffer_id = c.buffer_id;
  GLsizei count = static_cast<GLsizei>(c.count);
  GLenum type = static_cast<GLenum>(c.type);
  GLuint offset = static_cast<GLuint>(c.offset);
  typedef cmds::GetMaxValueInBufferCHROMIUM::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error =
      DoGetMaxValueInBufferCHROMIUM(buffer_id, count, type, offset, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleFlushMappedBufferRange(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::FlushMappedBufferRange& c =
      *static_cast<const volatile gles2::cmds::FlushMappedBufferRange*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  error::Error error = DoFlushMappedBufferRange(target, offset, size);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCopyTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CopyTextureCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CopyTextureCHROMIUM*>(cmd_data);
  GLuint source_id = static_cast<GLuint>(c.source_id);
  GLint source_level = static_cast<GLint>(c.source_level);
  GLenum dest_target = static_cast<GLenum>(c.dest_target);
  GLuint dest_id = static_cast<GLuint>(c.dest_id);
  GLint dest_level = static_cast<GLint>(c.dest_level);
  GLint internalformat = static_cast<GLint>(c.internalformat);
  GLenum dest_type = static_cast<GLenum>(c.dest_type);
  GLboolean unpack_flip_y = static_cast<GLboolean>(c.unpack_flip_y);
  GLboolean unpack_premultiply_alpha =
      static_cast<GLboolean>(c.unpack_premultiply_alpha);
  GLboolean unpack_unmultiply_alpha =
      static_cast<GLboolean>(c.unpack_unmultiply_alpha);
  error::Error error = DoCopyTextureCHROMIUM(
      source_id, source_level, dest_target, dest_id, dest_level, internalformat,
      dest_type, unpack_flip_y, unpack_premultiply_alpha,
      unpack_unmultiply_alpha);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCopySubTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CopySubTextureCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CopySubTextureCHROMIUM*>(
          cmd_data);
  GLuint source_id = static_cast<GLuint>(c.source_id);
  GLint source_level = static_cast<GLint>(c.source_level);
  GLenum dest_target = static_cast<GLenum>(c.dest_target);
  GLuint dest_id = static_cast<GLuint>(c.dest_id);
  GLint dest_level = static_cast<GLint>(c.dest_level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLboolean unpack_flip_y = static_cast<GLboolean>(c.unpack_flip_y);
  GLboolean unpack_premultiply_alpha =
      static_cast<GLboolean>(c.unpack_premultiply_alpha);
  GLboolean unpack_unmultiply_alpha =
      static_cast<GLboolean>(c.unpack_unmultiply_alpha);
  error::Error error = DoCopySubTextureCHROMIUM(
      source_id, source_level, dest_target, dest_id, dest_level, xoffset,
      yoffset, x, y, width, height, unpack_flip_y, unpack_premultiply_alpha,
      unpack_unmultiply_alpha);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCompressedCopyTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CompressedCopyTextureCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CompressedCopyTextureCHROMIUM*>(
          cmd_data);
  GLuint source_id = static_cast<GLuint>(c.source_id);
  GLuint dest_id = static_cast<GLuint>(c.dest_id);
  error::Error error = DoCompressedCopyTextureCHROMIUM(source_id, dest_id);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleProduceTextureCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ProduceTextureCHROMIUMImmediate& c = *static_cast<
      const volatile gles2::cmds::ProduceTextureCHROMIUMImmediate*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLbyte, 16>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLbyte* mailbox = GetImmediateDataAs<volatile const GLbyte*>(
      c, data_size, immediate_data_size);
  if (mailbox == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoProduceTextureCHROMIUM(target, mailbox);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleProduceTextureDirectCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ProduceTextureDirectCHROMIUMImmediate& c =
      *static_cast<
          const volatile gles2::cmds::ProduceTextureDirectCHROMIUMImmediate*>(
          cmd_data);
  GLuint texture = c.texture;
  GLenum target = static_cast<GLenum>(c.target);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLbyte, 16>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLbyte* mailbox = GetImmediateDataAs<volatile const GLbyte*>(
      c, data_size, immediate_data_size);
  if (mailbox == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoProduceTextureDirectCHROMIUM(texture, target, mailbox);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleConsumeTextureCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ConsumeTextureCHROMIUMImmediate& c = *static_cast<
      const volatile gles2::cmds::ConsumeTextureCHROMIUMImmediate*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLbyte, 16>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLbyte* mailbox = GetImmediateDataAs<volatile const GLbyte*>(
      c, data_size, immediate_data_size);
  if (mailbox == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoConsumeTextureCHROMIUM(target, mailbox);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleCreateAndConsumeTextureINTERNALImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CreateAndConsumeTextureINTERNALImmediate& c =
      *static_cast<const volatile gles2::cmds::
                       CreateAndConsumeTextureINTERNALImmediate*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint texture = static_cast<GLuint>(c.texture);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLbyte, 16>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLbyte* mailbox = GetImmediateDataAs<volatile const GLbyte*>(
      c, data_size, immediate_data_size);
  if (mailbox == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error =
      DoCreateAndConsumeTextureINTERNAL(target, texture, mailbox);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBindTexImage2DCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindTexImage2DCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::BindTexImage2DCHROMIUM*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint imageId = static_cast<GLint>(c.imageId);
  error::Error error = DoBindTexImage2DCHROMIUM(target, imageId);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleBindTexImage2DWithInternalformatCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindTexImage2DWithInternalformatCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::
                       BindTexImage2DWithInternalformatCHROMIUM*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLint imageId = static_cast<GLint>(c.imageId);
  error::Error error = DoBindTexImage2DWithInternalformatCHROMIUM(
      target, internalformat, imageId);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleReleaseTexImage2DCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ReleaseTexImage2DCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::ReleaseTexImage2DCHROMIUM*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint imageId = static_cast<GLint>(c.imageId);
  error::Error error = DoReleaseTexImage2DCHROMIUM(target, imageId);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleTraceEndCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoTraceEndCHROMIUM();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDiscardFramebufferEXTImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DiscardFramebufferEXTImmediate& c =
      *static_cast<const volatile gles2::cmds::DiscardFramebufferEXTImmediate*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLenum, 1>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLenum* attachments =
      GetImmediateDataAs<volatile const GLenum*>(c, data_size,
                                                 immediate_data_size);
  if (attachments == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoDiscardFramebufferEXT(target, count, attachments);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleLoseContextCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::LoseContextCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::LoseContextCHROMIUM*>(cmd_data);
  GLenum current = static_cast<GLenum>(c.current);
  GLenum other = static_cast<GLenum>(c.other);
  error::Error error = DoLoseContextCHROMIUM(current, other);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleDrawBuffersEXTImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawBuffersEXTImmediate& c =
      *static_cast<const volatile gles2::cmds::DrawBuffersEXTImmediate*>(
          cmd_data);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLenum, 1>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLenum* bufs = GetImmediateDataAs<volatile const GLenum*>(
      c, data_size, immediate_data_size);
  if (bufs == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoDrawBuffersEXT(count, bufs);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleScheduleCALayerInUseQueryCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ScheduleCALayerInUseQueryCHROMIUMImmediate& c =
      *static_cast<const volatile gles2::cmds::
                       ScheduleCALayerInUseQueryCHROMIUMImmediate*>(cmd_data);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLuint, 1>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* textures = GetImmediateDataAs<volatile const GLuint*>(
      c, data_size, immediate_data_size);
  if (textures == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoScheduleCALayerInUseQueryCHROMIUM(count, textures);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCommitOverlayPlanesCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoCommitOverlayPlanesCHROMIUM();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleSwapInterval(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SwapInterval& c =
      *static_cast<const volatile gles2::cmds::SwapInterval*>(cmd_data);
  GLint interval = static_cast<GLint>(c.interval);
  error::Error error = DoSwapInterval(interval);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleFlushDriverCachesCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoFlushDriverCachesCHROMIUM();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleMatrixLoadfCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MatrixLoadfCHROMIUMImmediate& c =
      *static_cast<const volatile gles2::cmds::MatrixLoadfCHROMIUMImmediate*>(
          cmd_data);
  GLenum matrixMode = static_cast<GLenum>(c.matrixMode);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 16>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* m = GetImmediateDataAs<volatile const GLfloat*>(
      c, data_size, immediate_data_size);
  if (m == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoMatrixLoadfCHROMIUM(matrixMode, m);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleMatrixLoadIdentityCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MatrixLoadIdentityCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::MatrixLoadIdentityCHROMIUM*>(
          cmd_data);
  GLenum matrixMode = static_cast<GLenum>(c.matrixMode);
  error::Error error = DoMatrixLoadIdentityCHROMIUM(matrixMode);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleIsPathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsPathCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::IsPathCHROMIUM*>(cmd_data);
  GLuint path = c.path;
  typedef cmds::IsPathCHROMIUM::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  error::Error error = DoIsPathCHROMIUM(path, result);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandlePathStencilFuncCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::PathStencilFuncCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::PathStencilFuncCHROMIUM*>(
          cmd_data);
  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  error::Error error = DoPathStencilFuncCHROMIUM(func, ref, mask);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleCoverageModulationCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CoverageModulationCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CoverageModulationCHROMIUM*>(
          cmd_data);
  GLenum components = static_cast<GLenum>(c.components);
  error::Error error = DoCoverageModulationCHROMIUM(components);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBlendBarrierKHR(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoBlendBarrierKHR();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleApplyScreenSpaceAntialiasingCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoApplyScreenSpaceAntialiasingCHROMIUM();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::
    HandleUniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate(
        uint32_t immediate_data_size,
        const volatile void* cmd_data) {
  const volatile gles2::cmds::
      UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate& c = *static_cast<
          const volatile gles2::cmds::
              UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t data_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 16>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* transform =
      GetImmediateDataAs<volatile const GLfloat*>(c, data_size,
                                                  immediate_data_size);
  if (transform == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoUniformMatrix4fvStreamTextureMatrixCHROMIUM(
      location, transpose, transform);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleOverlayPromotionHintCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::OverlayPromotionHintCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::OverlayPromotionHintCHROMIUM*>(
          cmd_data);
  GLuint texture = c.texture;
  GLboolean promotion_hint = static_cast<GLboolean>(c.promotion_hint);
  GLint display_x = static_cast<GLint>(c.display_x);
  GLint display_y = static_cast<GLint>(c.display_y);
  GLint display_width = static_cast<GLint>(c.display_width);
  GLint display_height = static_cast<GLint>(c.display_height);
  error::Error error =
      DoOverlayPromotionHintCHROMIUM(texture, promotion_hint, display_x,
                                     display_y, display_width, display_height);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleSwapBuffersWithBoundsCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SwapBuffersWithBoundsCHROMIUMImmediate& c =
      *static_cast<
          const volatile gles2::cmds::SwapBuffersWithBoundsCHROMIUMImmediate*>(
          cmd_data);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t data_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLint, 4>(count, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* rects = GetImmediateDataAs<volatile const GLint*>(
      c, data_size, immediate_data_size);
  if (rects == nullptr) {
    return error::kOutOfBounds;
  }
  error::Error error = DoSwapBuffersWithBoundsCHROMIUM(count, rects);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleSetDrawRectangleCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SetDrawRectangleCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::SetDrawRectangleCHROMIUM*>(
          cmd_data);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint width = static_cast<GLint>(c.width);
  GLint height = static_cast<GLint>(c.height);
  error::Error error = DoSetDrawRectangleCHROMIUM(x, y, width, height);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleSetEnableDCLayersCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SetEnableDCLayersCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::SetEnableDCLayersCHROMIUM*>(
          cmd_data);
  GLboolean enabled = static_cast<GLboolean>(c.enabled);
  error::Error error = DoSetEnableDCLayersCHROMIUM(enabled);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleBeginRasterCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BeginRasterCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::BeginRasterCHROMIUM*>(cmd_data);
  GLuint texture_id = static_cast<GLuint>(c.texture_id);
  GLuint sk_color = static_cast<GLuint>(c.sk_color);
  GLuint msaa_sample_count = static_cast<GLuint>(c.msaa_sample_count);
  GLboolean can_use_lcd_text = static_cast<GLboolean>(c.can_use_lcd_text);
  GLboolean use_distance_field_text =
      static_cast<GLboolean>(c.use_distance_field_text);
  GLint pixel_config = static_cast<GLint>(c.pixel_config);
  error::Error error = DoBeginRasterCHROMIUM(
      texture_id, sk_color, msaa_sample_count, can_use_lcd_text,
      use_distance_field_text, pixel_config);
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleEndRasterCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  error::Error error = DoEndRasterCHROMIUM();
  if (error != error::kNoError) {
    return error;
  }
  return error::kNoError;
}

}  // namespace gles2
}  // namespace gpu
