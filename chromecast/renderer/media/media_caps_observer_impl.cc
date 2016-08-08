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
  ::media::SetHdmiSinkCodecs(codec_support_bitmask);
}

}  // namespace media
}  // namespace chromecast
