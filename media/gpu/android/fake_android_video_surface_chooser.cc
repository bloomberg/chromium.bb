// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/fake_android_video_surface_chooser.h"

namespace media {

FakeSurfaceChooser::FakeSurfaceChooser() = default;
FakeSurfaceChooser::~FakeSurfaceChooser() = default;

void FakeSurfaceChooser::SetClientCallbacks(
    UseOverlayCB use_overlay_cb,
    UseSurfaceTextureCB use_surface_texture_cb) {
  MockSetClientCallbacks();
  use_overlay_cb_ = std::move(use_overlay_cb);
  use_surface_texture_cb_ = std::move(use_surface_texture_cb);
}

void FakeSurfaceChooser::UpdateState(
    base::Optional<AndroidOverlayFactoryCB> factory,
    const State& new_state) {
  MockUpdateState();
  if (factory) {
    factory_ = std::move(*factory);
    MockReplaceOverlayFactory(!factory_.is_null());
  }
  current_state_ = new_state;
}

void FakeSurfaceChooser::ProvideSurfaceTexture() {
  use_surface_texture_cb_.Run();
}

void FakeSurfaceChooser::ProvideOverlay(
    std::unique_ptr<AndroidOverlay> overlay) {
  use_overlay_cb_.Run(std::move(overlay));
}

}  // namespace media
