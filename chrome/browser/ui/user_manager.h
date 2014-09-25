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

  // TODO(noms): Figure out if this size can be computed dynamically or adjusted
  // for smaller screens.
  static const int kWindowWidth = 800;
  static const int kWindowHeight = 600;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserManager);
};

#endif  // CHROME_BROWSER_UI_USER_MANAGER_H_
