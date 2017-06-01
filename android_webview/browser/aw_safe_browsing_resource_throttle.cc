// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_safe_browsing_resource_throttle.h"

#include "android_webview/browser/aw_safe_browsing_ui_manager.h"
#include "base/macros.h"
#include "components/safe_browsing/base_resource_throttle.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/common/resource_type.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

namespace android_webview {

// static
const void* AwSafeBrowsingResourceThrottle::kUserDataKey =
    static_cast<void*>(&AwSafeBrowsingResourceThrottle::kUserDataKey);

// static
AwSafeBrowsingResourceThrottle* AwSafeBrowsingResourceThrottle::MaybeCreate(
    net::URLRequest* request,
    content::ResourceType resource_type,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<AwSafeBrowsingUIManager> ui_manager) {
  if (database_manager->IsSupported()) {
    return new AwSafeBrowsingResourceThrottle(request, resource_type,
                                              database_manager, ui_manager);
  }
  return nullptr;
}

AwSafeBrowsingResourceThrottle::AwSafeBrowsingResourceThrottle(
    net::URLRequest* request,
    content::ResourceType resource_type,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<AwSafeBrowsingUIManager> ui_manager)
    : safe_browsing::BaseResourceThrottle(request,
                                          resource_type,
                                          database_manager,
                                          ui_manager),
      request_(request) {}

AwSafeBrowsingResourceThrottle::~AwSafeBrowsingResourceThrottle() {}

void AwSafeBrowsingResourceThrottle::CancelResourceLoad() {
  request_->SetUserData(kUserDataKey,
                        base::MakeUnique<base::SupportsUserData::Data>());
  Cancel();
}

void AwSafeBrowsingResourceThrottle::OnCheckBrowseUrlResult(
    const GURL& url,
    SBThreatType threat_type,
    const ThreatMetadata& metadata) {
  if (threat_type != safe_browsing::SB_THREAT_TYPE_URL_PHISHING &&
      threat_type != safe_browsing::SB_THREAT_TYPE_URL_MALWARE) {
    // If we don't recognize the threat type, just mark it as safe
    threat_type = safe_browsing::SB_THREAT_TYPE_SAFE;
  }

  BaseResourceThrottle::OnCheckBrowseUrlResult(url, threat_type, metadata);
}

}  // namespace android_webview
