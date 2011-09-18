// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Tabs API and the Windows API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_OFFSCREEN_TABS_MODULE_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_OFFSCREEN_TABS_MODULE_CONSTANTS_H_
#pragma once

namespace extension_offscreen_tabs_module_constants {

// TODO(alexbost): Some of these should be refactored

// offscreen tab keys
extern const char kIdKey[];
extern const char kUrlKey[];
extern const char kWidthKey[];
extern const char kHeightKey[];

// toDataUrl keys
extern const char kFormatKey[];
extern const char kQualityKey[];

// mouse keys
extern const char kMouseEventTypeKey[];
extern const char kMouseEventButtonKey[];
extern const char kMouseEventWheelDeltaXKey[];
extern const char kMouseEventWheelDeltaYKey[];
extern const char kMouseEventAltKeyKey[];
extern const char kMouseEventCtrlKeyKey[];
extern const char kMouseEventMetaKeyKey[];
extern const char kMouseEventShiftKeyKey[];

// toDataUrl values
extern const char kFormatValueJpeg[];
extern const char kFormatValuePng[];
extern const char kMimeTypeJpeg[];
extern const char kMimeTypePng[];

// mouse values
extern const char kMouseEventTypeValueMousedown[];
extern const char kMouseEventTypeValueMouseup[];
extern const char kMouseEventTypeValueClick[];
extern const char kMouseEventTypeValueMousemove[];
extern const char kMouseEventTypeValueMousewheel[];
extern const int kMouseEventButtonValueLeft;
extern const int kMouseEventButtonValueMiddle;
extern const int kMouseEventButtonValueRight;

// keyboard keys
extern const char kKeyboardEventTypeKey[];
extern const char kKeyboardEventCharCodeKey[];
extern const char kKeyboardEventKeyCodeKey[];
extern const char kKeyboardEventAltKeyKey[];
extern const char kKeyboardEventCtrlKeyKey[];
extern const char kKeyboardEventShiftKeyKey[];

// keyboard values
extern const char kKeyboardEventTypeValueKeypress[];
extern const char kKeyboardEventTypeValueKeydown[];
extern const char kKeyboardEventTypeValueKeyup[];

// events
extern const char kDispatchEvent[];
extern const char kEventOnUpdated[];

// errors
extern const char kCurrentTabNotFound[];
extern const char kInternalVisibleTabCaptureError[];
extern const char kInvalidKeyboardEventObjectError[];
extern const char kInvalidMouseEventObjectError[];
extern const char kInvalidUrlError[];
extern const char kNoCrashBrowserError[];
extern const char kNoCurrentWindowError[];
extern const char kNoMouseCoordinatesError[];
extern const char kOffscreenTabNotFoundError[];
extern const char kTabNotFoundError[];

};  // namespace extension_offscreen_tabs_module_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_OFFSCREEN_TABS_MODULE_CONSTANTS_H_

