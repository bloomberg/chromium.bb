// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_ANDROID_OVERLAY_FACTORY_H_
#define MEDIA_BASE_ANDROID_ANDROID_OVERLAY_FACTORY_H_

#include "media/base/android/android_overlay.h"

namespace media {

// Base class for overlay factories.
class AndroidOverlayFactory {
 public:
  virtual ~AndroidOverlayFactory() {}

  virtual std::unique_ptr<AndroidOverlay> CreateOverlay(
      const AndroidOverlay::Config& config) = 0;

 protected:
  AndroidOverlayFactory() {}

  DISALLOW_COPY_AND_ASSIGN(AndroidOverlayFactory);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_ANDROID_OVERLAY_FACTORY_H_
