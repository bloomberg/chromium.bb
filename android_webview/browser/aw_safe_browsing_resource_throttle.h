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
#include "components/safe_browsing_db/database_manager.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/common/resource_type.h"

namespace net {
class URLRequest;
}

using safe_browsing::ThreatMetadata;
using safe_browsing::SBThreatType;

namespace android_webview {

class AwSafeBrowsingWhitelistManager;

class AwSafeBrowsingResourceThrottle
    : public safe_browsing::BaseResourceThrottle {
 public:
  static const void* const kUserDataKey;

  // Will construct an AwSafeBrowsingResourceThrottle if GMS exists on device
  // and supports safebrowsing.
  static AwSafeBrowsingResourceThrottle* MaybeCreate(
      net::URLRequest* request,
      content::ResourceType resource_type,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
      AwSafeBrowsingWhitelistManager* whitelist_manager);

 protected:
  bool CheckUrl(const GURL& url) override;

 private:
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

  static void OnBlockingPageComplete(
      const base::WeakPtr<BaseResourceThrottle>& throttle,
      const base::Callback<void(bool)>& forward_callback,
      bool proceed);

  net::URLRequest* request_;

  scoped_refptr<safe_browsing::UrlCheckerDelegate> url_checker_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AwSafeBrowsingResourceThrottle);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_RESOURCE_THROTTLE_H_
