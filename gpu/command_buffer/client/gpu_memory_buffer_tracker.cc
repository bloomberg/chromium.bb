// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gpu_memory_buffer_tracker.h"

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/image_factory.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {
namespace gles2 {

GpuMemoryBufferTracker::GpuMemoryBufferTracker(ImageFactory* factory)
    : buffers_(),
      factory_(factory) {
}

GpuMemoryBufferTracker::~GpuMemoryBufferTracker() {
  while (!buffers_.empty()) {
    RemoveBuffer(buffers_.begin()->first);
  }
}

GLuint GpuMemoryBufferTracker::CreateBuffer(
    GLsizei width, GLsizei height, GLenum internalformat) {
  GLuint image_id = 0;
  DCHECK(factory_);
  scoped_ptr<gfx::GpuMemoryBuffer> buffer =
      factory_->CreateGpuMemoryBuffer(width, height, internalformat, &image_id);

  if (buffer.get() == NULL)
    return 0;

  std::pair<BufferMap::iterator, bool> result =
      buffers_.insert(std::make_pair(image_id, buffer.release()));
  GPU_DCHECK(result.second);

  return image_id;
}

gfx::GpuMemoryBuffer* GpuMemoryBufferTracker::GetBuffer(GLuint image_id) {
  BufferMap::iterator it = buffers_.find(image_id);
  return (it != buffers_.end()) ? it->second : NULL;
}

void GpuMemoryBufferTracker::RemoveBuffer(GLuint image_id) {
  BufferMap::iterator buffer_it = buffers_.find(image_id);
  if (buffer_it != buffers_.end()) {
    gfx::GpuMemoryBuffer* buffer = buffer_it->second;
    buffers_.erase(buffer_it);
    delete buffer;
  }
  DCHECK(factory_);
  factory_->DeleteGpuMemoryBuffer(image_id);
}

}  // namespace gles2
}  // namespace gpu
