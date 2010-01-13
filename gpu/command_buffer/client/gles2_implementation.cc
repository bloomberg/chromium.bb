// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class to emluate GLES2 over command buffers.

#include "gpu/command_buffer/client/gles2_implementation.h"
// TODO(gman): remove when all functions have been implemented.
#include "gpu/command_buffer/client/gles2_implementation_gen.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"

namespace gpu {
namespace gles2 {

// A 32-bit and 64-bit compatible way of converting a pointer to a GLuint.
static GLuint ToGLuint(const void* ptr) {
  return static_cast<GLuint>(reinterpret_cast<size_t>(ptr));
}

#if !defined(COMPILER_MSVC)
const size_t GLES2Implementation::kMaxSizeOfSimpleResult;
#endif

GLES2Implementation::GLES2Implementation(
      GLES2CmdHelper* helper,
      size_t transfer_buffer_size,
      void* transfer_buffer,
      int32 transfer_buffer_id)
    : util_(0),  // TODO(gman): Get real number of compressed texture formats.
      helper_(helper),
      transfer_buffer_(transfer_buffer_size, helper, transfer_buffer),
      transfer_buffer_id_(transfer_buffer_id),
      pack_alignment_(4),
      unpack_alignment_(4) {
  // Eat 1 id so we start at 1 instead of 0.
  GLuint eat;
  MakeIds(1, &eat);
  // Allocate space for simple GL results.
  result_buffer_ = transfer_buffer_.Alloc(kMaxSizeOfSimpleResult);
  result_shm_offset_ = transfer_buffer_.GetOffset(result_buffer_);
}

GLES2Implementation::~GLES2Implementation() {
  transfer_buffer_.Free(result_buffer_);
}

void GLES2Implementation::MakeIds(GLsizei n, GLuint* ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    ids[ii] = id_allocator_.AllocateID();
  }
}

void GLES2Implementation::FreeIds(GLsizei n, const GLuint* ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    id_allocator_.FreeID(ids[ii]);
  }
}

void GLES2Implementation::WaitForCmd() {
  int32 token = helper_->InsertToken();
  helper_->WaitForToken(token);
}

void GLES2Implementation::DrawElements(
    GLenum mode, GLsizei count, GLenum type, const void* indices) {
  helper_->DrawElements(mode, count, type, ToGLuint(indices));
}

void GLES2Implementation::Finish() {
  helper_->Finish();
  WaitForCmd();
}

void GLES2Implementation::SwapBuffers() {
  helper_->SwapBuffers();
  Finish();
}

GLint GLES2Implementation::GetAttribLocation(
    GLuint program, const char* name) {
  helper_->GetAttribLocationImmediate(
      program, name, result_shm_id(), result_shm_offset());
  WaitForCmd();
  return GetResultAs<GLint>();
}

GLint GLES2Implementation::GetUniformLocation(
    GLuint program, const char* name) {
  helper_->GetUniformLocationImmediate(
      program, name, result_shm_id(), result_shm_offset());
  WaitForCmd();
  return GetResultAs<GLint>();
}

void GLES2Implementation::PixelStorei(GLenum pname, GLint param) {
  switch (pname) {
  case GL_PACK_ALIGNMENT:
      pack_alignment_ = param;
      break;
  case GL_UNPACK_ALIGNMENT:
      unpack_alignment_ = param;
      break;
  default:
      break;
  }
  helper_->PixelStorei(pname, param);
}


void GLES2Implementation::VertexAttribPointer(
    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void* ptr) {
  helper_->VertexAttribPointer(index, size, type, normalized, stride,
                               ToGLuint(ptr));
}

void GLES2Implementation::ShaderSource(
    GLuint shader, GLsizei count, const char** source, const GLint* length) {
  // TODO(gman): change to use buckets and check that there is enough room.

  // Compute the total size.
  uint32 total_size = count * sizeof(total_size);
  for (GLsizei ii = 0; ii < count; ++ii) {
    total_size += length ? length[ii] : strlen(source[ii]);
  }

  // Create string table in transfer buffer.
  char* strings = transfer_buffer_.AllocTyped<char>(total_size);
  uint32* offsets = reinterpret_cast<uint32*>(strings);
  uint32 offset = count * sizeof(*offsets);
  for (GLsizei ii = 0; ii < count; ++ii) {
    uint32 len = length ? length[ii] : strlen(source[ii]);
    memcpy(strings + offset, source[ii], len);
    offset += len;
    offsets[ii] = offset;
  }

  helper_->ShaderSource(shader, count,
                        transfer_buffer_id_,
                        transfer_buffer_.GetOffset(strings), offset);
  transfer_buffer_.FreePendingToken(strings, helper_->InsertToken());
}

