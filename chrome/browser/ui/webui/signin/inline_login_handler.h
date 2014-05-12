// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

// The base class handler for the inline login WebUI.
class InlineLoginHandler : public content::WebUIMessageHandler {
 public:
  InlineLoginHandler();
  virtual ~InlineLoginHandler();

  // content::WebUIMessageHandler overrides:
  virtual void RegisterMessages() OVERRIDE;

 protected:
  // Enum for gaia auth mode, must match AuthMode defined in
  // chrome/browser/resources/gaia_auth_host/gaia_auth_host.js.
  enum AuthMode {
    kDefaultAuthMode = 0,
    kOfflineAuthMode = 1,
    kDesktopAuthMode = 2
  };

 private:
  // JS callback to initialize the gaia auth extension. It calls
  // |SetExtraInitParams| to set extra init params.
  void HandleInitializeMessage(const base::ListValue* args);
  // JS callback to complete login. It calls |CompleteLogin| to do the real
  // work.
  void HandleCompleteLoginMessage(const base::ListValue* args);

  // JS callback to switch the UI from a constrainted dialog to a full tab.
  void HandleSwitchToFullTabMessage(const base::ListValue* args);

  virtual void SetExtraInitParams(base::DictionaryValue& params) {}
  virtual void CompleteLogin(const base::ListValue* args) = 0;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_H_
