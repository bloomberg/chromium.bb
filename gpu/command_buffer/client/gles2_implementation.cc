// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class to emluate GLES2 over command buffers.

#include "gpu/command_buffer/client/gles2_implementation.h"
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
      unpack_alignment_(4),
      error_bits_(0) {
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
  helper_->CommandBufferHelper::Finish();
}

GLenum GLES2Implementation::GetError() {
  return GetGLError();
}

GLenum GLES2Implementation::GetGLError() {
  // Check the GL error first, then our wrapped error.
  typedef gles2::GetError::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = GL_NO_ERROR;
  helper_->GetError(result_shm_id(), result_shm_offset());
  WaitForCmd();
  GLenum error = *result;
  if (error == GL_NO_ERROR && error_bits_ != 0) {
    for (uint32 mask = 1; mask != 0; mask = mask << 1) {
      if ((error_bits_ & mask) != 0) {
        error = GLES2Util::GLErrorBitToGLError(mask);
        break;
      }
    }
  }

  if (error != GL_NO_ERROR) {
    // There was an error, clear the corresponding wrapped error.
    error_bits_ &= ~GLES2Util::GLErrorToErrorBit(error);
  }
  return error;
}

void GLES2Implementation::SetGLError(GLenum error) {
  error_bits_ |= GLES2Util::GLErrorToErrorBit(error);
}

void GLES2Implementation::GetBucketContents(uint32 bucket_id,
                                            std::vector<int8>* data) {
  DCHECK(data);
  typedef cmd::GetBucketSize::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->GetBucketSize(bucket_id, result_shm_id(), result_shm_offset());
  WaitForCmd();
  uint32 size = *result;
  data->resize(size);
  if (size > 0u) {
    uint32 max_size = transfer_buffer_.GetLargestFreeOrPendingSize();
    uint32 offset = 0;
    while (size) {
      uint32 part_size = std::min(max_size, size);
      void* buffer = transfer_buffer_.Alloc(part_size);
      helper_->GetBucketData(
          bucket_id, offset, part_size,
          transfer_buffer_id_, transfer_buffer_.GetOffset(buffer));
      WaitForCmd();
      memcpy(&(*data)[offset], buffer, part_size);
      transfer_buffer_.Free(buffer);
      offset += part_size;
      size -= part_size;
    }
    // Free the bucket. This is not required but it does free up the memory.
    // and we don't have to wait for the result so from the client's perspective
    // it's cheap.
    helper_->SetBucketSize(bucket_id, 0);
  }
}

void GLES2Implementation::SetBucketContents(
    uint32 bucket_id, const void* data, size_t size) {
  DCHECK(data);
  helper_->SetBucketSize(bucket_id, size);
  if (size > 0u) {
    uint32 max_size = transfer_buffer_.GetLargestFreeOrPendingSize();
    uint32 offset = 0;
    while (size) {
      uint32 part_size = std::min(static_cast<size_t>(max_size), size);
      void* buffer = transfer_buffer_.Alloc(part_size);
      memcpy(buffer, static_cast<const int8*>(data) + offset, part_size);
      helper_->SetBucketData(
          bucket_id, offset, part_size,
          transfer_buffer_id_, transfer_buffer_.GetOffset(buffer));
      transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
      offset += part_size;
      size -= part_size;
    }
  }
}

bool GLES2Implementation::GetBucketAsString(
    uint32 bucket_id, std::string* str) {
  DCHECK(str);
  std::vector<int8> data;
  // NOTE: strings are passed NULL terminated. That means the empty
  // string will have a size of 1 and no-string will have a size of 0
  GetBucketContents(bucket_id, &data);
  if (data.empty()) {
    return false;
  }
  str->assign(&data[0], &data[0] + data.size() - 1);
  return true;
}

void GLES2Implementation::SetBucketAsString(
    uint32 bucket_id, const std::string& str) {
  // NOTE: strings are passed NULL terminated. That means the empty
  // string will have a size of 1 and no-string will have a size of 0
  SetBucketContents(bucket_id, str.c_str(), str.size() + 1);
}

