// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INTERNALS_UI_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "content/public/browser/web_ui_controller.h"

// The implementation for the chrome://signin-internals page.
class SignInInternalsUI : public content::WebUIController,
                          public AboutSigninInternals::Observer {
 public:
  explicit SignInInternalsUI(content::WebUI* web_ui);
  virtual ~SignInInternalsUI();


  // content::WebUIController implementation.
  virtual bool OverrideHandleWebUIMessage(const GURL& source_url,
                                          const std::string& name,
                                          const base::ListValue& args) OVERRIDE;

  // AboutSigninInternals::Observer::OnSigninStateChanged implementation.
  virtual void OnSigninStateChanged(
      scoped_ptr<base::DictionaryValue> info) OVERRIDE;

  // Notification that the cookie accounts are ready to be displayed.
  virtual void OnCookieAccountsFetched(
      scoped_ptr<base::DictionaryValue> info) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SignInInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INTERNALS_UI_H_
