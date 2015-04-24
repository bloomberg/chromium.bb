// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_codec_support.h"

#include "base/bind.h"

namespace chromecast {
namespace media {

// This is a default implementation of GetIsCodecSupportedOnChromecastCB that is
// going to be used for brandings other than 'Chrome'. Most platforms will want
// to use their own custom implementations of the IsCodecSupportedCB callback.
net::IsCodecSupportedCB GetIsCodecSupportedOnChromecastCB() {
  return base::Bind(&net::DefaultIsCodecSupported);
}

}  // namespace media
}  // namespace chromecast

