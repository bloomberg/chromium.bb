// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/safe_browsing_resource_throttle.h"

#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/features.h"
#include "content/public/browser/resource_request_info.h"
#include "net/http/http_request_headers.h"

content::ResourceThrottle* MaybeCreateSafeBrowsingResourceThrottle(
    net::URLRequest* request,
    content::ResourceType resource_type,
    safe_browsing::SafeBrowsingService* sb_service) {
  if (!sb_service->database_manager()->IsSupported())
    return nullptr;

  if (base::FeatureList::IsEnabled(safe_browsing::kParallelUrlCheck)) {
    return new SafeBrowsingParallelResourceThrottle(request, resource_type,
                                                    sb_service);
  }
  return new SafeBrowsingResourceThrottle(request, resource_type, sb_service);
}

SafeBrowsingResourceThrottle::SafeBrowsingResourceThrottle(
    const net::URLRequest* request,
    content::ResourceType resource_type,
    safe_browsing::SafeBrowsingService* sb_service)
    : safe_browsing::BaseResourceThrottle(
          request,
          resource_type,
          safe_browsing::CreateSBThreatTypeSet(
              {safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
               safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
               safe_browsing::SB_THREAT_TYPE_URL_UNWANTED}),
          sb_service->database_manager(),
          sb_service->ui_manager()),
      url_checker_delegate_(new safe_browsing::UrlCheckerDelegateImpl(
          sb_service->database_manager(),
          sb_service->ui_manager())) {}

SafeBrowsingResourceThrottle::~SafeBrowsingResourceThrottle() = default;

const char* SafeBrowsingResourceThrottle::GetNameForLogging() const {
  return "SafeBrowsingResourceThrottle";
}

void SafeBrowsingResourceThrottle::MaybeDestroyPrerenderContents(
    const content::ResourceRequestInfo* info) {
  url_checker_delegate_->MaybeDestroyPrerenderContents(
      info->GetWebContentsGetterForRequest());
}

void SafeBrowsingResourceThrottle::StartDisplayingBlockingPageHelper(
    security_interstitials::UnsafeResource resource) {
  // On Chrome only the first argument is used.
  url_checker_delegate_->StartDisplayingBlockingPageHelper(
      resource, std::string(), net::HttpRequestHeaders(),
      false /* is_main_frame */, false /* has_user_gesture */);
}

SafeBrowsingParallelResourceThrottle::SafeBrowsingParallelResourceThrottle(
    const net::URLRequest* request,
    content::ResourceType resource_type,
    safe_browsing::SafeBrowsingService* sb_service)
    : safe_browsing::BaseParallelResourceThrottle(
          request,
          resource_type,
          new safe_browsing::UrlCheckerDelegateImpl(
              sb_service->database_manager(),
              sb_service->ui_manager())) {}

SafeBrowsingParallelResourceThrottle::~SafeBrowsingParallelResourceThrottle() =
    default;

const char* SafeBrowsingParallelResourceThrottle::GetNameForLogging() const {
  return "SafeBrowsingParallelResourceThrottle";
}
