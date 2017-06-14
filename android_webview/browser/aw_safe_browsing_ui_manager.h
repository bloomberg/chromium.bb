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

namespace safe_browsing {
class BasePingManager;
class SafeBrowsingURLRequestContextGetter;
}  // namespace

namespace android_webview {
class AwURLRequestContextGetter;

class AwSafeBrowsingUIManager : public safe_browsing::BaseUIManager {
 public:
  class UIManagerClient {
   public:
    static UIManagerClient* FromWebContents(content::WebContents* web_contents);

    // Whether this web contents can show any sort of interstitial
    virtual bool CanShowInterstitial() = 0;

    // Returns the appropriate BaseBlockingPage::ErrorUiType
    virtual int GetErrorUiType() = 0;
  };

  // Construction needs to happen on the UI thread.
  explicit AwSafeBrowsingUIManager(
      AwURLRequestContextGetter* browser_url_request_context_getter);

  // Gets the correct ErrorUiType for the web contents
  int GetErrorUiType(const UnsafeResource& resource) const;

  // BaseUIManager methods:
  void DisplayBlockingPage(const UnsafeResource& resource) override;

  // Called on the IO thread by the ThreatDetails with the serialized
  // protocol buffer, so the service can send it over.
  void SendSerializedThreatDetails(const std::string& serialized) override;

 protected:
  ~AwSafeBrowsingUIManager() override;

  void ShowBlockingPageForResource(const UnsafeResource& resource) override;

 private:
  // Provides phishing and malware statistics. Accessed on IO thread.
  std::unique_ptr<safe_browsing::BasePingManager> ping_manager_;

  // The SafeBrowsingURLRequestContextGetter used to access
  // |url_request_context_|. Accessed on UI thread.
  scoped_refptr<safe_browsing::SafeBrowsingURLRequestContextGetter>
      url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(AwSafeBrowsingUIManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_SAFE_BROWSING_UI_MANAGER_H_
