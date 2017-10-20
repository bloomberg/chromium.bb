// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_safe_browsing_resource_throttle.h"

#include <memory>

#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_safe_browsing_whitelist_manager.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/features.h"
#include "net/url_request/url_request.h"

namespace android_webview {
namespace {

// This is used as a user data key for net::URLRequest. Setting it indicates
// that the request is cancelled because of SafeBrowsing. It could be used, for
// example, to decide whether to override the error code with a
// SafeBrowsing-specific error code.
const void* const kCancelledBySafeBrowsingUserDataKey =
    &kCancelledBySafeBrowsingUserDataKey;

void SetCancelledBySafeBrowsing(net::URLRequest* request) {
  request->SetUserData(kCancelledBySafeBrowsingUserDataKey,
                       std::make_unique<base::SupportsUserData::Data>());
}

}  // namespace

content::ResourceThrottle* MaybeCreateAwSafeBrowsingResourceThrottle(
    net::URLRequest* request,
    content::ResourceType resource_type,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
    AwSafeBrowsingWhitelistManager* whitelist_manager) {
  if (!database_manager->IsSupported())
    return nullptr;

  if (base::FeatureList::IsEnabled(safe_browsing::kParallelUrlCheck)) {
    return new AwSafeBrowsingParallelResourceThrottle(
        request, resource_type, std::move(database_manager),
        std::move(ui_manager), whitelist_manager);
  }

  return new AwSafeBrowsingResourceThrottle(
      request, resource_type, std::move(database_manager),
      std::move(ui_manager), whitelist_manager);
}

bool IsCancelledBySafeBrowsing(const net::URLRequest* request) {
  return request->GetUserData(kCancelledBySafeBrowsingUserDataKey) != nullptr;
}

AwSafeBrowsingResourceThrottle::AwSafeBrowsingResourceThrottle(
    net::URLRequest* request,
    content::ResourceType resource_type,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
    AwSafeBrowsingWhitelistManager* whitelist_manager)
    : safe_browsing::BaseResourceThrottle(
          request,
          resource_type,
          safe_browsing::CreateSBThreatTypeSet(
              {safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
               safe_browsing::SB_THREAT_TYPE_URL_PHISHING}),
          database_manager,
          ui_manager),
      request_(request),
      url_checker_delegate_(
          new AwUrlCheckerDelegateImpl(std::move(database_manager),
                                       std::move(ui_manager),
                                       whitelist_manager)) {}

AwSafeBrowsingResourceThrottle::~AwSafeBrowsingResourceThrottle() = default;

bool AwSafeBrowsingResourceThrottle::CheckUrl(const GURL& gurl) {
  if (url_checker_delegate_->IsUrlWhitelisted(gurl)) {
    return true;
  }
  return BaseResourceThrottle::CheckUrl(gurl);
}

void AwSafeBrowsingResourceThrottle::StartDisplayingBlockingPageHelper(
    security_interstitials::UnsafeResource resource) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  bool is_main_frame =
      info && info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME;
  bool has_user_gesture = info && info->HasUserGesture();

  net::HttpRequestHeaders headers;
  if (!request_->GetFullRequestHeaders(&headers))
    headers = request_->extra_request_headers();

  url_checker_delegate_->StartDisplayingBlockingPageHelper(
      resource, request_->method(), headers, is_main_frame, has_user_gesture);
}

void AwSafeBrowsingResourceThrottle::CancelResourceLoad() {
  SetCancelledBySafeBrowsing(request_);
  BaseResourceThrottle::CancelResourceLoad();
}

AwSafeBrowsingParallelResourceThrottle::AwSafeBrowsingParallelResourceThrottle(
    net::URLRequest* request,
    content::ResourceType resource_type,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
    AwSafeBrowsingWhitelistManager* whitelist_manager)
    : safe_browsing::BaseParallelResourceThrottle(
          request,
          resource_type,
          new AwUrlCheckerDelegateImpl(std::move(database_manager),
                                       std::move(ui_manager),
                                       whitelist_manager)),
      request_(request) {}

AwSafeBrowsingParallelResourceThrottle::
    ~AwSafeBrowsingParallelResourceThrottle() = default;

void AwSafeBrowsingParallelResourceThrottle::CancelResourceLoad() {
  SetCancelledBySafeBrowsing(request_);
  BaseParallelResourceThrottle::CancelResourceLoad();
}

}  // namespace android_webview
