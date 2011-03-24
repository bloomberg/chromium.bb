// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "views/controls/button/native_button.h"
#include "views/view.h"

namespace views {
class GridLayout;
class Label;
}

namespace chromeos {

class EnterpriseEnrollmentController;
class ScreenObserver;

// Implements the UI for the enterprise enrollment screen in OOBE.
class EnterpriseEnrollmentView : public views::View,
                                 public views::ButtonListener {
 public:
  explicit EnterpriseEnrollmentView(ScreenObserver* observer);
  virtual ~EnterpriseEnrollmentView();

  // Initialize view controls and layout.
  void Init();

  // Sets the controller.
  void set_controller(EnterpriseEnrollmentController* controller) {
    controller_ = controller;
  }

 private:
  // Update strings from the resources. Executed on language change.
  void UpdateLocalizedStrings();

  // Creates the layout manager and registers it with the view.
  views::GridLayout* CreateLayout();

  // Overriden from views::View:
  virtual void OnLocaleChanged() OVERRIDE;

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event)
      OVERRIDE;

  EnterpriseEnrollmentController* controller_;

  // Controls.
  views::Label* title_label_;
  views::NativeButton* cancel_button_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_VIEW_H_
