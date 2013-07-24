// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_ANDROID_H_

#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class WebMouseEventBuilder {
 public:
  static WebKit::WebMouseEvent Build(WebKit::WebInputEvent::Type type,
                                     WebKit::WebMouseEvent::Button button,
                                     double time_sec,
                                     int window_x,
                                     int window_y,
                                     int modifiers,
                                     int click_count);
};

class WebMouseWheelEventBuilder {
 public:
  enum Direction {
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_RIGHT,
  };

  static WebKit::WebMouseWheelEvent Build(Direction direction,
                                          double time_sec,
                                          int window_x,
                                          int window_y);
};

class WebKeyboardEventBuilder {
 public:
  static WebKit::WebKeyboardEvent Build(WebKit::WebInputEvent::Type type,
                                        int modifiers,
                                        double time_sec,
                                        int keycode,
                                        int unicode_character,
                                        bool is_system_key);
};

class WebGestureEventBuilder {
 public:
  static WebKit::WebGestureEvent Build(WebKit::WebInputEvent::Type type,
                                       double time_sec,
                                       int x,
                                       int y);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_ANDROID_H_
