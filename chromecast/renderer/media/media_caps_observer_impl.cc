// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/media/media_caps_observer_impl.h"

#include "chromecast/media/base/media_caps.h"

namespace chromecast {
namespace media {

MediaCapsObserverImpl::MediaCapsObserverImpl(mojom::MediaCapsObserverPtr* proxy)
    : binding_(this, proxy) {}

MediaCapsObserverImpl::~MediaCapsObserverImpl() = default;

void MediaCapsObserverImpl::SupportedHdmiSinkCodecsChanged(
    uint32_t codec_support_bitmask) {
  MediaCapabilities::SetHdmiSinkCodecs(codec_support_bitmask);
}

void MediaCapsObserverImpl::ScreenResolutionChanged(uint32_t width,
                                                    uint32_t height) {
  MediaCapabilities::ScreenResolutionChanged(gfx::Size(width, height));
}

void MediaCapsObserverImpl::ScreenInfoChanged(int32_t hdcp_version,
                                              int32_t supported_eotfs,
                                              int32_t dolby_vision_flags) {
  MediaCapabilities::ScreenInfoChanged(hdcp_version, supported_eotfs,
                                       dolby_vision_flags);
}

}  // namespace media
}  // namespace chromecast
