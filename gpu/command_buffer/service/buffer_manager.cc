// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/buffer_manager.h"
#include <limits>
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/memory_tracking.h"

namespace gpu {
namespace gles2 {

BufferManager::BufferManager(MemoryTracker* memory_tracker)
    : memory_tracker_(
          new MemoryTypeTracker(memory_tracker, MemoryTracker::kManaged)),
      allow_buffers_on_multiple_targets_(false),
      buffer_info_count_(0),
      have_context_(true) {
}

BufferManager::~BufferManager() {
  DCHECK(buffer_infos_.empty());
  CHECK_EQ(buffer_info_count_, 0u);
}

void BufferManager::Destroy(bool have_context) {
  have_context_ = have_context;
  buffer_infos_.clear();
  DCHECK_EQ(0u, memory_tracker_->GetMemRepresented());
}

void BufferManager::CreateBuffer(GLuint client_id, GLuint service_id) {
  scoped_refptr<Buffer> buffer(new Buffer(this, service_id));
  std::pair<BufferInfoMap::iterator, bool> result =
      buffer_infos_.insert(std::make_pair(client_id, buffer));
  DCHECK(result.second);
}

Buffer* BufferManager::GetBuffer(
    GLuint client_id) {
  BufferInfoMap::iterator it = buffer_infos_.find(client_id);
  return it != buffer_infos_.end() ? it->second : NULL;
}

void BufferManager::RemoveBuffer(GLuint client_id) {
  BufferInfoMap::iterator it = buffer_infos_.find(client_id);
  if (it != buffer_infos_.end()) {
    Buffer* buffer = it->second;
    buffer->MarkAsDeleted();
    buffer_infos_.erase(it);
  }
}

void BufferManager::StartTracking(Buffer* /* buffer */) {
  ++buffer_info_count_;
}

void BufferManager::StopTracking(Buffer* buffer) {
  memory_tracker_->TrackMemFree(buffer->size());
  --buffer_info_count_;
}

Buffer::Buffer(BufferManager* manager, GLuint service_id)
    : manager_(manager),
      deleted_(false),
      service_id_(service_id),
      target_(0),
      size_(0),
      usage_(GL_STATIC_DRAW),
      shadowed_(false) {
  manager_->StartTracking(this);
}

Buffer::~Buffer() {
  if (manager_) {
    if (manager_->have_context_) {
      GLuint id = service_id();
      glDeleteBuffersARB(1, &id);
    }
    manager_->StopTracking(this);
    manager_ = NULL;
  }
}

void Buffer::SetInfo(
    GLsizeiptr size, GLenum usage, bool shadow) {
  usage_ = usage;
  if (size != size_ || shadow != shadowed_) {
    shadowed_ = shadow;
    size_ = size;
    ClearCache();
    if (shadowed_) {
      shadow_.reset(new int8[size]);
      memset(shadow_.get(), 0, size);
    }
  }
}

bool Buffer::CheckRange(
    GLintptr offset, GLsizeiptr size) const {
  int32 end = 0;
  return offset >= 0 && size >= 0 &&
         offset <= std::numeric_limits<int32>::max() &&
         size <= std::numeric_limits<int32>::max() &&
         SafeAddInt32(offset, size, &end) && end <= size_;
}

bool Buffer::SetRange(
    GLintptr offset, GLsizeiptr size, const GLvoid * data) {
  if (!CheckRange(offset, size)) {
    return false;
  }
  if (shadowed_) {
    memcpy(shadow_.get() + offset, data, size);
    ClearCache();
  }
  return true;
}

const void* Buffer::GetRange(
    GLintptr offset, GLsizeiptr size) const {
  if (!shadowed_) {
    return NULL;
  }
  if (!CheckRange(offset, size)) {
    return NULL;
  }
  return shadow_.get() + offset;
}

void Buffer::ClearCache() {
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

bool Buffer::GetMaxValueForRange(
    GLuint offset, GLsizei count, GLenum type, GLuint* max_value) {
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

  if (!shadowed_) {
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
    case GL_UNSIGNED_INT:
      // Check we are not accessing a non aligned address for a 4 byte value.
      if ((offset & 3) != 0) {
        return false;
      }
      max_v = GetMaxValue<uint32>(shadow_.get(), offset, count);
      break;
    default:
      NOTREACHED();  // should never get here by validation.
      break;
  }
  range_set_.insert(std::make_pair(range, max_v));
  *max_value = max_v;
  return true;
}

bool BufferManager::GetClientId(GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (BufferInfoMap::const_iterator it = buffer_infos_.begin();
       it != buffer_infos_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

void BufferManager::SetInfo(
    Buffer* info, GLsizeiptr size, GLenum usage) {
  DCHECK(info);
  memory_tracker_->TrackMemFree(info->size());
  info->SetInfo(size,
                usage,
                info->target() == GL_ELEMENT_ARRAY_BUFFER ||
                allow_buffers_on_multiple_targets_);
  memory_tracker_->TrackMemAlloc(info->size());
}

bool BufferManager::SetTarget(Buffer* info, GLenum target) {
  // Check that we are not trying to bind it to a different target.
  if (info->target() != 0 && info->target() != target &&
      !allow_buffers_on_multiple_targets_) {
    return false;
  }
  if (info->target() == 0) {
    info->set_target(target);
  }
  return true;
}

}  // namespace gles2
}  // namespace gpu


