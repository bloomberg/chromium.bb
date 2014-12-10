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
  virtual BrowserState* GetBrowserState() const override;
  virtual void OpenURL(const OpenURLParams& params) override {}
  virtual NavigationManager* GetNavigationManager() override;
  virtual CRWJSInjectionReceiver* GetJSInjectionReceiver() const override;
  virtual const std::string& GetContentsMimeType() const override;
  virtual const std::string& GetContentLanguageHeader() const override;
  virtual bool ContentIsHTML() const override;
  virtual const GURL& GetVisibleURL() const override;
  virtual const GURL& GetLastCommittedURL() const override;
  virtual void AddScriptCommandCallback(
      const ScriptCommandCallback& callback,
      const std::string& command_prefix) override {}
  virtual void RemoveScriptCommandCallback(
      const std::string& command_prefix) override {}
  virtual void AddObserver(WebStateObserver* observer) override {}
  virtual void RemoveObserver(WebStateObserver* observer) override {}

 private:
  GURL url_;
  std::string mime_type_;
  std::string content_language_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_TEST_WEB_STATE_H_
