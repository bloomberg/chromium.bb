// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/stream_texture_manager_in_process_android.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"

namespace gpu {

namespace {

// Simply wraps a SurfaceTexture reference as a GLImage.
class GLImageImpl : public gl::GLImage {
 public:
  GLImageImpl(uint32_t texture_id,
              gles2::TextureManager* texture_manager,
              const scoped_refptr<gfx::SurfaceTexture>& surface_texture,
              const base::Closure& release_callback);

  // implement gl::GLImage
  void Destroy(bool have_context) override;
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override;
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;

 private:
  ~GLImageImpl() override;

  uint32_t texture_id_;
  gles2::TextureManager* texture_manager_;
  scoped_refptr<gfx::SurfaceTexture> surface_texture_;
  base::Closure release_callback_;

  DISALLOW_COPY_AND_ASSIGN(GLImageImpl);
};

GLImageImpl::GLImageImpl(
    uint32_t texture_id,
    gles2::TextureManager* texture_manager,
    const scoped_refptr<gfx::SurfaceTexture>& surface_texture,
    const base::Closure& release_callback)
    : texture_id_(texture_id),
      texture_manager_(texture_manager),
      surface_texture_(surface_texture),
      release_callback_(release_callback) {}

GLImageImpl::~GLImageImpl() {
  release_callback_.Run();
}

void GLImageImpl::Destroy(bool have_context) {
  NOTREACHED();
}

gfx::Size GLImageImpl::GetSize() {
  return gfx::Size();
}

unsigned GLImageImpl::GetInternalFormat() {
  return GL_RGBA;
}

bool GLImageImpl::BindTexImage(unsigned target) {
  NOTREACHED();
  return false;
}

void GLImageImpl::ReleaseTexImage(unsigned target) {
  NOTREACHED();
}

bool GLImageImpl::CopyTexImage(unsigned target) {
  if (target != GL_TEXTURE_EXTERNAL_OES)
    return false;

  GLint texture_id;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &texture_id);
  DCHECK(texture_id);

  // The following code only works if we're being asked to copy into
  // |texture_id_|. Copying into a different texture is not supported.
  if (static_cast<unsigned>(texture_id) != texture_id_)
    return false;

  surface_texture_->UpdateTexImage();

  gles2::Texture* texture =
      texture_manager_->GetTextureForServiceId(texture_id_);
  if (texture) {
    // By setting image state to UNBOUND instead of COPIED we ensure that
    // CopyTexImage() is called each time the surface texture is used for
    // drawing.
    texture->SetLevelImage(GL_TEXTURE_EXTERNAL_OES, 0, this,
                           gles2::Texture::UNBOUND);
  }
  return true;
}

bool GLImageImpl::CopyTexSubImage(unsigned target,
                                  const gfx::Point& offset,
                                  const gfx::Rect& rect) {
  return false;
}

bool GLImageImpl::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                       int z_order,
                                       gfx::OverlayTransform transform,
                                       const gfx::Rect& bounds_rect,
                                       const gfx::RectF& crop_rect) {
  NOTREACHED();
  return false;
}

void GLImageImpl::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                               uint64_t process_tracing_id,
                               const std::string& dump_name) {
  // TODO(ericrk): Implement GLImage OnMemoryDump. crbug.com/514914
}

}  // anonymous namespace

StreamTextureManagerInProcess::StreamTextureManagerInProcess()
    : next_id_(1), weak_factory_(this) {}

StreamTextureManagerInProcess::~StreamTextureManagerInProcess() {
  if (!textures_.empty()) {
    LOG(WARNING) << "Undestroyed surface textures while tearing down "
                    "StreamTextureManager.";
  }
}

GLuint StreamTextureManagerInProcess::CreateStreamTexture(
    uint32_t client_texture_id,
    gles2::TextureManager* texture_manager) {
  CalledOnValidThread();

  gles2::TextureRef* texture = texture_manager->GetTexture(client_texture_id);

  if (!texture || (texture->texture()->target() &&
                   texture->texture()->target() != GL_TEXTURE_EXTERNAL_OES)) {
    return 0;
  }

  scoped_refptr<gfx::SurfaceTexture> surface_texture(
      gfx::SurfaceTexture::Create(texture->service_id()));

  uint32_t stream_id = next_id_++;
  base::Closure release_callback =
      base::Bind(&StreamTextureManagerInProcess::OnReleaseStreamTexture,
                 weak_factory_.GetWeakPtr(), stream_id);
  scoped_refptr<gl::GLImage> gl_image(
      new GLImageImpl(texture->service_id(), texture_manager, surface_texture,
                      release_callback));

  gfx::Size size = gl_image->GetSize();
  texture_manager->SetTarget(texture, GL_TEXTURE_EXTERNAL_OES);
  texture_manager->SetLevelInfo(texture, GL_TEXTURE_EXTERNAL_OES, 0, GL_RGBA,
                                size.width(), size.height(), 1, 0, GL_RGBA,
                                GL_UNSIGNED_BYTE, gfx::Rect(size));
  texture_manager->SetLevelImage(texture, GL_TEXTURE_EXTERNAL_OES, 0,
                                 gl_image.get(), gles2::Texture::UNBOUND);

  {
    base::AutoLock lock(map_lock_);
    textures_[stream_id] = surface_texture;
  }

  if (next_id_ == 0)
    next_id_++;

  return stream_id;
}

void StreamTextureManagerInProcess::OnReleaseStreamTexture(uint32_t stream_id) {
  CalledOnValidThread();
  base::AutoLock lock(map_lock_);
  textures_.erase(stream_id);
}

// This can get called from any thread.
scoped_refptr<gfx::SurfaceTexture>
StreamTextureManagerInProcess::GetSurfaceTexture(uint32_t stream_id) {
  base::AutoLock lock(map_lock_);
  TextureMap::const_iterator it = textures_.find(stream_id);
  if (it != textures_.end())
    return it->second;

  return NULL;
}

}  // namespace gpu
