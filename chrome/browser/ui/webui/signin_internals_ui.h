// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INTERNALS_UI_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/values.h"
#include "content/public/browser/web_ui_controller.h"

// An Observer class that gets notified on changes to AboutSigninInternals.
class SigninObserver {
 public:
  virtual void OnSigninStateChanged(const DictionaryValue* info) = 0;
};

// The implementation for the chrome://signin-internals page.
class SignInInternalsUI : public content::WebUIController,
                          public SigninObserver {
 public:
  explicit SignInInternalsUI(content::WebUI* web_ui);
  virtual ~SignInInternalsUI();


  // content::WebUIController implementation.
  virtual bool OverrideHandleWebUIMessage(const GURL& source_url,
                                          const std::string& name,
                                          const base::ListValue& args) OVERRIDE;

  virtual void OnSigninStateChanged(const DictionaryValue* info) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SignInInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INTERNALS_UI_H_
