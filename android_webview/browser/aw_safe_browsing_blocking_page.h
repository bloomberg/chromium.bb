// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_BLOCKING_PAGE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_BLOCKING_PAGE_H_

#include "components/safe_browsing/base_blocking_page.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"

namespace security_interstitials {
struct UnsafeResource;
}  // namespace security_interstitials

namespace android_webview {

class AwSafeBrowsingUIManager;

class AwSafeBrowsingBlockingPage : public safe_browsing::BaseBlockingPage {
 public:
  typedef security_interstitials::UnsafeResource UnsafeResource;

  static void ShowBlockingPage(AwSafeBrowsingUIManager* ui_manager,
                               const UnsafeResource& unsafe_resource);

 protected:
  // Don't instantiate this class directly, use ShowBlockingPage instead.
  AwSafeBrowsingBlockingPage(
      AwSafeBrowsingUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResourceList& unsafe_resources,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client,
      const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_BLOCKING_PAGE_H_
