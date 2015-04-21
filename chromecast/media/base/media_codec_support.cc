// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_codec_support.h"

// TODO(gunsch/servolk): delete this file once a solution exists upstream.

namespace net {

bool DefaultIsCodecSupported(const std::string&) {
  return true;
}

}  // namespace net

