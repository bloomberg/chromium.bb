// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_H_

#include "base/basictypes.h"

// Abstract base class for platform-specific password management icon views.
class ManagePasswordsIcon {
 public:
  enum State {
    // The icon should not be displayed.
    INACTIVE_STATE,

    // The user has blacklisted the current page; the icon should be visible,
    // and correctly represent the password manager's disabled state.
    BLACKLISTED_STATE,

    // Offer the user the ability to manage passwords for the current page.
    MANAGE_STATE,

    // Offer the user the ability to save a pending password.
    PENDING_STATE,
  };

  // Get/set the icon's state. Implementations of this class must implement
  // SetStateInternal to do reasonable platform-specific things to represent
  // the icon's state to the user.
  void SetState(State state);
  State state() const { return state_; }

  // Shows the bubble without user interaction; should only be called from
  // ManagePasswordsUIController.
  //
  // TODO(mkwst): This shouldn't be the IconView's responsiblity. Move it
  // somewhere else as part of the refactoring in http://crbug.com/365678.
  virtual void ShowBubbleWithoutUserInteraction() = 0;

 protected:
  ManagePasswordsIcon();
  ~ManagePasswordsIcon();

  // Called from SetState() iff the icon's state has changed in order to do
  // whatever platform-specific UI work is necessary given the new state.
  virtual void UpdateVisibleUI() = 0;

 private:
  State state_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsIcon);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_H_
