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
                                          ui_manager) {}

AwSafeBrowsingResourceThrottle::~AwSafeBrowsingResourceThrottle() {}

void AwSafeBrowsingResourceThrottle::CancelResourceLoad() {
  CancelWithError(net::ERR_FAILED);
}

}  // namespace android_webview
