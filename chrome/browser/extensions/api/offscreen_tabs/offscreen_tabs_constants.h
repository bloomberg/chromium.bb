// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Offscreen Tabs API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_OFFSCREEN_TABS_OFFSCREEN_TABS_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_API_OFFSCREEN_TABS_OFFSCREEN_TABS_CONSTANTS_H_
#pragma once

namespace extensions {
namespace offscreen_tabs_constants {

// Input event keys.
extern const char kEventTypeKey[];
extern const char kEventAltKeyKey[];
extern const char kEventCtrlKeyKey[];
extern const char kEventMetaKeyKey[];
extern const char kEventShiftKeyKey[];

// Mouse event keys.
extern const char kMouseEventButtonKey[];
extern const char kMouseEventPositionXKey[];
extern const char kMouseEventPositionYKey[];
extern const char kMouseEventWheelDeltaXKey[];
extern const char kMouseEventWheelDeltaYKey[];

// Mouse event values.
extern const char kMouseEventTypeValueMousedown[];
extern const char kMouseEventTypeValueMouseup[];
extern const char kMouseEventTypeValueClick[];
extern const char kMouseEventTypeValueMousemove[];
extern const char kMouseEventTypeValueMousewheel[];
extern const int kMouseEventButtonValueLeft;
extern const int kMouseEventButtonValueMiddle;
extern const int kMouseEventButtonValueRight;

// Keyboard event keys.
extern const char kKeyboardEventCharCodeKey[];
extern const char kKeyboardEventKeyCodeKey[];

// Keyboard event values.
extern const char kKeyboardEventTypeValueKeypress[];
extern const char kKeyboardEventTypeValueKeydown[];
extern const char kKeyboardEventTypeValueKeyup[];

// Errors.
extern const char kCurrentTabNotFound[];
extern const char kInvalidKeyboardEventObjectError[];
extern const char kInvalidMouseEventObjectError[];
extern const char kNoMouseCoordinatesError[];
extern const char kOffscreenTabNotFoundError[];

}  // namespace offscreen_tabs_constants
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_OFFSCREEN_TABS_OFFSCREEN_TABS_CONSTANTS_H_