void GLES2Implementation::DrawElements(
    GLenum mode, GLsizei count, GLenum type, const void* indices) {
  helper_->DrawElements(mode, count, type, ToGLuint(indices));
}

void GLES2Implementation::Flush() {
  // Insert the cmd to call glFlush
  helper_->Flush();
  // Flush our command buffer
  // (tell the service to execute upto the flush cmd.)
  helper_->CommandBufferHelper::Flush();
}

void GLES2Implementation::Finish() {
  // Insert the cmd to call glFinish
  helper_->Finish();
  // Finish our command buffer
  // (tell the service to execute upto the Finish cmd and wait for it to
  // execute.)
  helper_->CommandBufferHelper::Finish();
}

void GLES2Implementation::SwapBuffers() {
  helper_->SwapBuffers();
  Flush();
}

void GLES2Implementation::GetVertexAttribPointerv(
    GLuint index, GLenum pname, void** ptr) {
  helper_->GetVertexAttribPointerv(
    index, pname, result_shm_id(), result_shm_offset());
  WaitForCmd();
  static_cast<gles2::GetVertexAttribPointerv::Result*>(
      result_buffer_)->CopyResult(ptr);
};

GLint GLES2Implementation::GetAttribLocation(
    GLuint program, const char* name) {
  typedef cmd::GetBucketSize::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = -1;
  helper_->GetAttribLocationImmediate(
      program, name, result_shm_id(), result_shm_offset());
  WaitForCmd();
  return *result;
}

GLint GLES2Implementation::GetUniformLocation(
    GLuint program, const char* name) {
  typedef cmd::GetBucketSize::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = -1;
  helper_->GetUniformLocationImmediate(
      program, name, result_shm_id(), result_shm_offset());
  WaitForCmd();
  return *result;
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
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  // TODO(gman): change to use buckets and check that there is enough room.

  // Compute the total size.
  uint32 total_size = 0;
  for (GLsizei ii = 0; ii < count; ++ii) {
    total_size += length ? length[ii] : strlen(source[ii]);
  }

  // Concatenate all the strings in to the transfer buffer.
  char* strings = transfer_buffer_.AllocTyped<char>(total_size);
  uint32 offset = 0;
  for (GLsizei ii = 0; ii < count; ++ii) {
    uint32 len = length ? length[ii] : strlen(source[ii]);
    memcpy(strings + offset, source[ii], len);
    offset += len;
  }
  DCHECK_EQ(total_size, offset);

  helper_->ShaderSource(shader,
                        transfer_buffer_id_,
                        transfer_buffer_.GetOffset(strings),
                        total_size);
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
  if (size == 0) {
    return;
  }

  if (size < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }

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
  if (width < 0 || height < 0 || level < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  // TODO(gman): Switch to use buckets always or at least if no room in shared
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
  if (width < 0 || height < 0 || level < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  // TODO(gman): Switch to use buckets always or at least if no room in shared
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
  if (level < 0 || height < 0 || width < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  uint32 size;
  if (!GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_, &size)) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  helper_->TexImage2D(
      target, level, internalformat, width, height, border, format, type, 0, 0);
  if (pixels) {
    TexSubImage2D(target, level, 0, 0, width, height, format, type, pixels);
  }
}

void GLES2Implementation::TexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels) {
  if (level < 0 || height < 0 || width < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  const int8* source = static_cast<const int8*>(pixels);
  GLsizeiptr max_size = transfer_buffer_.GetLargestFreeOrPendingSize();
  uint32 temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 1, format, type, unpack_alignment_, &temp_size)) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  GLsizeiptr unpadded_row_size = temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 2, format, type, unpack_alignment_, &temp_size)) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  GLsizeiptr padded_row_size = temp_size - unpadded_row_size;
  if (padded_row_size < 0 || unpadded_row_size < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }

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
    uint32 temp;
    GLES2Util::ComputeImageDataSize(
       1, 1, format, type, unpack_alignment_, &temp);
    GLsizeiptr element_size = temp;
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

