// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GOOGLE_URL_LOADER_THROTTLE_H_
#define CHROME_COMMON_GOOGLE_URL_LOADER_THROTTLE_H_

#include "content/public/common/url_loader_throttle.h"

// This class changes requests for Google-specific features (e.g. adding &
// removing Varitaions headers, Safe Search & Restricted YouTube & restricting
// consumer accounts through group policy.
class GoogleURLLoaderThrottle
    : public content::URLLoaderThrottle,
      public base::SupportsWeakPtr<GoogleURLLoaderThrottle> {
 public:
  GoogleURLLoaderThrottle(bool is_off_the_record,
                          bool force_safe_search,
                          int32_t youtube_restrict,
                          const std::string& allowed_domains_for_apps,
                          const std::string& variation_ids_header);
  ~GoogleURLLoaderThrottle() override;

 private:
  // content::URLLoaderThrottle:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers) override;

  bool is_off_the_record_;
  bool force_safe_search_;
  int32_t youtube_restrict_;
  std::string allowed_domains_for_apps_;
  std::string variation_ids_header_;
};

#endif  // CHROME_COMMON_GOOGLE_URL_LOADER_THROTTLE_H_
