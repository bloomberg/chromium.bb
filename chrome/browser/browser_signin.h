// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SIGNIN_H_
#define CHROME_BROWSER_BROWSER_SIGNIN_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class BrowserSigninHtml;
class Profile;
class ProfileSyncService;
class TabContents;

// The BrowserSignin class provides a login screen which allows the
// user to signin to the browser.  Currently the signin is coordinated
// through the Chrome Sync logic.
//
// TODO(johnnyg): Separate this from the sync logic and make it the
// sole co-ordinator of browser signin.
//
// This class should only be accessed on the UI thread.
class BrowserSignin : public NotificationObserver {
 public:
  explicit BrowserSignin(Profile* profile);
  virtual ~BrowserSignin();

  // The delegate class is invoked on success and failure.
  class SigninDelegate {
   public:
    virtual ~SigninDelegate() {}

    // The login was successful.
    virtual void OnLoginSuccess() = 0;

    // The login failed.
    virtual void OnLoginFailure(const GoogleServiceAuthError& error) = 0;
  };

  // Request that the user signin, modal to TabContents provided.
  // If a user is already signed in, this will show a login dialog where
  // the username is not editable.
  //
  // A custom HTML string can be provided that will be displayed next
  // to the signin dialog.
  //
  // Only one sign-in can be in process at a time; if there is one in
  // progress already it will be canceled in favor of this one.
  //
  // The delegate will eventually be called with OnLoginSuccess() or
  // OnLoginFailure(), but never both.  virtual for test override.
  virtual void RequestSignin(TabContents* tab_contents,
                             const string16& suggested_email,
                             const string16& login_message,
                             SigninDelegate* delegate);

  // Returns the username of the user currently signed in.  If no
  // user is signed in, returns the empty string.  virtual for test
  // override.
  virtual std::string GetSignedInUsername() const;

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  ProfileSyncService* GetProfileSyncService() const;

  // Close the dialog.  Delegate's OnLoginFailure method will be called.
  void Cancel();

 private:
  // Create the HTML Dialog content.
  BrowserSigninHtml* CreateHtmlDialogUI();

  // When the dialog is closed.
  void OnLoginFinished();

  // Turn auth notifications on.
  void RegisterAuthNotifications();

  // Turn auth notifications off.
  void UnregisterAuthNotifications();

  // Show the dialog Tab-Modal.
  void ShowSigninTabModal(TabContents* tab_contents);

  // Non-owned pointer to the profile (which owns this object).
  Profile* profile_;

  // Suggested email for the current login prompt.
  string16 suggested_email_;

  // Current login message.
  string16 login_message_;

  // Delegate for the current sign in request.
  SigninDelegate* delegate_;

  // Current HTML Dialog information.  Pointer is owned by the WebUI it will be
  // attached to.
  BrowserSigninHtml* html_dialog_ui_delegate_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSignin);
};


#endif  // CHROME_BROWSER_BROWSER_SIGNIN_H_
