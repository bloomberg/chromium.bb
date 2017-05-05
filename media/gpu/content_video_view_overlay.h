// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CONTENT_VIDEO_VIEW_OVERLAY_H_
#define MEDIA_GPU_CONTENT_VIDEO_VIEW_OVERLAY_H_

#include "base/memory/weak_ptr.h"
#include "media/base/android/android_overlay.h"
#include "media/gpu/content_video_view_overlay_allocator.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace media {

class ContentVideoViewOverlay
    : public ContentVideoViewOverlayAllocator::Client {
 public:
  // |config| is ignored except for callbacks.  Callbacks will not be called
  // before this returns.
  ContentVideoViewOverlay(int surface_id, AndroidOverlayConfig config);
  ~ContentVideoViewOverlay() override;

  // AndroidOverlay (via ContentVideoViewOverlayAllocator::Client)
  // ContentVideoView ignores this, unfortunately.
  void ScheduleLayout(const gfx::Rect& rect) override;
  const base::android::JavaRef<jobject>& GetJavaSurface() const override;

  // ContentVideoViewOverlayAllocator::Client
  void OnSurfaceAvailable(bool success) override;
  void OnSurfaceDestroyed() override;
  int32_t GetSurfaceId() override;

 protected:
  // For tests.
  ContentVideoViewOverlay();

 private:
  int surface_id_;
  AndroidOverlayConfig config_;
  gl::ScopedJavaSurface surface_;

  base::WeakPtrFactory<ContentVideoViewOverlay> weak_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CONTENT_VIDEO_VIEW_OVERLAY_H_
