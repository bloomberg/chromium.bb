// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_OVERLAY_CONFIG_H_
#define MEDIA_BASE_ANDROID_OVERLAY_CONFIG_H_

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/media_export.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

class AndroidOverlay;

// Configuration used to create an overlay.
struct MEDIA_EXPORT AndroidOverlayConfig {
 public:
  // Called when the overlay is ready for use, via |GetJavaSurface()|.
  using ReadyCB = base::OnceCallback<void(AndroidOverlay*)>;

  // Called when overlay has failed before |ReadyCB| is called.  Will not be
  // called after ReadyCB.  It will be the last callback for the overlay.
  using FailedCB = base::OnceCallback<void(AndroidOverlay*)>;

  // Called when the overlay has been destroyed.  This will not be called unless
  // ReadyCB has been called.  It will be the last callback for the overlay.
  using DestroyedCB = base::OnceCallback<void(AndroidOverlay*)>;

  // Configuration used to create an overlay.
  AndroidOverlayConfig();
  AndroidOverlayConfig(AndroidOverlayConfig&&);
  ~AndroidOverlayConfig();

  // Initial rectangle for the overlay.  May be changed via ScheduleLayout().
  gfx::Rect rect;

  // Require a secure overlay?
  bool secure = false;

  // Convenient helpers since the syntax is weird.
  void is_ready(AndroidOverlay* overlay) { std::move(ready_cb).Run(overlay); }
  void is_failed(AndroidOverlay* overlay) { std::move(failed_cb).Run(overlay); }

  ReadyCB ready_cb;
  FailedCB failed_cb;

  DISALLOW_COPY(AndroidOverlayConfig);
};

// Common factory type.
using AndroidOverlayFactoryCB =
    base::RepeatingCallback<std::unique_ptr<AndroidOverlay>(
        AndroidOverlayConfig)>;

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_OVERLAY_CONFIG_H_
