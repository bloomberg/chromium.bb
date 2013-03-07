// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capturer.h"

namespace media {

scoped_refptr<SharedBuffer> ScreenCapturer::Delegate::CreateSharedBuffer(
    uint32 size) {
  return scoped_refptr<SharedBuffer>();
}

void ScreenCapturer::Delegate::ReleaseSharedBuffer(
    scoped_refptr<SharedBuffer> buffer) {
}

// static
scoped_ptr<ScreenCapturer> ScreenCapturer::Create() {
  return scoped_ptr<ScreenCapturer>();
}

// static
scoped_ptr<ScreenCapturer> ScreenCapturer::CreateWithXDamage(
    bool use_x_damage) {
  return scoped_ptr<ScreenCapturer>();
}

}  // namespace media
