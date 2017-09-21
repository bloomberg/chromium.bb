// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_RESOURCE_THROTTLE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_RESOURCE_THROTTLE_H_

#include "android_webview/browser/aw_safe_browsing_ui_manager.h"
#include "android_webview/browser/aw_url_checker_delegate_impl.h"
#include "android_webview/browser/net/aw_web_resource_request.h"
#include "base/macros.h"
#include "components/safe_browsing/base_resource_throttle.h"
#include "components/safe_browsing/browser/base_parallel_resource_throttle.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/common/resource_type.h"

namespace net {
class URLRequest;
}

using safe_browsing::ThreatMetadata;
using safe_browsing::SBThreatType;

namespace android_webview {

class AwSafeBrowsingWhitelistManager;

// Contructs a resource throttle for SafeBrowsing. It returns an
// AwSafeBrowsingParallelResourceThrottle instance if
// --enable-features=S13nSafeBrowsingParallelUrlCheck is specified; returns
// an AwSafeBrowsingResourceThrottle otherwise.
//
// It returns nullptr if GMS doesn't exist on device or support SafeBrowsing.
content::ResourceThrottle* MaybeCreateAwSafeBrowsingResourceThrottle(
    net::URLRequest* request,
    content::ResourceType resource_type,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
    AwSafeBrowsingWhitelistManager* whitelist_manager);

bool IsCancelledBySafeBrowsing(const net::URLRequest* request);

class AwSafeBrowsingResourceThrottle
    : public safe_browsing::BaseResourceThrottle {
 protected:
  bool CheckUrl(const GURL& url) override;

 private:
  friend content::ResourceThrottle* MaybeCreateAwSafeBrowsingResourceThrottle(
      net::URLRequest* request,
      content::ResourceType resource_type,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
      AwSafeBrowsingWhitelistManager* whitelist_manager);

  AwSafeBrowsingResourceThrottle(
      net::URLRequest* request,
      content::ResourceType resource_type,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
      AwSafeBrowsingWhitelistManager* whitelist_manager);

  ~AwSafeBrowsingResourceThrottle() override;

  void StartDisplayingBlockingPageHelper(
      security_interstitials::UnsafeResource resource) override;

  // safe_browsing::BaseResourceThrottle overrides:
  void CancelResourceLoad() override;

  net::URLRequest* request_;

  scoped_refptr<safe_browsing::UrlCheckerDelegate> url_checker_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AwSafeBrowsingResourceThrottle);
};

// Unlike AwSafeBrowsingResourceThrottle, this class never defers starting the
// URL request or following redirects. If any of the checks for the original URL
// and redirect chain are not complete by the time the response headers are
// available, the request is deferred until all the checks are done.
class AwSafeBrowsingParallelResourceThrottle
    : public safe_browsing::BaseParallelResourceThrottle {
 private:
  friend content::ResourceThrottle* MaybeCreateAwSafeBrowsingResourceThrottle(
      net::URLRequest* request,
      content::ResourceType resource_type,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
      AwSafeBrowsingWhitelistManager* whitelist_manager);

  AwSafeBrowsingParallelResourceThrottle(
      net::URLRequest* request,
      content::ResourceType resource_type,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
      AwSafeBrowsingWhitelistManager* whitelist_manager);

  ~AwSafeBrowsingParallelResourceThrottle() override;

  // safe_browsing::BaseParallelResourceThrottle overrides:
  void CancelResourceLoad() override;

  net::URLRequest* request_;

  DISALLOW_COPY_AND_ASSIGN(AwSafeBrowsingParallelResourceThrottle);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_RESOURCE_THROTTLE_H_
