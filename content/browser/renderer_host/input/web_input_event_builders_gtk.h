// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_GTK_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_GTK_H_

#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/keycodes/keyboard_codes.h"

typedef struct _GdkEventButton GdkEventButton;
typedef struct _GdkEventMotion GdkEventMotion;
typedef struct _GdkEventCrossing GdkEventCrossing;
typedef struct _GdkEventScroll GdkEventScroll;
typedef struct _GdkEventKey GdkEventKey;

namespace content {

class CONTENT_EXPORT WebKeyboardEventBuilder {
 public:
  static WebKit::WebKeyboardEvent Build(const GdkEventKey* event);
  static WebKit::WebKeyboardEvent Build(wchar_t character,
                                        int state,
                                        double time_secs);
};

class CONTENT_EXPORT WebMouseEventBuilder {
 public:
  static WebKit::WebMouseEvent Build(const GdkEventButton* event);
  static WebKit::WebMouseEvent Build(const GdkEventMotion* event);
  static WebKit::WebMouseEvent Build(const GdkEventCrossing* event);
};

class CONTENT_EXPORT WebMouseWheelEventBuilder {
 public:
  static float ScrollbarPixelsPerTick();
  static WebKit::WebMouseWheelEvent Build(
      const GdkEventScroll* event);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_WEB_INPUT_EVENT_BUILDERS_GTK_H
