// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_INLINE_LOGIN_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_INLINE_LOGIN_HANDLER_CHROMEOS_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"

namespace chromeos {

class OAuth2TokenFetcher;

// Implementation for the inline login WebUI handler on ChromeOS.
class InlineLoginHandlerChromeOS : public ::InlineLoginHandler {
 public:
  InlineLoginHandlerChromeOS();
  virtual ~InlineLoginHandlerChromeOS();

 private:
  class InlineLoginUIOAuth2Delegate;

  // InlineLoginHandler overrides:
  virtual void CompleteLogin(const base::ListValue* args) OVERRIDE;

  scoped_ptr<InlineLoginUIOAuth2Delegate> oauth2_delegate_;
  scoped_ptr<chromeos::OAuth2TokenFetcher> oauth2_token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginHandlerChromeOS);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_INLINE_LOGIN_HANDLER_CHROMEOS_H_
