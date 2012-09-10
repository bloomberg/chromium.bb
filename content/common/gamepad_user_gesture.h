// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GAMEPAD_USER_GESTURE_H_
#define CONTENT_COMMON_GAMEPAD_USER_GESTURE_H_

namespace WebKit {
class WebGamepads;
}

namespace content {

// Returns true if any of the gamepads have a button pressed that would be
// considerd a user gesture for interaction.
bool GamepadsHaveUserGesture(const WebKit::WebGamepads& gamepads);

}  // namespace content

#endif  // CONTENT_COMMON_GAMEPAD_USER_GESTURE_H_
