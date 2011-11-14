// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/accelerated_surface_container_linux.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/image_transport_client.h"
#include "ui/gfx/compositor/compositor_cc.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/scoped_make_current.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace {

class AcceleratedSurfaceContainerLinuxCC
    : public AcceleratedSurfaceContainerLinux, public ui::TextureCC {
 public:
  explicit AcceleratedSurfaceContainerLinuxCC(const gfx::Size& size)
      : acquired_(false) {
    size_ = size;
  }

  virtual ~AcceleratedSurfaceContainerLinuxCC() {
    if (texture_id_) {
      ui::SharedResourcesCC* instance = ui::SharedResourcesCC::GetInstance();
      DCHECK(instance);
      scoped_ptr<gfx::ScopedMakeCurrent> bind(instance->GetScopedMakeCurrent());
      glDeleteTextures(1, &texture_id_);
    }

    if (image_transport_client_.get())
      image_transport_client_->Release();
  }

  virtual void AddRef() { ui::TextureCC::AddRef(); }
  virtual void Release() { ui::TextureCC::Release(); }

  virtual bool Initialize(uint64* surface_id) OVERRIDE {
    ui::SharedResourcesCC* instance = ui::SharedResourcesCC::GetInstance();
    DCHECK(instance);
    image_transport_client_.reset(
        ImageTransportClient::Create(instance, size_));
    if (!image_transport_client_.get())
      return false;

    texture_id_ = image_transport_client_->Initialize(surface_id);
    if (!texture_id_) {
      image_transport_client_.reset();
      return false;
    }
    flipped_ = image_transport_client_->Flipped();
    return true;
  }

  // TextureCC implementation
  virtual void Update() OVERRIDE {
    ui::SharedResourcesCC* instance = ui::SharedResourcesCC::GetInstance();
    DCHECK(instance);
    scoped_ptr<gfx::ScopedMakeCurrent> bind(instance->GetScopedMakeCurrent());
    if (acquired_)
      image_transport_client_->Release();
    else
      acquired_ = true;
    image_transport_client_->Acquire();
  }

  virtual TransportDIB::Handle Handle() const {
    return image_transport_client_->Handle();
  }

  virtual ui::Texture* GetTexture() { return this; }

 private:
  scoped_ptr<ImageTransportClient> image_transport_client_;
  bool acquired_;
  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerLinuxCC);
};

}  // namespace

// static
AcceleratedSurfaceContainerLinux*
AcceleratedSurfaceContainerLinux::Create(const gfx::Size& size) {
  return new AcceleratedSurfaceContainerLinuxCC(size);
}
