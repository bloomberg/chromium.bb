// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"
#include "content/public/browser/web_contents_delegate.h"

class GaiaAuthFetcher;

// Implementation for the inline login WebUI handler on desktop Chrome. Once
// CrOS migrates to the same webview approach as desktop Chrome, much of the
// code in this class should move to its base class |InlineLoginHandler|.
class InlineLoginHandlerImpl : public InlineLoginHandler,
                               public content::WebContentsDelegate,
                               public content::WebContentsObserver {
 public:
  InlineLoginHandlerImpl();
  virtual ~InlineLoginHandlerImpl();

  using InlineLoginHandler::web_ui;

  base::WeakPtr<InlineLoginHandlerImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  Browser* GetDesktopBrowser();
  void SyncStarterCallback(OneClickSigninSyncStarter::SyncSetupResult result);
  // Closes the current tab and shows the account management view of the avatar
  // bubble if |show_account_management| is true.
  void CloseTab(bool show_account_management);
  void HandleLoginError(const std::string& error_msg);

 private:
  // InlineLoginHandler overrides:
  virtual void SetExtraInitParams(base::DictionaryValue& params) OVERRIDE;
  virtual void CompleteLogin(const base::ListValue* args) OVERRIDE;

  // Overridden from content::WebContentsDelegate.
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;

  // Overridden from content::WebContentsObserver overrides.
  virtual void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      content::PageTransition transition_type) OVERRIDE;

  base::WeakPtrFactory<InlineLoginHandlerImpl> weak_factory_;
  // True if the user has navigated to untrusted domains during the signin
  // process.
  bool confirm_untrusted_signin_;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginHandlerImpl);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_
