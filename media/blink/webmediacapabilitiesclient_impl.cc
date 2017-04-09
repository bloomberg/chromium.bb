// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediacapabilitiesclient_impl.h"

#include "third_party/WebKit/public/platform/modules/media_capabilities/WebMediaDecodingAbility.h"

namespace media {

WebMediaCapabilitiesClientImpl::WebMediaCapabilitiesClientImpl() = default;

WebMediaCapabilitiesClientImpl::~WebMediaCapabilitiesClientImpl() = default;

void WebMediaCapabilitiesClientImpl::Query(
    const blink::WebMediaConfiguration& configuration,
    std::unique_ptr<blink::WebMediaCapabilitiesQueryCallbacks> callbacks) {
  // TODO(chcunningham, mlamouri): this is a dummy implementation that returns
  // true for all the fields.
  std::unique_ptr<blink::WebMediaDecodingAbility> ability(
      new blink::WebMediaDecodingAbility());
  ability->supported = true;
  ability->smooth = true;
  ability->power_efficient = true;
  callbacks->OnSuccess(std::move(ability));
}

}  // namespace media
