// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_content_util.h"

#include "net/url_request/url_request.h"

namespace previews {

content::PreviewsState DetermineClientPreviewsState(
    const net::URLRequest& url_request,
    previews::PreviewsDecider* previews_decider) {
  content::PreviewsState previews_state = content::PREVIEWS_UNSPECIFIED;

  if (!url_request.url().SchemeIsHTTPOrHTTPS()) {
    return previews_state;
  }

  // Check for client-side previews in precendence order.
  // Note: this for for the beginning of navigation so we should not
  // check for https here (since an http request may redirect to https).
  if (previews_decider->ShouldAllowPreviewAtECT(
          url_request, previews::PreviewsType::NOSCRIPT,
          previews::params::GetECTThresholdForPreview(
              previews::PreviewsType::NOSCRIPT),
          std::vector<std::string>())) {
    previews_state |= content::NOSCRIPT_ON;
    return previews_state;
  }

  if (previews::params::IsClientLoFiEnabled() &&
      previews_decider->ShouldAllowPreviewAtECT(
          url_request, previews::PreviewsType::LOFI,
          previews::params::EffectiveConnectionTypeThresholdForClientLoFi(),
          previews::params::GetBlackListedHostsForClientLoFiFieldTrial())) {
    previews_state |= content::CLIENT_LOFI_ON;
    return previews_state;
  }

  return previews_state;
}

previews::PreviewsType GetMainFramePreviewsType(
    content::PreviewsState previews_state) {
  if (previews_state & content::SERVER_LITE_PAGE_ON) {
    return previews::PreviewsType::LITE_PAGE;
  } else if (previews_state & content::NOSCRIPT_ON) {
    return previews::PreviewsType::NOSCRIPT;
  }
  return previews::PreviewsType::NONE;
}

}  // namespace previews
