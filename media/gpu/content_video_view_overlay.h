// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CONTENT_VIDEO_VIEW_OVERLAY_H_
#define MEDIA_GPU_CONTENT_VIDEO_VIEW_OVERLAY_H_

#include "base/memory/weak_ptr.h"
#include "media/base/android/android_overlay.h"
#include "media/gpu/avda_codec_allocator.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace media {

// TODO(liberato): most of Allocate/DeallocateSurface can be moved here, out
// of AVDACodecAllocator.
class ContentVideoViewOverlay : public AndroidOverlay,
                                public AVDASurfaceAllocatorClient {
 public:
  // |config| is ignored except for callbacks.  Callbacks will not be called
  // before this returns.
  ContentVideoViewOverlay(AVDACodecAllocator* codec_allocator,
                          int surface_id,
                          const AndroidOverlay::Config& config);
  ~ContentVideoViewOverlay() override;

  // ContentVideoView ignores this, unfortunately.
  void ScheduleLayout(const gfx::Rect& rect) override;
  const base::android::JavaRef<jobject>& GetJavaSurface() const override;

  // AVDASurfaceAllocatorClient
  void OnSurfaceAvailable(bool success) override;
  void OnSurfaceDestroyed() override;

 private:
  AVDACodecAllocator* codec_allocator_;
  int surface_id_;
  AndroidOverlay::Config config_;
  gl::ScopedJavaSurface surface_;

  base::WeakPtrFactory<ContentVideoViewOverlay> weak_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CONTENT_VIDEO_VIEW_OVERLAY_H_
