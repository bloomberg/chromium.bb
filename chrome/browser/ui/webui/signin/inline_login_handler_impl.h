// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

class GaiaAuthFetcher;

// Implementation for the inline login WebUI handler on desktop Chrome. Once
// CrOS migrates to the same webview approach as desktop Chrome, much of the
// code in this class should move to its base class |InlineLoginHandler|.
class InlineLoginHandlerImpl : public GaiaAuthConsumer,
                               public InlineLoginHandler {
 public:
  InlineLoginHandlerImpl();
  virtual ~InlineLoginHandlerImpl();

 private:
  // InlineLoginHandler overrides:
  virtual void RegisterMessages() OVERRIDE;
  virtual void SetExtraInitParams(base::DictionaryValue& params) OVERRIDE;
  virtual void CompleteLogin(const base::ListValue* args) OVERRIDE;

  // GaiaAuthConsumer override.
  virtual void OnClientOAuthCodeSuccess(const std::string& oauth_code) OVERRIDE;
  virtual void OnClientOAuthCodeFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  // JS callback to switch the UI from a constrainted dialog to a full tab.
  void HandleSwitchToFullTabMessage(const base::ListValue* args);
  void HandleLoginError(const std::string& error_msg);
  void SyncStarterCallback(OneClickSigninSyncStarter::SyncSetupResult result);
  void CloseTab();

  base::WeakPtrFactory<InlineLoginHandlerImpl> weak_factory_;
  scoped_ptr<GaiaAuthFetcher> auth_fetcher_;
  std::string email_;
  std::string password_;
  std::string session_index_;
  bool choose_what_to_sync_;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginHandlerImpl);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_
