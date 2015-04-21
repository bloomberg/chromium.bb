// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_MEDIA_CODEC_SUPPORT_H_
#define CHROMECAST_MEDIA_BASE_MEDIA_CODEC_SUPPORT_H_

#include <string>

#include "base/callback.h"
#include "net/base/mime_util.h"

// TODO(gunsch/servolk): remove when this definition exists upstream.

namespace net {
typedef base::Callback<bool(const std::string&)> IsCodecSupportedCB;
bool DefaultIsCodecSupported(const std::string&);
}

namespace chromecast {
namespace media {

// This function should return a callback capable of deciding whether a given
// codec (passed in as a string representation of the codec id conforming to
// RFC 6381) is supported or not. The implementation of this function is
// expected to be provided somewhere in the vendor platform-specific libraries
// that will get linked with cast_shell_common target.
net::IsCodecSupportedCB GetIsCodecSupportedOnChromecastCB();

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_MEDIA_CODEC_SUPPORT_H_

