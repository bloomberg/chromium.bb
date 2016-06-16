// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_MANAGER_H_
#define CHROME_BROWSER_UI_USER_MANAGER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile_window.h"
#include "components/signin/core/browser/signin_metrics.h"

namespace base {
class FilePath;
}

// Cross-platform methods for displaying the user manager.
class UserManager {
 public:
  // Shows the User Manager or re-activates an existing one, focusing the
  // profile given by |profile_path_to_focus|; passing an empty base::FilePath
  // focuses no user pod. Based on the value of |tutorial_mode|, a tutorial
  // could be shown, in which case |profile_path_to_focus| is ignored. After a
  // profile is opened, executes the |profile_open_action|.
  static void Show(const base::FilePath& profile_path_to_focus,
                   profiles::UserManagerTutorialMode tutorial_mode,
                   profiles::UserManagerProfileSelected profile_open_action);

  // Hides the User Manager.
  static void Hide();

  // Returns whether the User Manager is showing.
  static bool IsShowing();

  // To be called once the User Manager's contents are showing.
  static void OnUserManagerShown();

  // Add a callback that will be called when OnUserManagerShown is called.
  static void AddOnUserManagerShownCallbackForTesting(
      const base::Closure& callback);

  // Shows a dialog where the user can re-authenticate the profile with the
  // given |email|. This is called in the following scenarios:
  //  -From the user manager when a profile is locked and the user's password is
  //   detected to have been changed.
  //  -From the user manager when a custodian account needs to be
  //   reauthenticated.
  // |reason| can be REASON_UNLOCK or REASON_REAUTHENTICATION to indicate
  // whether this is a reauth or unlock scenario.
  static void ShowReauthDialog(content::BrowserContext* browser_context,
                               const std::string& email,
                               signin_metrics::Reason reason);

  // Hides the reauth dialog if it is showing.
  static void HideReauthDialog();

  // TODO(noms): Figure out if this size can be computed dynamically or adjusted
  // for smaller screens.
  static const int kWindowWidth = 800;
  static const int kWindowHeight = 600;

  // Dimensions of the reauth dialog displaying the old-style signin flow with
  // the username and password challenge on the same form.
  static const int kPasswordCombinedReauthDialogHeight = 440;
  static const int kPasswordCombinedReauthDialogWidth = 360;

  // Dimensions of the reauth dialog displaying the password-separated signin
  // flow.
  static const int kReauthDialogHeight = 512;
  static const int kReauthDialogWidth =  448;

  // This class observes the WebUI used in the UserManager to perform online
  // reauthentication of locked profiles. It is concretely implemented in
  // UserManagerMac and UserManagerView to specialize the closing of the UI's
  // dialog widget.
  class ReauthDialogObserver : public content::WebContentsObserver {
   public:
    ReauthDialogObserver(content::WebContents* web_contents,
                         const std::string& email_address);

   private:
    // content::WebContentsObserver:
    void DidStopLoading() override;

    virtual void CloseReauthDialog() = 0;

    const std::string email_address_;

    DISALLOW_COPY_AND_ASSIGN(ReauthDialogObserver);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(UserManager);
};

#endif  // CHROME_BROWSER_UI_USER_MANAGER_H_
