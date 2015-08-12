// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_MANAGER_H_
#define CHROME_BROWSER_UI_USER_MANAGER_H_

#include "chrome/browser/profiles/profile_window.h"

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

  // Shows a dialog where the user can re-authenticate when their profile is
  // not yet open.  This is called from the user manager when a profile is
  // locked and it has detected that the user's password has changed since the
  // profile was locked.
  static void ShowReauthDialog(content::BrowserContext* browser_context,
                               const std::string& email);

  // TODO(noms): Figure out if this size can be computed dynamically or adjusted
  // for smaller screens.
  static const int kWindowWidth = 800;
  static const int kWindowHeight = 600;

  static const int kReauthDialogWidth = 360;
  static const int kReauthDialogHeight = 440;

  // This class observes the WebUI used in the UserManager to perform online
  // reauthentication of locked profiles. Its concretely implemented in
  // UserManagerMac and UserManagerView to specialize the closing of the UI's
  // dialog widget.
  class ReauthDialogObserver : public content::WebContentsObserver {
   public:
    ReauthDialogObserver(content::WebContents* web_contents,
                         const std::string& email_address);
    ~ReauthDialogObserver() override {}

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
