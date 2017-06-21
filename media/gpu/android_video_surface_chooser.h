// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_VIDEO_SURFACE_CHOOSER_H_
#define MEDIA_GPU_ANDROID_VIDEO_SURFACE_CHOOSER_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "media/base/android/android_overlay.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// Manage details of which surface to use for video playback.
class MEDIA_GPU_EXPORT AndroidVideoSurfaceChooser {
 public:
  // Input state used for choosing the surface type.
  struct State {
    bool is_fullscreen = false;

    // Does playback require a secure surface?
    bool is_secure = false;

    // TODO(liberato): add compositor feedback.
  };

  // Notify the client that |overlay| is ready for use.  The client may get
  // the surface immediately.
  using UseOverlayCB =
      base::RepeatingCallback<void(std::unique_ptr<AndroidOverlay> overlay)>;

  // Notify the client that the most recently provided overlay should be
  // discarded.  The overlay is still valid, but we recommend against
  // using it soon, in favor of a SurfaceTexture.
  using UseSurfaceTextureCB = base::RepeatingCallback<void(void)>;

  AndroidVideoSurfaceChooser() {}
  virtual ~AndroidVideoSurfaceChooser() {}

  // Notify us that our client is ready for overlays.  We will send it a
  // callback telling it whether to start with a SurfaceTexture or overlay,
  // either synchronously or post one very soon.  |initial_factory| can be
  // an empty callback to indicate "no factory".
  virtual void Initialize(UseOverlayCB use_overlay_cb,
                          UseSurfaceTextureCB use_surface_texture_cb,
                          AndroidOverlayFactoryCB initial_factory,
                          const State& initial_state) = 0;

  // Notify us that a new factory has arrived and / or other state is available.
  // |*new_factory| may be an is_null() callback to indicate that the most
  // recent factory has been revoked.  |!new_factory.has_value()| results in no
  // change to the factory.
  virtual void UpdateState(base::Optional<AndroidOverlayFactoryCB> new_factory,
                           const State& new_state) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidVideoSurfaceChooser);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_VIDEO_SURFACE_CHOOSER_H_
