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
  AndroidVideoSurfaceChooserImpl();
  ~AndroidVideoSurfaceChooserImpl() override;

  // AndroidVideoSurfaceChooser
  void Initialize(UseOverlayCB use_overlay_cb,
                  UseSurfaceTextureCB use_surface_texture_cb,
                  AndroidOverlayFactoryCB initial_factory) override;
  void ReplaceOverlayFactory(AndroidOverlayFactoryCB factory) override;

 private:
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

  // If true, we owe the client notification about whether to use an overlay
  // or a surface texture.
  bool client_notification_pending_ = false;

  AndroidOverlayFactoryCB overlay_factory_;

  base::WeakPtrFactory<AndroidVideoSurfaceChooserImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidVideoSurfaceChooserImpl);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_VIDEO_SURFACE_CHOOSER_IMPL_H_
