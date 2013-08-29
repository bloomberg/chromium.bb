// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/stream_texture_manager_in_process_android.h"

#include "base/bind.h"
#include "ui/gfx/size.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

StreamTextureManagerInProcess::StreamTextureImpl::StreamTextureImpl(
    uint32 service_id,
    uint32 stream_id)
    : surface_texture_(new gfx::SurfaceTexture(service_id)),
      stream_id_(stream_id) {}

StreamTextureManagerInProcess::StreamTextureImpl::~StreamTextureImpl() {}

void StreamTextureManagerInProcess::StreamTextureImpl::Update() {
  GLint texture_id = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &texture_id);
  surface_texture_->UpdateTexImage();
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
}

gfx::Size StreamTextureManagerInProcess::StreamTextureImpl::GetSize() {
  return size_;
}

void StreamTextureManagerInProcess::StreamTextureImpl::SetSize(gfx::Size size) {
  size_ = size;
}

scoped_refptr<gfx::SurfaceTexture>
StreamTextureManagerInProcess::StreamTextureImpl::GetSurfaceTexture() {
  return surface_texture_;
}

StreamTextureManagerInProcess::StreamTextureManagerInProcess() : next_id_(1) {}

StreamTextureManagerInProcess::~StreamTextureManagerInProcess() {
  if (!textures_.empty()) {
    LOG(WARNING) << "Undestroyed surface textures while tearing down "
                    "StreamTextureManager.";
  }
}

GLuint StreamTextureManagerInProcess::CreateStreamTexture(uint32 service_id,
                                                          uint32 client_id) {
  base::AutoLock lock(map_lock_);
  uint32 stream_id = next_id_++;
  linked_ptr<StreamTextureImpl> texture(
      new StreamTextureImpl(service_id, stream_id));
  textures_[service_id] = texture;

  if (next_id_ == 0)
    next_id_++;

  return stream_id;
}

void StreamTextureManagerInProcess::DestroyStreamTexture(uint32 service_id) {
  base::AutoLock lock(map_lock_);
  textures_.erase(service_id);
}

gpu::StreamTexture* StreamTextureManagerInProcess::LookupStreamTexture(
    uint32 service_id) {
  base::AutoLock lock(map_lock_);
  TextureMap::const_iterator it = textures_.find(service_id);
  if (it != textures_.end())
    return it->second.get();

  return NULL;
}

scoped_refptr<gfx::SurfaceTexture>
StreamTextureManagerInProcess::GetSurfaceTexture(uint32 stream_id) {
  base::AutoLock lock(map_lock_);
  for (TextureMap::iterator it = textures_.begin(); it != textures_.end();
       it++) {
    if (it->second->stream_id() == stream_id)
      return it->second->GetSurfaceTexture();
  }

  return NULL;
}

}  // namespace gpu
