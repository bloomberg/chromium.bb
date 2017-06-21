// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MOCK_ANDROID_VIDEO_SURFACE_CHOOSER_H_
#define MEDIA_GPU_MOCK_ANDROID_VIDEO_SURFACE_CHOOSER_H_

#include "media/gpu/android_video_surface_chooser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// A fake surface chooser that lets tests choose the surface with
// ProvideOverlay() and ProvideSurfaceTexture().
class FakeSurfaceChooser : public AndroidVideoSurfaceChooser {
 public:
  FakeSurfaceChooser();
  ~FakeSurfaceChooser() override;

  // Mocks that are called by the fakes below.
  MOCK_METHOD0(MockInitialize, void());
  MOCK_METHOD0(MockUpdateState, void());

  // Called by UpdateState if the factory is changed.  It is called with true if
  // and only if the replacement factory isn't null.
  MOCK_METHOD1(MockReplaceOverlayFactory, void(bool));

  void Initialize(UseOverlayCB use_overlay_cb,
                  UseSurfaceTextureCB use_surface_texture_cb,
                  AndroidOverlayFactoryCB initial_factory,
                  const State& initial_state) override;
  void UpdateState(base::Optional<AndroidOverlayFactoryCB> factory,
                   const State& new_state) override;

  // Calls the corresponding callback to choose the surface.
  void ProvideOverlay(std::unique_ptr<AndroidOverlay> overlay);
  void ProvideSurfaceTexture();

  UseOverlayCB use_overlay_cb_;
  UseSurfaceTextureCB use_surface_texture_cb_;
  AndroidOverlayFactoryCB factory_;
  State current_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeSurfaceChooser);
};

}  // namespace media

#endif  // MEDIA_GPU_MOCK_ANDROID_VIDEO_SURFACE_CHOOSER_H_
