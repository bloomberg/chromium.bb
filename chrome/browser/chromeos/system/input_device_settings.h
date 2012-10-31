// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_INPUT_DEVICE_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_INPUT_DEVICE_SETTINGS_H_

#include "base/callback.h"

namespace chromeos {
namespace system {

typedef base::Callback<void(bool)> DeviceExistsCallback;

namespace touchpad_settings {

// Calls |callback| asynchronously after determining if a touchpad is connected.
void TouchpadExists(const DeviceExistsCallback& callback);

// Sets the touchpad sensitivity in the range [1, 5].
void SetSensitivity(int value);

// Turns tap to click on/off.
void SetTapToClick(bool enabled);

// Switch for three-finger click.
void SetThreeFingerClick(bool enabled);

// Switch for three-finger swipe.
void SetThreeFingerSwipe(bool enabled);

// Turns tap-dragging on/off.
void SetTapDragging(bool enabled);

}  // namespace touchpad_settings

namespace mouse_settings {

// Calls |callback| asynchronously after determining if a mouse is connected.
void MouseExists(const DeviceExistsCallback& callback);

// Sets the mouse sensitivity in the range [1, 5].
void SetSensitivity(int value);

// Sets the primary mouse button to the right button if |right| is true.
void SetPrimaryButtonRight(bool right);

}  // namespace mouse_settings

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_INPUT_DEVICE_SETTINGS_H_
