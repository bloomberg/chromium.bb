// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_INTERNAL_CHROME_BROWSER_WEB_WEB_CONTROLLER_PROVIDER_IMPL_H_
#define IOS_INTERNAL_CHROME_BROWSER_WEB_WEB_CONTROLLER_PROVIDER_IMPL_H_

#include <memory>

#import "ios/public/provider/web/web_controller_provider.h"

namespace web {
class WebStateImpl;
}

// Concrete implementation of ios::WebControllerProvider.
class WebControllerProviderImpl : public ios::WebControllerProvider {
 public:
  explicit WebControllerProviderImpl(web::BrowserState* browser_state);
  ~WebControllerProviderImpl() override;

  // ios::WebControllerProvider implementation.
  bool SuppressesDialogs() const override;
  void SetSuppressesDialogs(bool should_suppress_dialogs) override;
  void LoadURL(const GURL& url) override;
  web::WebState* GetWebState() const override;
  void InjectScript(const std::string& script,
                    web::JavaScriptResultBlock completion) override;

 private:
  std::unique_ptr<web::WebStateImpl> web_state_impl_;
  GURL last_requested_url_;
  bool suppresses_dialogs_;
};

#endif  // IOS_INTERNAL_CHROME_BROWSER_WEB_WEB_CONTROLLER_PROVIDER_IMPL_H_
