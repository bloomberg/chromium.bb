// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
#pragma once

#include <map>
#include <vector>

#include "app/keyboard_codes.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_handler.h"
#include "chrome/browser/views/accessible_view_helper.h"
#include "chrome/common/notification_registrar.h"

class Profile;
namespace views {
class Accelerator;
class View;
}

// Class that provides convenience methods to enable accessibility for a
// specified View.
class WizardAccessibilityHelper {
 public:
  // Get Singleton instance of WizardAccessibilityHelper.
  static WizardAccessibilityHelper* GetInstance();

  // Get accelerator for enabling accessibility.
  static views::Accelerator GetAccelerator();

  // Enables Accessibility by setting the accessibility pref and registers
  // all views in the view buffer to raise accessibility notifications,
  // including the specified |view_tree|.
  void EnableAccessibility(views::View* view_tree);

  // Enables accessibility for the specified |view_tree| if the
  // accessibility pref is already set. Otherwise the |view_tree| is
  // added to a view buffer so that accessibility can be enabled for it
  // later when requested.
  void MaybeEnableAccessibility(views::View* view_tree);

  // Speak the given text if the accessibility pref is already set. |queue|
  // specifies whether this utterance will be queued or spoken immediately.
  // |interruptible| specified whether this utterance can be flushed by a
  // subsequent utterance.
  void MaybeSpeak(const char* str, bool queue, bool interruptible);

 private:
  friend struct DefaultSingletonTraits<WizardAccessibilityHelper>;

  WizardAccessibilityHelper();

  virtual ~WizardAccessibilityHelper() {}

  static scoped_ptr<views::Accelerator> accelerator_;

  void AddViewToBuffer(views::View* view_tree);

  std::map<views::View*, bool> views_buffer_;

  std::vector<AccessibleViewHelper*> accessible_view_helpers_;

  scoped_ptr<WizardAccessibilityHandler> accessibility_handler_;

  Profile* profile_;

  // Used for tracking registrations to accessibility notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WizardAccessibilityHelper);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
