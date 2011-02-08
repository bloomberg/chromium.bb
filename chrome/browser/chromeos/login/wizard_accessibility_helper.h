// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
#pragma once

#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_handler.h"
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

  // Speak the given text if the accessibility pref is already set. |queue|
  // specifies whether this utterance will be queued or spoken immediately.
  // |interruptible| specified whether this utterance can be flushed by a
  // subsequent utterance.
  // TODO (chaitanyag): Change API to use string16 instead of char*.
  void MaybeSpeak(const char* str, bool queue, bool interruptible);

  // Unregisters all accessibility notifications
  void UnregisterNotifications();

  // Toggles accessibility support.
  void ToggleAccessibility();

 private:
  friend struct DefaultSingletonTraits<WizardAccessibilityHelper>;

  WizardAccessibilityHelper();

  virtual ~WizardAccessibilityHelper() {}

  void RegisterNotifications();

  bool IsAccessibilityEnabled();

  void SetAccessibilityEnabled(bool);

  static scoped_ptr<views::Accelerator> accelerator_;

  scoped_ptr<WizardAccessibilityHandler> accessibility_handler_;

  Profile* profile_;

  // Used for tracking registrations to accessibility notifications.
  NotificationRegistrar registrar_;

  bool registered_notifications_;

  DISALLOW_COPY_AND_ASSIGN(WizardAccessibilityHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
