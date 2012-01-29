// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/accelerated_surface_container_linux.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/image_transport_client.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/scoped_make_current.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

AcceleratedSurfaceContainerLinux::AcceleratedSurfaceContainerLinux(
    const gfx::Size& size)
    : acquired_(false) {
  size_ = size;
}

AcceleratedSurfaceContainerLinux::~AcceleratedSurfaceContainerLinux() {
  if (texture_id_) {
    ui::SharedResources* instance = ui::SharedResources::GetInstance();
    DCHECK(instance);
    scoped_ptr<gfx::ScopedMakeCurrent> bind(instance->GetScopedMakeCurrent());
    glDeleteTextures(1, &texture_id_);
  }

  if (image_transport_client_.get())
    image_transport_client_->Release();
}

bool AcceleratedSurfaceContainerLinux::Initialize(uint64* surface_handle) {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
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
  flipped_ = image_transport_client_->Flipped();
  return true;
}

// Texture implementation
void AcceleratedSurfaceContainerLinux::Update() {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);
  scoped_ptr<gfx::ScopedMakeCurrent> bind(instance->GetScopedMakeCurrent());
  if (acquired_)
    image_transport_client_->Release();
  else
    acquired_ = true;
  image_transport_client_->Acquire();
}

TransportDIB::Handle AcceleratedSurfaceContainerLinux::Handle() const {
  return image_transport_client_->Handle();
}
