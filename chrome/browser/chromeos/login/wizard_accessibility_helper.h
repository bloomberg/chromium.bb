// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
#pragma once

#include "base/keyboard_codes.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_handler.h"
#include "chrome/browser/views/accessible_view_helper.h"
#include "chrome/common/notification_registrar.h"

class Profile;
namespace views {
class View;
}

// Class that provides convenience methods to enable accessibility for a
// specified View.
class WizardAccessibilityHelper {
 public:
  // Get Singleton instance of WizardAccessibilityHelper.
  static WizardAccessibilityHelper* GetInstance();

  // Enables Accessibility by setting the accessibility pref and
  // registering the specified view_tree to raise UI notifications.
  void EnableAccessibility(views::View* view_tree, Profile* profile);

  // Enabled accessibility for the specified view_tree if the
  // accessibility pref is already set.
  void MaybeEnableAccessibility(views::View* view_tree, Profile* profile);

  // Keyboard accelerator key to enable accessibility.
  static const base::KeyboardCode accelerator = base::VKEY_Z;

 private:
  friend struct DefaultSingletonTraits<WizardAccessibilityHelper>;
  WizardAccessibilityHelper();
  ~WizardAccessibilityHelper() {}

  scoped_ptr<AccessibleViewHelper> accessible_view_helper_;

  scoped_ptr<WizardAccessibilityHandler> accessibility_handler_;

  // Used for tracking registrations to accessibility notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WizardAccessibilityHelper);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
