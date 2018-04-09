// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/ad_delay_renderer_metadata_provider.h"

#include "third_party/blink/public/platform/web_url_request.h"

namespace subresource_filter {

AdDelayRendererMetadataProvider::AdDelayRendererMetadataProvider(
    const blink::WebURLRequest& request)
    : is_ad_request_(request.IsAdResource()) {}

AdDelayRendererMetadataProvider::~AdDelayRendererMetadataProvider() = default;

// TODO(csharrison): Update |is_ad_request_| across redirects.
bool AdDelayRendererMetadataProvider::IsAdRequest() {
  return is_ad_request_;
}

}  // namespace subresource_filter
