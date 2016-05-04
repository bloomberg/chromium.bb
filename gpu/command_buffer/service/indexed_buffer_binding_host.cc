// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/indexed_buffer_binding_host.h"

#include "gpu/command_buffer/service/buffer_manager.h"

namespace gpu {
namespace gles2 {

IndexedBufferBindingHost::IndexedBufferBinding::IndexedBufferBinding()
    : type(kBindBufferNone),
      offset(0),
      size(0),
      effective_full_buffer_size(0) {
}

IndexedBufferBindingHost::IndexedBufferBinding::IndexedBufferBinding(
    const IndexedBufferBindingHost::IndexedBufferBinding& other)
    : type(other.type),
      buffer(other.buffer.get()),
      offset(other.offset),
      size(other.size),
      effective_full_buffer_size(other.effective_full_buffer_size) {
}

IndexedBufferBindingHost::IndexedBufferBinding::~IndexedBufferBinding() {
}

void IndexedBufferBindingHost::IndexedBufferBinding::SetBindBufferBase(
    Buffer* _buffer) {
  if (!_buffer) {
    Reset();
    return;
  }
  type = kBindBufferBase;
  buffer = _buffer;
  offset = 0;
  size = 0;
  effective_full_buffer_size = 0;
}

void IndexedBufferBindingHost::IndexedBufferBinding::SetBindBufferRange(
    Buffer* _buffer, GLintptr _offset, GLsizeiptr _size) {
  if (!_buffer) {
    Reset();
    return;
  }
  type = kBindBufferRange;
  buffer = _buffer;
  offset = _offset;
  size = _size;
  effective_full_buffer_size = _buffer ? _buffer->size() : 0;
}

void IndexedBufferBindingHost::IndexedBufferBinding::Reset() {
  type = kBindBufferNone;
  buffer = nullptr;
  offset = 0;
  size = 0;
  effective_full_buffer_size = 0;
}


IndexedBufferBindingHost::IndexedBufferBindingHost(
    uint32_t max_bindings, bool needs_emulation)
    : needs_emulation_(needs_emulation) {
  buffer_bindings_.resize(max_bindings);
}

IndexedBufferBindingHost::~IndexedBufferBindingHost() {
}

void IndexedBufferBindingHost::DoBindBufferBase(
    GLenum target, GLuint index, Buffer* buffer) {
  DCHECK_LT(index, buffer_bindings_.size());
  GLuint service_id = buffer ? buffer->service_id() : 0;
  glBindBufferBase(target, index, service_id);

  buffer_bindings_[index].SetBindBufferBase(buffer);
}

void IndexedBufferBindingHost::DoBindBufferRange(
    GLenum target, GLuint index, Buffer* buffer, GLintptr offset,
    GLsizeiptr size) {
  DCHECK_LT(index, buffer_bindings_.size());
  GLuint service_id = buffer ? buffer->service_id() : 0;
  if (buffer && needs_emulation_) {
    DoAdjustedBindBufferRange(
        target, index, service_id, offset, size, buffer->size());
  } else {
    glBindBufferRange(target, index, service_id, offset, size);
  }

  buffer_bindings_[index].SetBindBufferRange(buffer, offset, size);
}

// static
void IndexedBufferBindingHost::DoAdjustedBindBufferRange(
    GLenum target, GLuint index, GLuint service_id, GLintptr offset,
    GLsizeiptr size, GLsizeiptr full_buffer_size) {
  GLsizeiptr adjusted_size = size;
  if (offset >= full_buffer_size) {
    // Situation 1: We can't really call glBindBufferRange with reasonable
    // offset/size without triggering a GL error because size == 0 isn't
    // valid.
    // TODO(zmo): it's ambiguous in the GL 4.1 spec whether BindBufferBase
    // generates a GL error in such case. In reality, no error is generated on
    // MacOSX with AMD/4.1.
    glBindBufferBase(target, index, service_id);
    return;
  } else if (offset + size > full_buffer_size) {
    adjusted_size = full_buffer_size - offset;
    // size needs to be a multiple of 4.
    adjusted_size = adjusted_size & ~3;
    if (adjusted_size == 0) {
      // Situation 2: The original size is valid, but the adjusted size
      // is 0 and isn't valid. Handle it the same way as situation 1.
      glBindBufferBase(target, index, service_id);
      return;
    }
  }
  glBindBufferRange(target, index, service_id, offset, adjusted_size);
}

void IndexedBufferBindingHost::OnBindHost(GLenum target) {
  if (needs_emulation_) {
    // If some bound buffers change size since last time the transformfeedback
    // is bound, we might need to reset the ranges.
    for (size_t ii = 0; ii < buffer_bindings_.size(); ++ii) {
      Buffer* buffer = buffer_bindings_[ii].buffer.get();
      if (buffer && buffer_bindings_[ii].type == kBindBufferRange &&
          buffer_bindings_[ii].effective_full_buffer_size != buffer->size()) {
        DoAdjustedBindBufferRange(
            target, ii, buffer->service_id(), buffer_bindings_[ii].offset,
            buffer_bindings_[ii].size, buffer->size());
        buffer_bindings_[ii].effective_full_buffer_size = buffer->size();
      }
    }
  }
}

void IndexedBufferBindingHost::OnBufferData(GLenum target, Buffer* buffer) {
  DCHECK(buffer);
  if (needs_emulation_) {
    // If some bound buffers change size since last time the transformfeedback
    // is bound, we might need to reset the ranges.
    for (size_t ii = 0; ii < buffer_bindings_.size(); ++ii) {
      if (buffer_bindings_[ii].buffer.get() != buffer)
        continue;
      if (buffer_bindings_[ii].type == kBindBufferRange &&
          buffer_bindings_[ii].effective_full_buffer_size != buffer->size()) {
        DoAdjustedBindBufferRange(
            target, ii, buffer->service_id(), buffer_bindings_[ii].offset,
            buffer_bindings_[ii].size, buffer->size());
        buffer_bindings_[ii].effective_full_buffer_size = buffer->size();
      }
    }
  }
}

void IndexedBufferBindingHost::RemoveBoundBuffer(Buffer* buffer) {
  for (size_t ii = 0; ii < buffer_bindings_.size(); ++ii) {
    if (buffer_bindings_[ii].buffer.get() == buffer) {
      buffer_bindings_[ii].Reset();
    }
  }
}

Buffer* IndexedBufferBindingHost::GetBufferBinding(GLuint index) const {
  DCHECK_LT(index, buffer_bindings_.size());
  return buffer_bindings_[index].buffer.get();
}

GLsizeiptr IndexedBufferBindingHost::GetBufferSize(GLuint index) const {
  DCHECK_LT(index, buffer_bindings_.size());
  return buffer_bindings_[index].size;
}

GLintptr IndexedBufferBindingHost::GetBufferStart(GLuint index) const {
  DCHECK_LT(index, buffer_bindings_.size());
  return buffer_bindings_[index].offset;
}

}  // namespace gles2
}  // namespace gpu
