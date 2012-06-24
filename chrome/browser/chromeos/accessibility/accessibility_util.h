// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_UTIL_H_
#pragma once

#include <string>

namespace content {
class WebUI;
}

namespace chromeos {
namespace accessibility {

// Enable or disable spoken feedback. Enabling spoken feedback installs the
// ChromeVox component extension.  If this is being called in a login/oobe
// login screen, pass the WebUI object in login_web_ui so that ChromeVox
// can be injected directly into that screen, otherwise it should be NULL.
void EnableSpokenFeedback(bool enabled, content::WebUI* login_web_ui);

// Enable or disable the high contrast mode for Chrome.
void EnableHighContrast(bool enabled);

// Enable or disable the screen magnifier.
void EnableScreenMagnifier(bool enabled);

// Enable or disable the virtual keyboard.
void EnableVirtualKeyboard(bool enabled);

// Toggles whether Chrome OS spoken feedback is on or off. See docs for
// EnableSpokenFeedback, above.
void ToggleSpokenFeedback(content::WebUI* login_web_ui);

// Speaks the specified string.
void Speak(const std::string& utterance);

// Returns true if spoken feedback is enabled, or false if not.
bool IsSpokenFeedbackEnabled();

// Returns true if High Contrast is enabled, or false if not.
bool IsHighContrastEnabled();

// Returns true if Screen Magnifier is enabled, or false if not.
bool IsScreenMagnifierEnabled();

// Speak the given text if the accessibility pref is already set.
void MaybeSpeak(const std::string& utterance);

}  // namespace accessibility
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_UTIL_H_
