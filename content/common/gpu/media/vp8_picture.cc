// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/vp8_picture.h"

namespace content {

VP8Picture::VP8Picture() {
}

VP8Picture::~VP8Picture() {
}

V4L2VP8Picture* VP8Picture::AsV4L2VP8Picture() {
  return nullptr;
}

VaapiVP8Picture* VP8Picture::AsVaapiVP8Picture() {
  return nullptr;
}

}  // namespace content
