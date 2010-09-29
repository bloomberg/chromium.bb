// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_VIEW_H_
#pragma once

#include "views/accelerator.h"
#include "views/controls/button/native_button.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"

namespace chromeos {

class UserController;

class ExistingUserView : public views::View {
 public:
  explicit ExistingUserView(UserController* uc);

  void RecreateFields();

  views::Textfield* password_field() { return password_field_; }

  views::NativeButton* submit_button() { return submit_button_; }

  void FocusPasswordField();

  // Overridden from views::View:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

 protected:
  // Overridden from views::View:
  virtual void OnLocaleChanged();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  views::Accelerator accel_login_off_the_record_;

  // For editing the password.
  views::Textfield* password_field_;

  // Button to start login.
  views::NativeButton* submit_button_;

  UserController* user_controller_;

 private:
  views::Accelerator accel_enable_accessibility_;

  DISALLOW_COPY_AND_ASSIGN(ExistingUserView);
};

}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_VIEW_H_
