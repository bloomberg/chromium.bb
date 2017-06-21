// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_VIDEO_SURFACE_CHOOSER_IMPL_H_
#define MEDIA_GPU_ANDROID_VIDEO_SURFACE_CHOOSER_IMPL_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/android/android_overlay.h"
#include "media/gpu/android_video_surface_chooser.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// Implementation of AndroidVideoSurfaceChooser.
class MEDIA_GPU_EXPORT AndroidVideoSurfaceChooserImpl
    : public AndroidVideoSurfaceChooser {
 public:
  // |allow_dynamic| should be true if and only if we are allowed to change the
  // surface selection after the initial callback.
  AndroidVideoSurfaceChooserImpl(bool allow_dynamic);
  ~AndroidVideoSurfaceChooserImpl() override;

  // AndroidVideoSurfaceChooser
  void Initialize(UseOverlayCB use_overlay_cb,
                  UseSurfaceTextureCB use_surface_texture_cb,
                  AndroidOverlayFactoryCB initial_factory,
                  const State& initial_state) override;
  void UpdateState(base::Optional<AndroidOverlayFactoryCB> new_factory,
                   const State& new_state) override;

 private:
  // Choose whether we should be using a SurfaceTexture or overlay, and issue
  // the right callbacks if we're changing between them.  This should only be
  // called if |allow_dynamic_|.
  void Choose();

  // Start switching to SurfaceTexture or overlay, as needed.  These will call
  // the client callbacks if we're changing state, though those callbacks might
  // happen after this returns.
  void SwitchToSurfaceTexture();
  // If |overlay_| has an in-flight request, then this will do nothing.
  void SwitchToOverlay();

  // AndroidOverlay callbacks.
  void OnOverlayReady(AndroidOverlay*);
  void OnOverlayFailed(AndroidOverlay*);

  // Client callbacks.
  UseOverlayCB use_overlay_cb_;
  UseSurfaceTextureCB use_surface_texture_cb_;

  // Current overlay that we've constructed but haven't received ready / failed
  // callbacks yet.  Will be nullptr if we haven't constructed one, or if we
  // sent it to the client already once it became ready to use.
  std::unique_ptr<AndroidOverlay> overlay_;

  AndroidOverlayFactoryCB overlay_factory_;

  // Do we allow dynamic surface switches.  Usually this means "Are we running
  // on M or later?".
  bool allow_dynamic_;

  enum OverlayState {
    kUnknown,
    kUsingSurfaceTexture,
    kUsingOverlay,
  };

  // What was the last signal that the client received?
  OverlayState client_overlay_state_ = kUnknown;

  State current_state_;

  base::WeakPtrFactory<AndroidVideoSurfaceChooserImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidVideoSurfaceChooserImpl);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_VIDEO_SURFACE_CHOOSER_IMPL_H_
