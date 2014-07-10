// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILES_USER_MANAGER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_PROFILES_USER_MANAGER_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"

@class UserManagerWindowController;

namespace content {
class NavigationController;
class WebContents;
}

// Dialog widget that contains the Desktop User Manager webui.
class UserManagerMac {
 public:
  // Shows the User Manager or re-activates an existing one, focusing the
  // profile given by |profile_path_to_focus|. Based on the value of
  // |tutorial_mode|, a tutorial could be shown, in which case
  // |profile_path_to_focus| is ignored.
  static void Show(const base::FilePath& profile_path_to_focus,
                   profiles::UserManagerTutorialMode tutorial_mode);

  // Hide the User Manager.
  static void Hide();

  // Returns whether or not the User Manager is showing.
  static bool IsShowing();

  // Called by the cocoa window controller when its window closes and the
  // controller destroyed itself. Deletes the instance.
  void WindowWasClosed();

 private:
  explicit UserManagerMac(Profile* profile);
  virtual ~UserManagerMac();

  // Creates a new UserManagerMac instance for the |guest_profile| and
  // shows the |url|.
  static void OnGuestProfileCreated(Profile* guest_profile,
                                    const std::string& url);

  // An open User Manager window. There can only be one open at a time. This
  // is reset to NULL when the window is closed.
  static UserManagerMac* instance_;  // Weak.

  // Controller of the window.
  base::scoped_nsobject<UserManagerWindowController> window_controller_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_PROFILES_USER_MANAGER_MAC_H_
