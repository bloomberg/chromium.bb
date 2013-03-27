// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_SIGNIN_CONFIRMATION_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_SIGNIN_CONFIRMATION_DIALOG_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "ui/base/ui_base_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Profile;
class WebUIMessageHandler;

// A tab-modal dialog to allow a user signing in with a managed account
// to create a new Chrome profile.
class ProfileSigninConfirmationDialog : public ui::WebDialogDelegate {
 public:
  // Creates and shows the modal dialog.  |profile| is the current Chrome
  // profile and |username| is the GAIA username that the user is signing
  // in with.
  static void ShowDialog(Profile* profile,
                         const std::string& username,
                         const base::Closure& cancel_signin,
                         const base::Closure& signin_with_new_profile,
                         const base::Closure& continue_signin);

  // Closes the dialog, which will delete itself.
  void Close() const;

 private:
  ProfileSigninConfirmationDialog(
      Profile* profile,
      const std::string& username,
      const base::Closure& cancel_signin,
      const base::Closure& signin_with_new_profile,
      const base::Closure& continue_signin);
  virtual ~ProfileSigninConfirmationDialog();

  // Shows the dialog and releases ownership of this object. It will
  // delete itself when the dialog is closed. If |prompt_for_new_profile|
  // is true, the dialog will offer to create a new profile before signin.
  void Show(bool prompt_for_new_profile);

  // Determines whether the user should be prompted to create a new
  // profile before signin.
  void CheckShouldPromptForNewProfile(base::Callback<void(bool)> callback);
  bool HasBeenShutdown();
  bool HasBookmarks();
  void CheckHasHistory(int max_entries,
                       base::Callback<void(bool)> cb);
  void OnHistoryQueryResults(size_t max_entries,
                             base::Callback<void(bool)> cb,
                             CancelableRequestProvider::Handle handle,
                             history::QueryResults* results);
  bool HasSyncedExtensions();
  void CheckHasTypedURLs(base::Callback<void(bool)> cb);

  // WebDialogDelegate implementation.
  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(content::WebContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;

  friend class ProfileSigninConfirmationDialogTest;
  FRIEND_TEST_ALL_PREFIXES(ProfileSigninConfirmationDialogTest,
                           DoNotPromptForNewProfile);
  FRIEND_TEST_ALL_PREFIXES(ProfileSigninConfirmationDialogTest,
                           PromptForNewProfile_Bookmarks);
  FRIEND_TEST_ALL_PREFIXES(ProfileSigninConfirmationDialogTest,
                           PromptForNewProfile_Extensions);
  FRIEND_TEST_ALL_PREFIXES(ProfileSigninConfirmationDialogTest,
                           PromptForNewProfile_History);
  FRIEND_TEST_ALL_PREFIXES(ProfileSigninConfirmationDialogTest,
                           PromptForNewProfile_TypedURLs);
  FRIEND_TEST_ALL_PREFIXES(ProfileSigninConfirmationDialogTest,
                           PromptForNewProfile_Restarted);

  // Weak ptr to delegate.
  ConstrainedWebDialogDelegate* delegate_;

  // The GAIA username being signed in.
  std::string username_;

  // Whether to show the "Create a new profile" button.
  bool prompt_for_new_profile_;

  // Dialog button callbacks.
  base::Closure cancel_signin_;
  base::Closure signin_with_new_profile_;
  base::Closure continue_signin_;

  // Weak pointer to the profile being signed-in.
  Profile* profile_;

  // Cleanup bookkeeping.  Labeled mutable to get around inherited const
  // label on GetWebUIMessageHandlers.
  mutable bool closed_by_handler_;

  // Used to count history items.
  CancelableRequestConsumer history_count_request_consumer;

  // Used to count typed URLs.
  CancelableRequestConsumer typed_urls_request_consumer;

  base::WeakPtrFactory<ProfileSigninConfirmationDialog> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSigninConfirmationDialog);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_SIGNIN_CONFIRMATION_DIALOG_H_