void GLES2Implementation::BufferData(
    GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
  // NOTE: Should this be optimized for the case where we can call BufferData
  //    with the actual data in the case of our transfer buffer being big
  //    enough?
  helper_->BufferData(target, size, 0, 0, usage);
  if (data != NULL) {
    BufferSubData(target, 0, size, data);
  }
}

void GLES2Implementation::BufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
  const int8* source = static_cast<const int8*>(data);
  GLsizeiptr max_size = transfer_buffer_.GetLargestFreeOrPendingSize();
  while (size) {
    GLsizeiptr part_size = std::min(size, max_size);
    void* buffer = transfer_buffer_.Alloc(part_size);
    memcpy(buffer, source, part_size);
    helper_->BufferSubData(target, offset, part_size,
                           transfer_buffer_id_,
                           transfer_buffer_.GetOffset(buffer));
    transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
    offset += part_size;
    source += part_size;
    size -= part_size;
  }
}

void GLES2Implementation::CompressedTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLsizei width,
    GLsizei height, GLint border, GLsizei image_size, const void* data) {
  // TODO(gman): Switch to use buckets alwayst or at least if no room in shared
  //    memory.
  DCHECK_LE(image_size,
            static_cast<GLsizei>(
                transfer_buffer_.GetLargestFreeOrPendingSize()));
  void* buffer = transfer_buffer_.Alloc(image_size);
  memcpy(buffer, data, image_size);
  helper_->CompressedTexImage2D(
      target, level, internalformat, width, height, border, image_size,
      transfer_buffer_id_, transfer_buffer_.GetOffset(buffer));
  transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
}

void GLES2Implementation::CompressedTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei image_size, const void* data) {
  // TODO(gman): Switch to use buckets alwayst or at least if no room in shared
  //    memory.
  DCHECK_LE(image_size,
            static_cast<GLsizei>(
                transfer_buffer_.GetLargestFreeOrPendingSize()));
  void* buffer = transfer_buffer_.Alloc(image_size);
  memcpy(buffer, data, image_size);
  helper_->CompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, image_size,
      transfer_buffer_id_, transfer_buffer_.GetOffset(buffer));
  transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
}

void GLES2Implementation::TexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  helper_->TexImage2D(
      target, level, internalformat, width, height, border, format, type, 0, 0);
  if (pixels) {
    TexSubImage2D(target, level, 0, 0, width, height, format, type, pixels);
  }
}

void GLES2Implementation::TexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels) {
  const int8* source = static_cast<const int8*>(pixels);
  GLsizeiptr max_size = transfer_buffer_.GetLargestFreeOrPendingSize();

  GLsizeiptr unpadded_row_size = GLES2Util::ComputeImageDataSize(
      width, 1, format, type, unpack_alignment_);
  GLsizeiptr padded_row_size = GLES2Util::ComputeImageDataSize(
      width, 2, format, type, unpack_alignment_) - unpadded_row_size;

  if (padded_row_size <= max_size) {
    // Transfer by rows.
    GLint max_rows = max_size / padded_row_size;
    while (height) {
      GLint num_rows = std::min(height, max_rows);
      GLsizeiptr part_size = num_rows * padded_row_size;
      void* buffer = transfer_buffer_.Alloc(part_size);
      memcpy(buffer, source, part_size);
      helper_->TexSubImage2D(
          target, level, xoffset, yoffset, width, num_rows, format, type,
          transfer_buffer_id_, transfer_buffer_.GetOffset(buffer));
      transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
      yoffset += num_rows;
      source += part_size;
      height -= num_rows;
    }
  } else {
    // Transfer by sub rows. Beacuse GL has no maximum texture dimensions.
    GLsizeiptr element_size = GLES2Util::ComputeImageDataSize(
       1, 1, format, type, unpack_alignment_);
    max_size -= max_size % element_size;
    GLint max_sub_row_pixels = max_size / element_size;
    for (; height; --height) {
      GLint temp_width = width;
      GLint temp_xoffset = xoffset;
      const int8* row_source = source;
      while (temp_width) {
        GLint num_pixels = std::min(width, max_sub_row_pixels);
        GLsizeiptr part_size = num_pixels * element_size;
        void* buffer = transfer_buffer_.Alloc(part_size);
        memcpy(buffer, row_source, part_size);
        helper_->TexSubImage2D(
            target, level, temp_xoffset, yoffset, temp_width, 1, format, type,
            transfer_buffer_id_, transfer_buffer_.GetOffset(buffer));
        transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
        row_source += part_size;
        temp_xoffset += num_pixels;
        temp_width -= num_pixels;
      }
      ++yoffset;
      source += padded_row_size;
    }
  }
}


}  // namespace gles2
}  // namespace gpu
