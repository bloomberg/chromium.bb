// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/google_url_loader_throttle.h"

#include "chrome/common/net/safe_search_util.h"
#include "components/variations/net/variations_http_headers.h"

GoogleURLLoaderThrottle::GoogleURLLoaderThrottle(
    bool is_off_the_record,
    bool is_signed_in,
    bool force_safe_search,
    int32_t youtube_restrict,
    const std::string& allowed_domains_for_apps)
    : is_off_the_record_(is_off_the_record),
      is_signed_in_(is_signed_in),
      force_safe_search_(force_safe_search),
      youtube_restrict_(youtube_restrict),
      allowed_domains_for_apps_(allowed_domains_for_apps) {}

GoogleURLLoaderThrottle::~GoogleURLLoaderThrottle() {}

void GoogleURLLoaderThrottle::DetachFromCurrentSequence() {}

void GoogleURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  variations::AppendVariationHeaders(
      request->url,
      is_off_the_record_ ? variations::InIncognito::kYes
                         : variations::InIncognito::kNo,
      is_signed_in_ ? variations::SignedIn::kYes : variations::SignedIn::kNo,
      &request->headers);

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
    const network::ResourceResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_headers) {
  if (!variations::internal::ShouldAppendVariationHeaders(
          redirect_info.new_url)) {
    const char kClientData[] = "X-Client-Data";
    to_be_removed_headers->push_back(kClientData);
  }
}
