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

  // Handles "goToSettings" message from the page. No arguments.
  // This message is sent when the user clicks on the "Settings" link in the
  // sync confirmation dialog, which completes sign in but takes the user to the
  // sync settings page for configuration before starting sync.
  virtual void HandleGoToSettings(const base::ListValue* args);

  // Handles the web ui message sent when the html content is done being laid
  // out and it's time to resize the native view hosting it to fit. |args| is
  // a single integer value for the height the native view should resize to.
  virtual void HandleInitializedWithSize(const base::ListValue* args);

  // Sets the profile picture shown in the dialog to the image at |url|.
  virtual void SetUserImageURL(const std::string& url);

  // Closes the modal signin window and calls
  // LoginUIService::SyncConfirmationUIClosed with |result|. |result| indicates
  // the option chosen by the user in the confirmation UI.
  void CloseModalSigninWindow(
      LoginUIService::SyncConfirmationUIClosedResult result);

 private:
  // Records whether the user clicked on Undo, Ok, or Settings.
  bool did_user_explicitly_interact;

  DISALLOW_COPY_AND_ASSIGN(SyncConfirmationHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_HANDLER_H_
