// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
#pragma once

#include "base/memory/singleton.h"

namespace chromeos {

// Class that provides convenience methods to enable accessibility for a
// specified View.
class WizardAccessibilityHelper {
 public:
  // Get Singleton instance of WizardAccessibilityHelper.
  static WizardAccessibilityHelper* GetInstance();

  // Speak the given text if the accessibility pref is already set. |queue|
  // specifies whether this utterance will be queued or spoken immediately.
  // |interruptible| specified whether this utterance can be flushed by a
  // subsequent utterance.
  // TODO (chaitanyag): Change API to use string16 instead of char*.
  void MaybeSpeak(const char* str, bool queue, bool interruptible);

 private:
  friend struct DefaultSingletonTraits<WizardAccessibilityHelper>;

  WizardAccessibilityHelper();

  virtual ~WizardAccessibilityHelper();

  bool IsAccessibilityEnabled();

  DISALLOW_COPY_AND_ASSIGN(WizardAccessibilityHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_ACCESSIBILITY_HELPER_H_