GLenum GLES2Implementation::CheckFramebufferStatus(GLenum target) {
  // TODO(gman): implement.
  return 0;
}

void GLES2Implementation::GetActiveAttrib(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  // Clear the bucket so if we the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  typedef gles2::GetActiveAttrib::Result Result;
  Result* result = static_cast<Result*>(result_buffer_);
  // Set as failed so if the command fails we'll recover.
  result->success = false;
  helper_->GetActiveAttrib(program, index, kResultBucketId,
                           result_shm_id(), result_shm_offset());
  WaitForCmd();
  if (result->success) {
    if (size) {
      *size = result->size;
    }
    if (type) {
      *type = result->type;
    }
    if (length || name) {
      std::vector<int8> str;
      GetBucketContents(kResultBucketId, &str);
      GLsizei max_size = std::min(static_cast<size_t>(bufsize) - 1,
                                  str.size());
      if (length) {
        *length = max_size;
      }
      if (name && bufsize > 0) {
        memcpy(name, &str[0], max_size);
        name[max_size] = '\0';
      }
    }
  }
}

void GLES2Implementation::GetActiveUniform(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  // Clear the bucket so if we the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  typedef gles2::GetActiveUniform::Result Result;
  Result* result = static_cast<Result*>(result_buffer_);
  // Set as failed so if the command fails we'll recover.
  result->success = false;
  helper_->GetActiveUniform(program, index, kResultBucketId,
                            result_shm_id(), result_shm_offset());
  WaitForCmd();
  if (result->success) {
    if (size) {
      *size = result->size;
    }
    if (type) {
      *type = result->type;
    }
    if (length || name) {
      std::vector<int8> str;
      GetBucketContents(kResultBucketId, &str);
      GLsizei max_size = std::min(static_cast<size_t>(bufsize) - 1,
                                  str.size());
      if (length) {
        *length = max_size;
      }
      if (name && bufsize > 0) {
        memcpy(name, &str[0], max_size);
        name[max_size] = '\0';
      }
    }
  }
}

void GLES2Implementation::GetAttachedShaders(
    GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) {
  if (maxcount < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  typedef gles2::GetAttachedShaders::Result Result;
  uint32 size = Result::ComputeSize(maxcount);
  Result* result = transfer_buffer_.AllocTyped<Result>(size);
  helper_->GetAttachedShaders(
    program,
    transfer_buffer_id_,
    transfer_buffer_.GetOffset(result),
    size);
  int32 token = helper_->InsertToken();
  WaitForCmd();
  if (count) {
    *count = result->GetNumResults();
  }
  result->CopyResult(shaders);
  transfer_buffer_.FreePendingToken(result, token);
}

void GLES2Implementation::GetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
  typedef gles2::GetShaderPrecisionFormat::Result Result;
  Result* result = static_cast<Result*>(result_buffer_);
  helper_->GetShaderPrecisionFormat(
    shadertype, precisiontype, result_shm_id(), result_shm_offset());
  WaitForCmd();
  if (result->success) {
    if (range) {
      range[0] = result->min_range;
      range[1] = result->max_range;
    }
    if (precision) {
      precision[0] = result->precision;
    }
  }
}

const GLubyte* GLES2Implementation::GetString(GLenum name) {
  const char* result;
  GLStringMap::const_iterator it = gl_strings_.find(name);
  if (it != gl_strings_.end()) {
    result = it->second.c_str();
  } else {
    // Clear the bucket so if we the command fails nothing will be in it.
    helper_->SetBucketSize(kResultBucketId, 0);
    helper_->GetString(name, kResultBucketId);
    std::string str;
    if (GetBucketAsString(kResultBucketId, &str)) {
      std::pair<GLStringMap::const_iterator, bool> insert_result =
          gl_strings_.insert(std::make_pair(name, str));
      DCHECK(insert_result.second);
      result = insert_result.first->second.c_str();
    } else {
      result = NULL;
    }
  }
  return reinterpret_cast<const GLubyte*>(result);
}

