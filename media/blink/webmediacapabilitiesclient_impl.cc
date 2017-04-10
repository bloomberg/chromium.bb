// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediacapabilitiesclient_impl.h"

#include "third_party/WebKit/public/platform/modules/media_capabilities/WebMediaCapabilitiesInfo.h"

namespace media {

WebMediaCapabilitiesClientImpl::WebMediaCapabilitiesClientImpl() = default;

WebMediaCapabilitiesClientImpl::~WebMediaCapabilitiesClientImpl() = default;

void WebMediaCapabilitiesClientImpl::DecodingInfo(
    const blink::WebMediaConfiguration& configuration,
    std::unique_ptr<blink::WebMediaCapabilitiesQueryCallbacks> callbacks) {
  // TODO(chcunningham, mlamouri): this is a dummy implementation that returns
  // true for all the fields.
  std::unique_ptr<blink::WebMediaCapabilitiesInfo> info(
      new blink::WebMediaCapabilitiesInfo());
  info->supported = true;
  info->smooth = true;
  info->power_efficient = true;
  callbacks->OnSuccess(std::move(info));
}

}  // namespace media
