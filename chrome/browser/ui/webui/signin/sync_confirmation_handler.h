// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "content/public/browser/web_ui_message_handler.h"

class Browser;

namespace base {
class ListValue;
}

class SyncConfirmationHandler : public content::WebUIMessageHandler,
                                public AccountTrackerService::Observer {
 public:
  SyncConfirmationHandler();
  ~SyncConfirmationHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // AccountTrackerService::Observer:
  void OnAccountUpdated(const AccountInfo& info) override;

 protected:
  // Handles "confirm" message from the page. No arguments.
  // This message is sent when the user confirms that they want complete sign in
  // with default sync settings.
  virtual void HandleConfirm(const base::ListValue* args);

  // Handles "undo" message from the page. No arguments.
  // This message is sent when the user clicks "undo" on the sync confirmation
  // dialog, which aborts signin and prevents sync from starting.
  virtual void HandleUndo(const base::ListValue* args);

  // Handles "initialized" message from the page. No arguments.
  // This message is sent when the sync confirmation dialog is finished being
  // initialized.
  virtual void HandleInitialized(const base::ListValue* args);

  // Handles "goToSettings" message from the page. No arguments.
  // This message is sent when the user clicks on the "Settings" link in the
  // sync confirmation dialog, which completes sign in but takes the user to the
  // sync settings page for configuration before starting sync.
  virtual void HandleGoToSettings(const base::ListValue* args);

  // Sets the profile picture shown in the dialog to the image at |url|.
  virtual void SetUserImageURL(const std::string& url);

  Browser* GetDesktopBrowser();
  void CloseModalSigninWindow(
      LoginUIService::SyncConfirmationUIClosedResults results);

  DISALLOW_COPY_AND_ASSIGN(SyncConfirmationHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_HANDLER_H_
