// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_TEST_WEB_STATE_H_
#define IOS_WEB_PUBLIC_TEST_TEST_WEB_STATE_H_

#include <stdint.h>

#include <string>

#include "base/strings/string16.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

namespace web {

// Minimal implementation of WebState, to be used in tests.
class TestWebState : public WebState {
 public:
  TestWebState();
  ~TestWebState() override;

  // WebState implementation.
  WebStateDelegate* GetDelegate() override;
  void SetDelegate(WebStateDelegate* delegate) override;
  bool IsWebUsageEnabled() const override;
  void SetWebUsageEnabled(bool enabled) override;
  UIView* GetView() override;
  BrowserState* GetBrowserState() const override;
  void OpenURL(const OpenURLParams& params) override {}
  NavigationManager* GetNavigationManager() override;
  CRWJSInjectionReceiver* GetJSInjectionReceiver() const override;
  void ExecuteJavaScript(const base::string16& javascript) override;
  void ExecuteJavaScript(const base::string16& javascript,
                         const JavaScriptResultCallback& callback) override;
  const std::string& GetContentsMimeType() const override;
  const std::string& GetContentLanguageHeader() const override;
  bool ContentIsHTML() const override;
  const base::string16& GetTitle() const override;
  bool IsLoading() const override;
  double GetLoadingProgress() const override;
  bool IsBeingDestroyed() const override;
  const GURL& GetVisibleURL() const override;
  const GURL& GetLastCommittedURL() const override;
  GURL GetCurrentURL(URLVerificationTrustLevel* trust_level) const override;
  void ShowTransientContentView(CRWContentView* content_view) override {}
  void AddScriptCommandCallback(const ScriptCommandCallback& callback,
                                const std::string& command_prefix) override {}
  void RemoveScriptCommandCallback(const std::string& command_prefix) override {
  }
  CRWWebViewProxyType GetWebViewProxy() const override;
  bool IsShowingWebInterstitial() const override;
  WebInterstitial* GetWebInterstitial() const override;
  int GetCertGroupId() const override;
  void AddObserver(WebStateObserver* observer) override {}
  void RemoveObserver(WebStateObserver* observer) override {}
  void AddPolicyDecider(WebStatePolicyDecider* decider) override {}
  void RemovePolicyDecider(WebStatePolicyDecider* decider) override {}
  int DownloadImage(const GURL& url,
                    bool is_favicon,
                    uint32_t max_bitmap_size,
                    bool bypass_cache,
                    const ImageDownloadCallback& callback) override;
  base::WeakPtr<WebState> AsWeakPtr() override;

  // Setters for test data.
  void SetContentIsHTML(bool content_is_html);
  void SetCurrentURL(const GURL& url);
  void SetTrustLevel(URLVerificationTrustLevel trust_level);

 private:
  bool web_usage_enabled_;
  GURL url_;
  base::string16 title_;
  URLVerificationTrustLevel trust_level_;
  bool content_is_html_;
  std::string mime_type_;
  std::string content_language_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_TEST_WEB_STATE_H_
