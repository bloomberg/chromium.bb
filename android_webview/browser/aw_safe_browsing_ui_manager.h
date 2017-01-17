// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them. This is android_webview
// specific ui_manager.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_UI_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_UI_MANAGER_H_

#include "components/safe_browsing/base_ui_manager.h"
#include "content/public/browser/web_contents.h"

namespace android_webview {

class AwSafeBrowsingUIManager : public safe_browsing::BaseUIManager {
 public:
  class UIManagerClient {
   public:
    static UIManagerClient* FromWebContents(content::WebContents* web_contents);

    // Whether this web contents can show an interstitial
    virtual bool CanShowInterstitial() = 0;
  };

  // Construction needs to happen on the UI thread.
  AwSafeBrowsingUIManager();

  void DisplayBlockingPage(const UnsafeResource& resource) override;

 protected:
  ~AwSafeBrowsingUIManager() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AwSafeBrowsingUIManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_UI_MANAGER_H_
