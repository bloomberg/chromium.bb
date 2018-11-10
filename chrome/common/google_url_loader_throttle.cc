// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/google_url_loader_throttle.h"

#include "chrome/common/net/safe_search_util.h"
#include "components/variations/net/variations_http_headers.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_urls.h"
#endif

GoogleURLLoaderThrottle::GoogleURLLoaderThrottle(
    bool is_off_the_record,
    chrome::mojom::DynamicParams dynamic_params)
    : is_off_the_record_(is_off_the_record),
      dynamic_params_(std::move(dynamic_params)) {}

GoogleURLLoaderThrottle::~GoogleURLLoaderThrottle() {}

void GoogleURLLoaderThrottle::DetachFromCurrentSequence() {}

void GoogleURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (!is_off_the_record_ &&
      variations::ShouldAppendVariationHeaders(request->url) &&
      !dynamic_params_.variation_ids_header.empty()) {
    request->headers.SetHeaderIfMissing(variations::kClientDataHeader,
                                        dynamic_params_.variation_ids_header);
  }

  if (dynamic_params_.force_safe_search) {
    GURL new_url;
    safe_search_util::ForceGoogleSafeSearch(request->url, &new_url);
    if (!new_url.is_empty())
      request->url = new_url;
  }

  static_assert(safe_search_util::YOUTUBE_RESTRICT_OFF == 0,
                "OFF must be first");
  if (dynamic_params_.youtube_restrict >
          safe_search_util::YOUTUBE_RESTRICT_OFF &&
      dynamic_params_.youtube_restrict <
          safe_search_util::YOUTUBE_RESTRICT_COUNT) {
    safe_search_util::ForceYouTubeRestrict(
        request->url, &request->headers,
        static_cast<safe_search_util::YouTubeRestrictMode>(
            dynamic_params_.youtube_restrict));
  }

  if (!dynamic_params_.allowed_domains_for_apps.empty() &&
      request->url.DomainIs("google.com")) {
    request->headers.SetHeader(safe_search_util::kGoogleAppsAllowedDomains,
                               dynamic_params_.allowed_domains_for_apps);
  }
}

void GoogleURLLoaderThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& /* response_head */,
    bool* /* defer */,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* /* modified_headers */) {
  if (!variations::ShouldAppendVariationHeaders(redirect_info.new_url))
    to_be_removed_headers->push_back(variations::kClientDataHeader);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void GoogleURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::ResourceResponseHead* response_head,
    bool* defer) {
  // Built-in additional protection for the chrome web store origin.
  GURL webstore_url(extension_urls::GetWebstoreLaunchURL());
  if (response_url.SchemeIsHTTPOrHTTPS() &&
      response_url.DomainIs(webstore_url.host_piece())) {
    if (response_head && response_head->headers &&
        !response_head->headers->HasHeaderValue("x-frame-options", "deny") &&
        !response_head->headers->HasHeaderValue("x-frame-options",
                                                "sameorigin")) {
      response_head->headers->RemoveHeader("x-frame-options");
      response_head->headers->AddHeader("x-frame-options: sameorigin");
    }
  }
}
#endif
