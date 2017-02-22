// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/frame_owner_properties.h"

#include <algorithm>
#include <iterator>

#include "third_party/WebKit/public/platform/WebFeaturePolicy.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission.mojom.h"

namespace content {

FrameOwnerProperties ConvertWebFrameOwnerPropertiesToFrameOwnerProperties(
    const blink::WebFrameOwnerProperties& web_frame_owner_properties) {
  FrameOwnerProperties result;

  result.name = web_frame_owner_properties.name.utf8();
  result.scrolling_mode = web_frame_owner_properties.scrollingMode;
  result.margin_width = web_frame_owner_properties.marginWidth;
  result.margin_height = web_frame_owner_properties.marginHeight;
  result.allow_fullscreen = web_frame_owner_properties.allowFullscreen;
  result.allow_payment_request = web_frame_owner_properties.allowPaymentRequest;
  result.required_csp = web_frame_owner_properties.requiredCsp.utf8();
  std::copy(web_frame_owner_properties.delegatedPermissions.begin(),
            web_frame_owner_properties.delegatedPermissions.end(),
            std::back_inserter(result.delegated_permissions));
  std::copy(web_frame_owner_properties.allowedFeatures.begin(),
            web_frame_owner_properties.allowedFeatures.end(),
            std::back_inserter(result.allowed_features));

  return result;
}

blink::WebFrameOwnerProperties
ConvertFrameOwnerPropertiesToWebFrameOwnerProperties(
    const FrameOwnerProperties& frame_owner_properties) {
  blink::WebFrameOwnerProperties result;

  result.name = blink::WebString::fromUTF8(frame_owner_properties.name);
  result.scrollingMode = frame_owner_properties.scrolling_mode;
  result.marginWidth = frame_owner_properties.margin_width;
  result.marginHeight = frame_owner_properties.margin_height;
  result.allowFullscreen = frame_owner_properties.allow_fullscreen;
  result.allowPaymentRequest = frame_owner_properties.allow_payment_request;
  result.requiredCsp =
      blink::WebString::fromUTF8(frame_owner_properties.required_csp);
  result.delegatedPermissions = blink::WebVector<blink::mojom::PermissionName>(
      frame_owner_properties.delegated_permissions);
  result.allowedFeatures = blink::WebVector<blink::WebFeaturePolicyFeature>(
      frame_owner_properties.allowed_features);

  return result;
}

}  // namespace content
