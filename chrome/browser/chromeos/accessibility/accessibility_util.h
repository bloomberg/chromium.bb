// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_UTIL_H_

#include <string>

#include "ash/magnifier/magnifier_constants.h"
#include "ash/shell_delegate.h"
#include "base/basictypes.h"

class Browser;

namespace content {
class WebUI;
}

namespace chromeos {
namespace accessibility {

struct AccessibilityStatusEventDetails {
  AccessibilityStatusEventDetails(
      bool enabled, ash::AccessibilityNotificationVisibility notify)
    : enabled(enabled),
      magnifier_type(ash::kDefaultMagnifierType),
      notify(notify) {}

  AccessibilityStatusEventDetails(
      bool enabled,
      ash::MagnifierType magnifier_type,
      ash::AccessibilityNotificationVisibility notify)
    : enabled(enabled),
      magnifier_type(magnifier_type),
      notify(notify) {}

  bool enabled;
  ash::MagnifierType magnifier_type;
  ash::AccessibilityNotificationVisibility notify;
};

// Do any accessibility initialization that should happen once on startup.
void Initialize();

// Enables or disables spoken feedback. Enabling spoken feedback installs the
// ChromeVox component extension.  If this is being called in a login/oobe
// login screen, pass the WebUI object in login_web_ui so that ChromeVox
// can be injected directly into that screen, otherwise it should be NULL.
void EnableSpokenFeedback(bool enabled,
                          content::WebUI* login_web_ui,
                          ash::AccessibilityNotificationVisibility notify);

// Enables or disables the high contrast mode for Chrome.
void EnableHighContrast(bool enabled);

// Enables or disable the virtual keyboard.
void EnableVirtualKeyboard(bool enabled);

// Toggles whether Chrome OS spoken feedback is on or off. See docs for
// EnableSpokenFeedback, above.
void ToggleSpokenFeedback(content::WebUI* login_web_ui,
                          ash::AccessibilityNotificationVisibility notify);

// Speaks the specified string.
void Speak(const std::string& utterance);

// Returns true if spoken feedback is enabled, or false if not.
bool IsSpokenFeedbackEnabled();

// Returns true if High Contrast is enabled, or false if not.
bool IsHighContrastEnabled();

// Returns true if the Virtual Keyboard is enabled, or false if not.
bool IsVirtualKeyboardEnabled();

// Speaks the given text if the accessibility pref is already set.
void MaybeSpeak(const std::string& utterance);

// Shows the accessibility help tab on the browser.
void ShowAccessibilityHelp(Browser* browser);

}  // namespace accessibility
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_UTIL_H_
