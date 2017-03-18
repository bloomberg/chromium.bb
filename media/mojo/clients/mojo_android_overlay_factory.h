// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOJO_ANDROID_OVERLAY_FACTORY_H_
#define MEDIA_BASE_MOJO_ANDROID_OVERLAY_FACTORY_H_

#include "base/macros.h"
#include "media/base/android/android_overlay_factory.h"
#include "media/mojo/interfaces/android_overlay.mojom.h"

namespace service_manager {
namespace mojom {
class InterfaceProvider;
}
}

namespace media {

// AndroidOverlayFactory implementation for mojo-based overlays.
class MojoAndroidOverlayFactory : public AndroidOverlayFactory {
 public:
  MojoAndroidOverlayFactory(
      service_manager::mojom::InterfaceProvider* interface_provider);
  ~MojoAndroidOverlayFactory() override;

  std::unique_ptr<AndroidOverlay> CreateOverlay(
      const AndroidOverlay::Config& config) override;

 private:
  service_manager::mojom::InterfaceProvider* const interface_provider_;

  DISALLOW_COPY_AND_ASSIGN(MojoAndroidOverlayFactory);
};

}  // namespace media

#endif  // MEDIA_BASE_MOJO_ANDROID_OVERLAY_H_
