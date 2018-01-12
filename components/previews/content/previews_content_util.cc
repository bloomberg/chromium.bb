// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_content_util.h"

#include "base/metrics/histogram_macros.h"
#include "components/previews/core/previews_user_data.h"
#include "net/url_request/url_request.h"

namespace previews {

bool HasEnabledPreviews(content::PreviewsState previews_state) {
  return previews_state != content::PREVIEWS_UNSPECIFIED &&
         !(previews_state & content::PREVIEWS_OFF) &&
         !(previews_state & content::PREVIEWS_NO_TRANSFORM);
}

content::PreviewsState DetermineEnabledClientPreviewsState(
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

content::PreviewsState DetermineCommittedClientPreviewsState(
    const net::URLRequest& url_request,
    content::PreviewsState previews_state) {
  bool is_https = url_request.url().SchemeIs(url::kHttpsScheme);

  // If a server preview is set, retain only the bits determined for the server.
  // |previews_state| must already have been updated for server previews from
  // the main frame response headers (so if they are set here, then they are
  // the specify the committed preview). Note: for Server LoFi we keep the
  // Client LoFi bit on so that it is applied to both HTTP and HTTPS images.
  if (previews_state &
      (content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON)) {
    return previews_state & (content::SERVER_LITE_PAGE_ON |
                             content::SERVER_LOFI_ON | content::CLIENT_LOFI_ON);
  }

  previews::PreviewsUserData* previews_user_data =
      previews::PreviewsUserData::GetData(url_request);
  if (previews_user_data &&
      previews_user_data->cache_control_no_transform_directive()) {
    if (HasEnabledPreviews(previews_state)) {
      UMA_HISTOGRAM_ENUMERATION(
          "Previews.CacheControlNoTransform.BlockedPreview",
          GetMainFramePreviewsType(previews_state),
          previews::PreviewsType::LAST);
    }
    return content::PREVIEWS_OFF;
  }

  // Make priority decision among allow client preview types that can be decided
  // at Commit time.
  if (previews_state & content::NOSCRIPT_ON) {
    if (is_https) {
      return content::NOSCRIPT_ON;
    } else {
      previews_state &= ~(content::NOSCRIPT_ON);
    }
  }
  if (previews_state & content::CLIENT_LOFI_ON) {
    return content::CLIENT_LOFI_ON;
  }

  if (!previews_state) {
    return content::PREVIEWS_OFF;
  }

  NOTREACHED() << previews_state;
  return previews_state;
}

previews::PreviewsType GetMainFramePreviewsType(
    content::PreviewsState previews_state) {
  if (previews_state & content::SERVER_LITE_PAGE_ON) {
    return previews::PreviewsType::LITE_PAGE;
  } else if (previews_state & content::SERVER_LOFI_ON) {
    return previews::PreviewsType::LOFI;
  } else if (previews_state & content::NOSCRIPT_ON) {
    return previews::PreviewsType::NOSCRIPT;
  } else if (previews_state & content::CLIENT_LOFI_ON) {
    return previews::PreviewsType::LOFI;
  }
  return previews::PreviewsType::NONE;
}

}  // namespace previews
