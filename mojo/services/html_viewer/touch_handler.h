// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_TOUCH_HANDLER_H_
#define MOJO_SERVICES_HTML_VIEWER_TOUCH_HANDLER_H_

#include "base/basictypes.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"

namespace blink {
class WebView;
}

namespace mojo {
class Event;
}

namespace ui {
class MotionEventGeneric;
}

namespace html_viewer {

// TouchHandler is responsible for converting touch events into gesture events.
// It does this by converting mojo::Events into a MotionEventGeneric and using
// FilteredGestureProvider.
class TouchHandler : public ui::GestureProviderClient {
 public:
  explicit TouchHandler(blink::WebView* web_view);
  ~TouchHandler() override;

  void OnTouchEvent(const mojo::Event& event);

  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;

 private:
  // Updates |current_motion_event_| from |event|. Returns true on success.
  bool UpdateMotionEvent(const mojo::Event& event);

  // Sends |current_motion_event_| to the GestureProvider and WebView.
  void SendMotionEventToGestureProvider();

  // Does post processing after sending |current_motion_event_| to the
  // GestureProvider.
  void PostProcessMotionEvent(const mojo::Event& event);

  blink::WebView* web_view_;

  ui::FilteredGestureProvider gesture_provider_;

  // As touch events are received they are converted to this event. If null no
  // touch events are in progress.
  scoped_ptr<ui::MotionEventGeneric> current_motion_event_;

  DISALLOW_COPY_AND_ASSIGN(TouchHandler);
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_TOUCH_HANDLER_H_
