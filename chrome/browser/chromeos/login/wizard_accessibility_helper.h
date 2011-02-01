// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
#pragma once

#include <map>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_handler.h"
#include "chrome/browser/ui/views/accessible_view_helper.h"
#include "chrome/common/notification_registrar.h"
#include "ui/base/keycodes/keyboard_codes.h"

class Profile;
namespace views {
class Accelerator;
class View;
}

namespace chromeos {

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
  void EnableAccessibilityForView(views::View* view_tree);

  // Enables accessibility for the specified |view_tree| if the
  // accessibility pref is already set. Otherwise the |view_tree| is
  // added to a view buffer so that accessibility can be enabled for it
  // later when requested.
  void MaybeEnableAccessibility(views::View* view_tree);

  // Speak the given text if the accessibility pref is already set. |queue|
  // specifies whether this utterance will be queued or spoken immediately.
  // |interruptible| specified whether this utterance can be flushed by a
  // subsequent utterance.
  // TODO (chaitanyag): Change API to use string16 instead of char*.
  void MaybeSpeak(const char* str, bool queue, bool interruptible);

  // Unregisters all accessibility notifications
  void UnregisterNotifications();

  // Toggles accessibility support. If |view_tree| is null, only the
  // access preference setting is toggled. |view_tree| has no effect while
  // disabling accessibility.
  void ToggleAccessibility(views::View* view_tree);

 private:
  friend struct DefaultSingletonTraits<WizardAccessibilityHelper>;

  WizardAccessibilityHelper();

  virtual ~WizardAccessibilityHelper() {}

  void RegisterNotifications();

  bool IsAccessibilityEnabled();

  void SetAccessibilityEnabled(bool);

  static scoped_ptr<views::Accelerator> accelerator_;

  void AddViewToBuffer(views::View* view_tree);

  std::map<views::View*, bool> views_buffer_;

  std::vector<AccessibleViewHelper*> accessible_view_helpers_;

  scoped_ptr<WizardAccessibilityHandler> accessibility_handler_;

  Profile* profile_;

  // Used for tracking registrations to accessibility notifications.
  NotificationRegistrar registrar_;

  bool registered_notifications_;

  DISALLOW_COPY_AND_ASSIGN(WizardAccessibilityHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
