// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILE_CHOOSER_CONSTANTS_H_
#define CHROME_BROWSER_UI_PROFILE_CHOOSER_CONSTANTS_H_

namespace profiles {

// Different views that can be displayed in the profile chooser bubble.
enum BubbleViewMode {
  // Shows a "fast profile switcher" view.
  BUBBLE_VIEW_MODE_PROFILE_CHOOSER,
  // Shows a list of accounts for the active user.
  BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT,
  // Shows a web view for primary sign in.
  BUBBLE_VIEW_MODE_GAIA_SIGNIN,
  // Shows a web view for adding secondary accounts.
  BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT,
  // Shows a web view for reauthenticating an account.
  BUBBLE_VIEW_MODE_GAIA_REAUTH,
  // Shows a view for confirming account removal.
  BUBBLE_VIEW_MODE_ACCOUNT_REMOVAL,
  // Shows a view for switching user.
  BUBBLE_VIEW_MODE_SWITCH_USER,
};

// Tutorial modes that can be displayed in the profile chooser bubble.
enum TutorialMode {
  // No tutorial card shown.
  TUTORIAL_MODE_NONE,
  // A tutorial card shown to confirm Chrome signin.
  TUTORIAL_MODE_CONFIRM_SIGNIN,
  // A tutorial card shown to introduce an upgrade user to the new avatar menu.
  TUTORIAL_MODE_WELCOME_UPGRADE,
  // A tutorial card shown to display the signin errors.
  TUTORIAL_MODE_SHOW_ERROR,
};

};  // namespace profiles

#endif  // CHROME_BROWSER_UI_PROFILE_CHOOSER_CONSTANTS_H_
