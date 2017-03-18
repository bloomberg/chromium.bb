// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_android_overlay.h"

#include "services/service_manager/public/cpp/connect.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"

namespace media {

MojoAndroidOverlay::MojoAndroidOverlay(
    service_manager::mojom::InterfaceProvider* interface_provider,
    const AndroidOverlay::Config& config)
    : interface_provider_(interface_provider), config_(config) {
  // Connect to the provider service.
  mojom::AndroidOverlayProviderPtr provider_ptr;
  service_manager::GetInterface<mojom::AndroidOverlayProvider>(
      interface_provider_, &provider_ptr_);

  // Fill in details of |config| into |mojo_config|.  Our caller could do this
  // too, but since we want to retain |config_| anyway, we do it here.
  mojom::AndroidOverlayConfigPtr mojo_config =
      mojom::AndroidOverlayConfig::New();
  mojo_config->routing_token = config_.routing_token;
  mojo_config->rect = config_.rect;

  mojom::AndroidOverlayClientPtr ptr;
  binding_ = base::MakeUnique<mojo::Binding<mojom::AndroidOverlayClient>>(
      this, mojo::MakeRequest(&ptr));

  provider_ptr_->CreateOverlay(mojo::MakeRequest(&overlay_ptr_), std::move(ptr),
                               std::move(mojo_config));
}

MojoAndroidOverlay::~MojoAndroidOverlay() {
  // Dropping |overlay_ptr_| will signal to the implementation that we're done
  // with the surface.  If a synchronous destroy is pending, then it can be
  // allowed to continue.
}

void MojoAndroidOverlay::ScheduleLayout(const gfx::Rect& rect) {
  // If we haven't gotten the surface yet, then ignore this.
  if (!received_surface_)
    return;

  overlay_ptr_->ScheduleLayout(rect);
}

const base::android::JavaRef<jobject>& MojoAndroidOverlay::GetJavaSurface()
    const {
  return surface_.j_surface();
}

void MojoAndroidOverlay::OnSurfaceReady(uint64_t surface_key) {
  // TODO(liberato): ask binder for the surface here, and fill in |surface_|.
  received_surface_ = true;
  config_.ready_cb.Run();
}

void MojoAndroidOverlay::OnDestroyed() {
  // Note that |overlay_ptr_| might not be bound yet, or we might not have ever
  // gotten a surface.  Regardless, the overlay cannot be used.

  if (!received_surface_)
    config_.failed_cb.Run();
  else
    config_.destroyed_cb.Run();

  // Note: we do not delete |overlay_ptr_| here.  Our client must delete us to
  // signal that we should do that, since it still might be in use.
}

}  // namespace media
