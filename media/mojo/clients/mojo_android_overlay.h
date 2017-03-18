// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOJO_ANDROID_OVERLAY_H_
#define MEDIA_BASE_MOJO_ANDROID_OVERLAY_H_

#include "base/macros.h"
#include "media/base/android/android_overlay.h"
#include "media/mojo/interfaces/android_overlay.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace service_manager {
namespace mojom {
class InterfaceProvider;
}
}

namespace media {

// AndroidOverlay implementation via mojo.
class MojoAndroidOverlay : public AndroidOverlay,
                           public mojom::AndroidOverlayClient {
 public:
  MojoAndroidOverlay(
      service_manager::mojom::InterfaceProvider* interface_provider,
      const AndroidOverlay::Config& config);

  ~MojoAndroidOverlay() override;

  // AndroidOverlay
  void ScheduleLayout(const gfx::Rect& rect) override;
  const base::android::JavaRef<jobject>& GetJavaSurface() const override;

  // mojom::AndroidOverlayClient
  void OnSurfaceReady(uint64_t surface_key) override;
  void OnDestroyed() override;

 private:
  service_manager::mojom::InterfaceProvider* interface_provider_;
  AndroidOverlay::Config config_;
  mojom::AndroidOverlayProviderPtr provider_ptr_;
  mojom::AndroidOverlayPtr overlay_ptr_;
  std::unique_ptr<mojo::Binding<mojom::AndroidOverlayClient>> binding_;
  gl::ScopedJavaSurface surface_;

  // Have we received OnSurfaceReady yet?
  bool received_surface_ = false;

  DISALLOW_COPY_AND_ASSIGN(MojoAndroidOverlay);
};

}  // namespace media

#endif  // MEDIA_BASE_MOJO_ANDROID_OVERLAY_H_
