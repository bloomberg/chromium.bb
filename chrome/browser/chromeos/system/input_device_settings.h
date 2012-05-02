// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_INPUT_DEVICE_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_INPUT_DEVICE_SETTINGS_H_
#pragma once

namespace chromeos {
namespace system {

namespace touchpad_settings {

bool TouchpadExists();

// Sets the touchpad sensitivity in the range [1, 5].
void SetSensitivity(int value);

// Turns tap to click on / off.
void SetTapToClick(bool enabled);

// Switch for three-finger click.
void SetThreeFingerClick(bool enabled);

}  // namespace touchpad_settings

namespace mouse_settings {

bool MouseExists();

// Sets the mouse sensitivity in the range [1, 5].
void SetSensitivity(int value);

void SetPrimaryButtonRight(bool right);

}  // namespace mouse_settings

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_INPUT_DEVICE_SETTINGS_H_
