// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_android_overlay_factory.h"

#include "media/mojo/clients/mojo_android_overlay.h"

namespace media {

MojoAndroidOverlayFactory::MojoAndroidOverlayFactory(
    service_manager::mojom::InterfaceProvider* interface_provider)
    : interface_provider_(interface_provider) {}

MojoAndroidOverlayFactory::~MojoAndroidOverlayFactory() {}

std::unique_ptr<AndroidOverlay> MojoAndroidOverlayFactory::CreateOverlay(
    const AndroidOverlay::Config& config) {
  return base::MakeUnique<MojoAndroidOverlay>(interface_provider_, config);
}

}  // namespace media
