// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/accelerated_surface_container_linux.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/image_transport_client.h"
#include "ui/gfx/compositor/compositor_gl.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace {

class AcceleratedSurfaceContainerLinuxGL
    : public AcceleratedSurfaceContainerLinux, public ui::TextureGL {
 public:
  explicit AcceleratedSurfaceContainerLinuxGL(const gfx::Size& size)
      : ui::TextureGL(size) {
  }

  virtual ~AcceleratedSurfaceContainerLinuxGL() { }
  virtual void AddRef() { ui::TextureGL::AddRef(); }
  virtual void Release() { ui::TextureGL::Release(); }

  virtual bool Initialize(uint64* surface_handle) OVERRIDE {
    ui::SharedResourcesGL* instance = ui::SharedResourcesGL::GetInstance();
    DCHECK(instance);
    image_transport_client_.reset(
        ImageTransportClient::Create(instance, size_));
    if (!image_transport_client_.get())
      return false;

    texture_id_ = image_transport_client_->Initialize(surface_handle);
    if (!texture_id_) {
      image_transport_client_.reset();
      return false;
    }
    return true;
  }

  virtual const gfx::Size& GetSize() {
    return ui::TextureGL::size();
  }

  // TextureGL implementation
  virtual void SetCanvas(const SkCanvas& canvas,
                         const gfx::Point& origin,
                         const gfx::Size& overall_size) OVERRIDE {
    NOTREACHED();
  }

  virtual void Draw(const ui::TextureDrawParams& params,
                    const gfx::Rect& clip_bounds_in_texture) OVERRIDE {
    image_transport_client_->Acquire();

    ui::TextureDrawParams modified_params = params;
    if (image_transport_client_->Flipped())
      modified_params.vertically_flipped = true;

    ui::SharedResourcesGL* instance = ui::SharedResourcesGL::GetInstance();
    DrawInternal(*instance->program_no_swizzle(),
        modified_params,
        clip_bounds_in_texture);

    image_transport_client_->Release();
  }

  virtual TransportDIB::Handle Handle() const {
    return image_transport_client_->Handle();
  }

  virtual ui::Texture* GetTexture() { return this; }

 private:
  scoped_ptr<ImageTransportClient> image_transport_client_;
  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerLinuxGL);
};

}  // namespace

// static
AcceleratedSurfaceContainerLinux*
AcceleratedSurfaceContainerLinux::Create(const gfx::Size& size) {
  return new AcceleratedSurfaceContainerLinuxGL(size);
}
