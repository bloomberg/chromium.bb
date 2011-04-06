// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_input.h"
#include "views/accelerator.h"
#include "views/controls/button/native_button.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/view.h"

namespace chromeos {

class UserController;

class ExistingUserView : public ThrobberHostView,
                         public UserInput,
                         public views::TextfieldController {
 public:
  explicit ExistingUserView(UserController* user_controller);

  void RecreateFields();

  void FocusPasswordField();

  // views::View:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // views::TextfieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents);
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& keystroke);
  virtual void RequestFocus();

  // UserInput:
  virtual void EnableInputControls(bool enabled);
  virtual void ClearAndFocusControls();
  virtual void ClearAndFocusPassword();
  virtual gfx::Rect GetMainInputScreenBounds() const;

 protected:
  // views::View:
  virtual void OnLocaleChanged();

 private:
  UserController* user_controller_;

  // For editing the password.
  views::Textfield* password_field_;

  views::Accelerator accel_enterprise_enrollment_;
  views::Accelerator accel_login_off_the_record_;
  views::Accelerator accel_toggle_accessibility_;

  DISALLOW_COPY_AND_ASSIGN(ExistingUserView);
};

}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_VIEW_H_