void GLES2Implementation::GetUniformfv(
    GLuint program, GLint location, GLfloat* params) {
  helper_->GetUniformfv(
      program, location, result_shm_id(), result_shm_offset());
  WaitForCmd();
  static_cast<gles2::GetUniformfv::Result*>(result_buffer_)->CopyResult(params);
}

void GLES2Implementation::GetUniformiv(
    GLuint program, GLint location, GLint* params) {
  helper_->GetUniformiv(
      program, location, result_shm_id(), result_shm_offset());
  WaitForCmd();
  static_cast<gles2::GetUniformfv::Result*>(result_buffer_)->CopyResult(params);
}

void GLES2Implementation::ReadPixels(
    GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLenum type, void* pixels) {
  // Note: Negative widths and heights are not handled here but are handled
  // by the service side so the glGetError wrapping works.
  if (width < 0 || height < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  if (width == 0 || height == 0) {
    return;
  }
  typedef gles2::ReadPixels::Result Result;
  Result* result = static_cast<Result*>(result_buffer_);
  int8* dest = reinterpret_cast<int8*>(pixels);
  GLsizeiptr max_size = transfer_buffer_.GetLargestFreeOrPendingSize();
  uint32 temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 1, format, type, pack_alignment_, &temp_size)) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  GLsizeiptr unpadded_row_size = temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 2, format, type, pack_alignment_, &temp_size)) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  GLsizeiptr padded_row_size = temp_size - unpadded_row_size;
  if (padded_row_size < 0 || unpadded_row_size < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  if (padded_row_size <= max_size) {
    // Transfer by rows.
    GLint max_rows = max_size / padded_row_size;
    while (height) {
      GLint num_rows = std::min(height, max_rows);
      GLsizeiptr part_size = num_rows * padded_row_size;
      void* buffer = transfer_buffer_.Alloc(part_size);
      *result = 0;  // mark as failed.
      helper_->ReadPixels(
          xoffset, yoffset, width, num_rows, format, type,
          transfer_buffer_id_, transfer_buffer_.GetOffset(buffer),
          result_shm_id(), result_shm_offset());
      WaitForCmd();
      // If it was not marked as successful exit.
      if (*result == 0) {
        return;
      }
      memcpy(dest, buffer, part_size);
      transfer_buffer_.Free(buffer);
      yoffset += num_rows;
      dest += part_size;
      height -= num_rows;
    }
  } else {
    // Transfer by sub rows. Beacuse GL has no maximum texture dimensions.
    GLES2Util::ComputeImageDataSize(
       1, 1, format, type, pack_alignment_, &temp_size);
    GLsizeiptr element_size = temp_size;
    max_size -= max_size % element_size;
    GLint max_sub_row_pixels = max_size / element_size;
    for (; height; --height) {
      GLint temp_width = width;
      GLint temp_xoffset = xoffset;
      int8* row_dest = dest;
      while (temp_width) {
        GLint num_pixels = std::min(width, max_sub_row_pixels);
        GLsizeiptr part_size = num_pixels * element_size;
        void* buffer = transfer_buffer_.Alloc(part_size);
        *result = 0;  // mark as failed.
        helper_->ReadPixels(
            temp_xoffset, yoffset, temp_width, 1, format, type,
            transfer_buffer_id_, transfer_buffer_.GetOffset(buffer),
            result_shm_id(), result_shm_offset());
        WaitForCmd();
        // If it was not marked as successful exit.
        if (*result == 0) {
          return;
        }
        memcpy(row_dest, buffer, part_size);
        transfer_buffer_.Free(buffer);
        row_dest += part_size;
        temp_xoffset += num_pixels;
        temp_width -= num_pixels;
      }
      ++yoffset;
      dest += padded_row_size;
    }
  }
}

}  // namespace gles2
}  // namespace gpu
