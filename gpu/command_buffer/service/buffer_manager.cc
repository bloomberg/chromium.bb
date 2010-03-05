// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/buffer_manager.h"
#include "base/logging.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

namespace gpu {
namespace gles2 {

void BufferManager::CreateBufferInfo(GLuint buffer_id) {
  std::pair<BufferInfoMap::iterator, bool> result =
      buffer_infos_.insert(
          std::make_pair(buffer_id,
                         BufferInfo::Ref(new BufferInfo(buffer_id))));
  DCHECK(result.second);
}

BufferManager::BufferInfo* BufferManager::GetBufferInfo(
    GLuint buffer_id) {
  BufferInfoMap::iterator it = buffer_infos_.find(buffer_id);
  return it != buffer_infos_.end() ? it->second : NULL;
}

void BufferManager::RemoveBufferInfo(GLuint buffer_id) {
  BufferInfoMap::iterator it = buffer_infos_.find(buffer_id);
  if (it != buffer_infos_.end()) {
    it->second->MarkAsDeleted();
    buffer_infos_.erase(buffer_id);
  }
}

void BufferManager::BufferInfo::SetSize(GLsizeiptr size) {
  DCHECK(!IsDeleted());
  if (size != size_) {
    size_ = size;
    ClearCache();
    if (target_ == GL_ELEMENT_ARRAY_BUFFER) {
      shadow_.reset(new int8[size]);
      memset(shadow_.get(), 0, size);
    }
  }
}

bool BufferManager::BufferInfo::SetRange(
    GLintptr offset, GLsizeiptr size, const GLvoid * data) {
  DCHECK(!IsDeleted());
  if (offset + size < offset ||
        offset + size > size_) {
    return false;
  }
  if (target_ == GL_ELEMENT_ARRAY_BUFFER) {
    memcpy(shadow_.get() + offset, data, size);
    ClearCache();
  }
  return true;
}

void BufferManager::BufferInfo::ClearCache() {
  range_set_.clear();
}

template <typename T>
GLuint GetMaxValue(const void* data, GLuint offset, GLsizei count) {
  GLuint max_value = 0;
  const T* element = reinterpret_cast<const T*>(
      static_cast<const int8*>(data) + offset);
  const T* end = element + count;
  for (; element < end; ++element) {
    if (*element > max_value) {
      max_value = *element;
    }
  }
  return max_value;
}

bool BufferManager::BufferInfo::GetMaxValueForRange(
    GLuint offset, GLsizei count, GLenum type, GLuint* max_value) {
  DCHECK_EQ(target_, static_cast<GLenum>(GL_ELEMENT_ARRAY_BUFFER));
  DCHECK(!IsDeleted());
  Range range(offset, count, type);
  RangeToMaxValueMap::iterator it = range_set_.find(range);
  if (it != range_set_.end()) {
    *max_value = it->second;
    return true;
  }

  uint32 size;
  if (!SafeMultiplyUint32(
      count, GLES2Util::GetGLTypeSizeForTexturesAndBuffers(type), &size)) {
    return false;
  }

  if (!SafeAddUint32(offset, size, &size)) {
    return false;
  }

  if (size > static_cast<uint32>(size_)) {
    return false;
  }

  // Scan the range for the max value and store
  GLuint max_v = 0;
  switch (type) {
    case GL_UNSIGNED_BYTE:
      max_v = GetMaxValue<uint8>(shadow_.get(), offset, count);
      break;
    case GL_UNSIGNED_SHORT:
      // Check we are not accessing an odd byte for a 2 byte value.
      if ((offset & 1) != 0) {
        return false;
      }
      max_v = GetMaxValue<uint16>(shadow_.get(), offset, count);
      break;
    default:
      NOTREACHED();  // should never get here by validation.
      break;
  }
  std::pair<RangeToMaxValueMap::iterator, bool> result =
      range_set_.insert(std::make_pair(range, max_v));
  *max_value = max_v;
  return true;
}

}  // namespace gles2
}  // namespace gpu


