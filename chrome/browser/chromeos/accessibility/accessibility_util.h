// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_UTIL_H_

class Browser;

namespace chromeos {
namespace accessibility {

// Enables or disable the virtual keyboard.
void EnableVirtualKeyboard(bool enabled);

// Returns true if the Virtual Keyboard is enabled, or false if not.
bool IsVirtualKeyboardEnabled();

// Shows the accessibility help tab on the browser.
void ShowAccessibilityHelp(Browser* browser);

}  // namespace accessibility
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_UTIL_H_
