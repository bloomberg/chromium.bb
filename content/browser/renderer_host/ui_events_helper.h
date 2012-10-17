// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_UI_EVENTS_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_UI_EVENTS_HELPER_H_

#include "base/memory/scoped_vector.h"

namespace WebKit {
class WebTouchEvent;
}

namespace ui {
class TouchEvent;
}

namespace content {

// Creates a list of ui::TouchEvents out of a single WebTouchEvent.
// A WebTouchEvent can contain information about a number of WebTouchPoints,
// whereas a ui::TouchEvent contains information about a single touch-point. So
// it is possible to create more than one ui::TouchEvents out of a single
// WebTouchEvent.
bool MakeUITouchEventsFromWebTouchEvents(const WebKit::WebTouchEvent& touch,
                                         ScopedVector<ui::TouchEvent>* list);
}

#endif  // CONTENT_BROWSER_RENDERER_HOST_UI_EVENTS_HELPER_H_
