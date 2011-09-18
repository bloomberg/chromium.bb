// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_offscreen_tabs_module_constants.h"

namespace extension_offscreen_tabs_module_constants {

// offscreen tab keys
const char kIdKey[] = "id";
const char kUrlKey[] = "url";
const char kWidthKey[] = "width";
const char kHeightKey[] = "height";

// toDataUrl keys
const char kFormatKey[] = "format";
const char kQualityKey[] = "quality";

// mouse keys
const char kMouseEventTypeKey[] = "type";
const char kMouseEventButtonKey[] = "button";
const char kMouseEventWheelDeltaXKey[] = "wheelDeltaX";
const char kMouseEventWheelDeltaYKey[] = "wheelDeltaY";
const char kMouseEventAltKeyKey[] = "altKey";
const char kMouseEventCtrlKeyKey[] = "ctrlKey";
const char kMouseEventMetaKeyKey[] = "metaKey";
const char kMouseEventShiftKeyKey[] = "shiftKey";

// toDataUrl values
const char kFormatValueJpeg[] = "jpeg";
const char kFormatValuePng[] = "png";
const char kMimeTypeJpeg[] = "image/jpeg";
const char kMimeTypePng[] = "image/png";

// mouse values
const char kMouseEventTypeValueMousedown[] = "mousedown";
const char kMouseEventTypeValueMouseup[] = "mouseup";
const char kMouseEventTypeValueClick[] = "click";
const char kMouseEventTypeValueMousemove[] = "mousemove";
const char kMouseEventTypeValueMousewheel[] = "mousewheel";
const int kMouseEventButtonValueLeft = 0;
const int kMouseEventButtonValueMiddle = 1;
const int kMouseEventButtonValueRight = 2;

// keyboard keys
const char kKeyboardEventTypeKey[] = "type";
const char kKeyboardEventCharCodeKey[] = "charCode";
const char kKeyboardEventKeyCodeKey[] = "keyCode";
const char kKeyboardEventAltKeyKey[] = "altKey";
const char kKeyboardEventCtrlKeyKey[] = "ctrlKey";
const char kKeyboardEventShiftKeyKey[] = "shiftKey";

// keyboard values
const char kKeyboardEventTypeValueKeypress[] = "keypress";
const char kKeyboardEventTypeValueKeydown[] = "keydown";
const char kKeyboardEventTypeValueKeyup[] = "keyup";

// events
const char kDispatchEvent[] = "Event.dispatchJSON";
const char kEventOnUpdated[] = "experimental.offscreenTabs.onUpdated";

// errors
const char kCurrentTabNotFound[]  = "No current tab found";
const char kInternalVisibleTabCaptureError[] =
    "Internal error while trying to capture visible region of the current tab";
const char kInvalidKeyboardEventObjectError[] =
    "Invalid or unexpected KeyboardEvent object";
const char kInvalidMouseEventObjectError[] =
    "Invalid or enexpected MouseEvent object";
const char kInvalidUrlError[] = "Invalid url: \"*\".";
const char kNoCrashBrowserError[] = "I'm sorry. I'm afraid I can't do that.";
const char kNoCurrentWindowError[] = "No current window";
const char kNoMouseCoordinatesError[] = "No mouse coordinates specified";
const char kOffscreenTabNotFoundError[] = "No offscreen tab with id: *.";
const char kTabNotFoundError[] = "No tab with id: *.";

}  // namespace extension_offscreen_tabs_module_constants

