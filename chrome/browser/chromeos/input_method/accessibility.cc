// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/accessibility.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/accessibility/accessibility_events.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace chromeos {
namespace input_method {

Accessibility::Accessibility(InputMethodManager* imm)
    : imm_(imm) {
  DCHECK(imm_);
  imm_->AddObserver(this);
}

Accessibility::~Accessibility() {
  DCHECK(imm_);
  imm_->RemoveObserver(this);
}

void Accessibility::InputMethodChanged(InputMethodManager* imm,
                                       bool show_message) {
  DCHECK_EQ(imm, imm_);
  if (!show_message)
    return;

  // Get the medium name of the changed input method (e.g. US, INTL, etc.)
  const InputMethodDescriptor descriptor = imm_->GetCurrentInputMethod();
  const std::string medium_name = base::UTF16ToUTF8(
      imm_->GetInputMethodUtil()->GetInputMethodMediumName(descriptor));

  AccessibilityAlertInfo event(ProfileManager::GetActiveUserProfile(),
                               medium_name);
  SendControlAccessibilityNotification(
      ui::AX_EVENT_ALERT, &event);
}

}  // namespace input_method
}  // namespace chromeos
