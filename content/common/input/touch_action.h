// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_TOUCH_ACTION_H_
#define CONTENT_COMMON_INPUT_TOUCH_ACTION_H_

namespace content {

// The current touch action specifies what accelerated browser operations
// (panning and zooming) are currently permitted via touch input.
// See http://www.w3.org/TR/pointerevents/#the-touch-action-css-property.
enum TouchAction {
  // No scrolling or zooming allowed.
  TOUCH_ACTION_NONE,

  // All actions are pemitted (the default).
  TOUCH_ACTION_AUTO,

  TOUCH_ACTION_MAX = TOUCH_ACTION_AUTO
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_TOUCH_ACTION_H_
