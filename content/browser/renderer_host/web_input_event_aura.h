// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_WEB_INPUT_EVENT_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_WEB_INPUT_EVENT_AURA_H_
#pragma once

#include "content/common/content_export.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

namespace aura {
class GestureEvent;
class KeyEvent;
class MouseEvent;
class ScrollEvent;
class TouchEvent;
}

namespace content {

CONTENT_EXPORT WebKit::WebMouseEvent MakeWebMouseEvent(
    aura::MouseEvent* event);
CONTENT_EXPORT WebKit::WebMouseWheelEvent MakeWebMouseWheelEvent(
    aura::MouseEvent* event);
CONTENT_EXPORT WebKit::WebMouseWheelEvent MakeWebMouseWheelEvent(
    aura::ScrollEvent* event);
CONTENT_EXPORT WebKit::WebKeyboardEvent MakeWebKeyboardEvent(
    aura::KeyEvent* event);
CONTENT_EXPORT WebKit::WebGestureEvent MakeWebGestureEvent(
    aura::GestureEvent* event);
CONTENT_EXPORT WebKit::WebGestureEvent MakeWebGestureEvent(
    aura::ScrollEvent* event);

// Updates the WebTouchEvent based on the TouchEvent. It returns the updated
// WebTouchPoint contained in the WebTouchEvent, or NULL if no point was
// updated.
WebKit::WebTouchPoint* UpdateWebTouchEvent(aura::TouchEvent* event,
                                           WebKit::WebTouchEvent* web_event);

}

#endif  // CONTENT_BROWSER_RENDERER_HOST_WEB_INPUT_EVENT_AURA_H_
