// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_WIN_H_

#include <windows.h>

#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class WebKeyboardEventBuilder {
 public:
  static WebKit::WebKeyboardEvent Build(HWND hwnd, UINT message,
                                        WPARAM wparam, LPARAM lparam);
};

class WebMouseEventBuilder {
 public:
  static WebKit::WebMouseEvent Build(HWND hwnd, UINT message,
                                     WPARAM wparam, LPARAM lparam);
};

class WebMouseWheelEventBuilder {
 public:
  static WebKit::WebMouseWheelEvent Build(HWND hwnd, UINT message,
                                          WPARAM wparam, LPARAM lparam);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_WEB_INPUT_EVENT_BUILDERS_WIN_H_
