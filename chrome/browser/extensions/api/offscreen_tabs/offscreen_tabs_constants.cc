// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/offscreen_tabs/offscreen_tabs_constants.h"

namespace extensions {
namespace offscreen_tabs_constants {

const char kEventTypeKey[] = "type";
const char kEventAltKeyKey[] = "altKey";
const char kEventCtrlKeyKey[] = "ctrlKey";
const char kEventMetaKeyKey[] = "metaKey";
const char kEventShiftKeyKey[] = "shiftKey";

const char kMouseEventButtonKey[] = "button";
const char kMouseEventPositionXKey[] = "x";
const char kMouseEventPositionYKey[] = "y";
const char kMouseEventWheelDeltaXKey[] = "wheelDeltaX";
const char kMouseEventWheelDeltaYKey[] = "wheelDeltaY";

const char kMouseEventTypeValueMousedown[] = "mousedown";
const char kMouseEventTypeValueMouseup[] = "mouseup";
const char kMouseEventTypeValueClick[] = "click";
const char kMouseEventTypeValueMousemove[] = "mousemove";
const char kMouseEventTypeValueMousewheel[] = "mousewheel";
const int kMouseEventButtonValueLeft = 0;
const int kMouseEventButtonValueMiddle = 1;
const int kMouseEventButtonValueRight = 2;

const char kKeyboardEventCharCodeKey[] = "charCode";
const char kKeyboardEventKeyCodeKey[] = "keyCode";

const char kKeyboardEventTypeValueKeypress[] = "keypress";
const char kKeyboardEventTypeValueKeydown[] = "keydown";
const char kKeyboardEventTypeValueKeyup[] = "keyup";

const char kCurrentTabNotFound[]  = "No current tab found";
const char kInvalidKeyboardEventObjectError[] =
    "Invalid or unexpected KeyboardEvent object";
const char kInvalidMouseEventObjectError[] =
    "Invalid or unexpected MouseEvent object";
const char kNoMouseCoordinatesError[] = "No mouse coordinates specified";
const char kOffscreenTabNotFoundError[] = "No offscreen tab with id: *.";

}  // namespace offscreen_Tabs_constants
}  // namespace extensions
