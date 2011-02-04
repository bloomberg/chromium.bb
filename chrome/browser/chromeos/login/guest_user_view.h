// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_GUEST_USER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_GUEST_USER_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_input.h"
#include "views/accelerator.h"
#include "views/controls/button/native_button.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"

namespace chromeos {

class UserController;

// This view is for controls window of Guest mode pod that allows user to
// get temporary profile and use it for browsing. Contains only one Sign-in
// button and handles different common keyboard shortcuts.
class GuestUserView : public ThrobberHostView,
                      public UserInput,
                      public views::ButtonListener {
 public:
  explicit GuestUserView(UserController* uc);

  void RecreateFields();

  // Overridden from views::View:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);
  virtual void RequestFocus();

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from UserInput:
  virtual void EnableInputControls(bool enabled);
  virtual void ClearAndFocusControls();
  virtual void ClearAndFocusPassword();
  virtual gfx::Rect GetMainInputScreenBounds() const;

 private:
  // Overridden from views::View:
  virtual void OnLocaleChanged();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
  virtual void Layout();

  // Button to start login.
  views::NativeButton* submit_button_;

  UserController* user_controller_;

  views::Accelerator accel_toggle_accessibility_;
  views::Accelerator accel_login_off_the_record_;
  views::Accelerator accel_previous_pod_by_arrow_;
  views::Accelerator accel_previous_pod_by_tab_;
  views::Accelerator accel_next_pod_by_arrow_;
  views::Accelerator accel_next_pod_by_tab_;

  DISALLOW_COPY_AND_ASSIGN(GuestUserView);
};

}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_GUEST_USER_VIEW_H_
