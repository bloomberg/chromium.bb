// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_RESOURCE_THROTTLE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_RESOURCE_THROTTLE_H_

#include "android_webview/browser/aw_safe_browsing_ui_manager.h"
#include "base/macros.h"
#include "components/safe_browsing/base_resource_throttle.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/common/resource_type.h"

namespace net {
class URLRequest;
}

namespace android_webview {

class AwSafeBrowsingResourceThrottle
    : public safe_browsing::BaseResourceThrottle {
 public:
  // Will construct an AwSafeBrowsingResourceThrottle if GMS exists on device
  // and supports safebrowsing.
  static AwSafeBrowsingResourceThrottle* MaybeCreate(
      net::URLRequest* request,
      content::ResourceType resource_type,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      scoped_refptr<AwSafeBrowsingUIManager> ui_manager);

 private:
  AwSafeBrowsingResourceThrottle(
      net::URLRequest* request,
      content::ResourceType resource_type,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      scoped_refptr<AwSafeBrowsingUIManager> ui_manager);

  ~AwSafeBrowsingResourceThrottle() override;

  void CancelResourceLoad() override;

  DISALLOW_COPY_AND_ASSIGN(AwSafeBrowsingResourceThrottle);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_RESOURCE_THROTTLE_H_
