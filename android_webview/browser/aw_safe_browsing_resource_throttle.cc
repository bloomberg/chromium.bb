// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_safe_browsing_resource_throttle.h"

#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_safe_browsing_ui_manager.h"
#include "android_webview/browser/aw_safe_browsing_whitelist_manager.h"
#include "base/macros.h"
#include "components/safe_browsing/base_resource_throttle.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/resource_type.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

namespace android_webview {

// static
const void* const AwSafeBrowsingResourceThrottle::kUserDataKey =
    &AwSafeBrowsingResourceThrottle::kUserDataKey;

// static
AwSafeBrowsingResourceThrottle* AwSafeBrowsingResourceThrottle::MaybeCreate(
    net::URLRequest* request,
    content::ResourceType resource_type,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
    AwSafeBrowsingWhitelistManager* whitelist_manager) {
  if (database_manager->IsSupported()) {
    return new AwSafeBrowsingResourceThrottle(request, resource_type,
                                              database_manager, ui_manager,
                                              whitelist_manager);
  }
  return nullptr;
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

AwSafeBrowsingResourceThrottle::~AwSafeBrowsingResourceThrottle() {}

bool AwSafeBrowsingResourceThrottle::CheckUrl(const GURL& gurl) {
  if (url_checker_delegate_->IsUrlWhitelisted(gurl)) {
    return true;
  }
  return BaseResourceThrottle::CheckUrl(gurl);
}

void AwSafeBrowsingResourceThrottle::StartDisplayingBlockingPageHelper(
    security_interstitials::UnsafeResource resource) {
  resource.callback =
      base::Bind(&AwSafeBrowsingResourceThrottle::OnBlockingPageComplete,
                 AsWeakPtr(), resource.callback);

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

// static
void AwSafeBrowsingResourceThrottle::OnBlockingPageComplete(
    const base::WeakPtr<BaseResourceThrottle>& throttle,
    const base::Callback<void(bool)>& forward_callback,
    bool proceed) {
  if (throttle && !proceed) {
    AwSafeBrowsingResourceThrottle* aw_throttle =
        static_cast<AwSafeBrowsingResourceThrottle*>(throttle.get());
    aw_throttle->request_->SetUserData(
        kUserDataKey, base::MakeUnique<base::SupportsUserData::Data>());
  }

  forward_callback.Run(proceed);
}

}  // namespace android_webview
