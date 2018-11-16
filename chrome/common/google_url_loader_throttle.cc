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
    bool force_safe_search,
    int32_t youtube_restrict,
    const std::string& allowed_domains_for_apps,
    const std::string& variation_ids_header)
    : is_off_the_record_(is_off_the_record),
      force_safe_search_(force_safe_search),
      youtube_restrict_(youtube_restrict),
      allowed_domains_for_apps_(allowed_domains_for_apps),
      variation_ids_header_(variation_ids_header) {}

GoogleURLLoaderThrottle::~GoogleURLLoaderThrottle() {}

void GoogleURLLoaderThrottle::DetachFromCurrentSequence() {}

void GoogleURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (!is_off_the_record_ &&
      variations::ShouldAppendVariationHeaders(request->url) &&
      !variation_ids_header_.empty()) {
    request->headers.SetHeaderIfMissing(variations::kClientDataHeader,
                                        variation_ids_header_);
  }

  if (force_safe_search_) {
    GURL new_url;
    safe_search_util::ForceGoogleSafeSearch(request->url, &new_url);
    if (!new_url.is_empty())
      request->url = new_url;
  }

  static_assert(safe_search_util::YOUTUBE_RESTRICT_OFF == 0,
                "OFF must be first");
  if (youtube_restrict_ > safe_search_util::YOUTUBE_RESTRICT_OFF &&
      youtube_restrict_ < safe_search_util::YOUTUBE_RESTRICT_COUNT) {
    safe_search_util::ForceYouTubeRestrict(
        request->url, &request->headers,
        static_cast<safe_search_util::YouTubeRestrictMode>(youtube_restrict_));
  }

  if (!allowed_domains_for_apps_.empty() &&
      request->url.DomainIs("google.com")) {
    request->headers.SetHeader(safe_search_util::kGoogleAppsAllowedDomains,
                               allowed_domains_for_apps_);
  }
}

void GoogleURLLoaderThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& /* response_head */,
    bool* /* defer */,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers) {
  if (!variations::ShouldAppendVariationHeaders(redirect_info.new_url))
    to_be_removed_headers->push_back(variations::kClientDataHeader);

  if (youtube_restrict_ > safe_search_util::YOUTUBE_RESTRICT_OFF &&
      youtube_restrict_ < safe_search_util::YOUTUBE_RESTRICT_COUNT) {
    safe_search_util::ForceYouTubeRestrict(
        redirect_info.new_url, modified_headers,
        static_cast<safe_search_util::YouTubeRestrictMode>(youtube_restrict_));
  }

  if (!allowed_domains_for_apps_.empty() &&
      redirect_info.new_url.DomainIs("google.com")) {
    modified_headers->SetHeader(safe_search_util::kGoogleAppsAllowedDomains,
                                allowed_domains_for_apps_);
  }
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
