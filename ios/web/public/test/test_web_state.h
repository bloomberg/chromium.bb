// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_TEST_WEB_STATE_H_
#define IOS_WEB_PUBLIC_TEST_TEST_WEB_STATE_H_

#include <string>

#include "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

namespace web {

// Minimal implementation of WebState, to be used in tests.
class TestWebState : public WebState {
 public:
  // WebState implementation.
  BrowserState* GetBrowserState() const override;
  void OpenURL(const OpenURLParams& params) override {}
  NavigationManager* GetNavigationManager() override;
  CRWJSInjectionReceiver* GetJSInjectionReceiver() const override;
  const std::string& GetContentsMimeType() const override;
  const std::string& GetContentLanguageHeader() const override;
  bool ContentIsHTML() const override;
  const GURL& GetVisibleURL() const override;
  const GURL& GetLastCommittedURL() const override;
  void AddScriptCommandCallback(const ScriptCommandCallback& callback,
                                const std::string& command_prefix) override {}
  void RemoveScriptCommandCallback(const std::string& command_prefix) override {
  }
  void AddObserver(WebStateObserver* observer) override {}
  void RemoveObserver(WebStateObserver* observer) override {}

 private:
  GURL url_;
  std::string mime_type_;
  std::string content_language_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_TEST_WEB_STATE_H_
