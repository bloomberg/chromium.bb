// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

namespace chromeos {

// static
WizardAccessibilityHelper* WizardAccessibilityHelper::GetInstance() {
  return Singleton<WizardAccessibilityHelper>::get();
}

WizardAccessibilityHelper::WizardAccessibilityHelper() {}

WizardAccessibilityHelper::~WizardAccessibilityHelper() {}

bool WizardAccessibilityHelper::IsAccessibilityEnabled() {
  return g_browser_process &&
      g_browser_process->local_state()->GetBoolean(
      prefs::kSpokenFeedbackEnabled);
}

void WizardAccessibilityHelper::MaybeSpeak(const char* str, bool queue,
    bool interruptible) {
  if (IsAccessibilityEnabled()) {
    // Note: queue and interruptible are no longer supported, but
    // that shouldn't matter because the whole views-based wizard
    // is obsolete and should be deleted soon.
    accessibility::Speak(str);
  }
}

}  // namespace chromeos
