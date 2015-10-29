// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_TOUCH_HANDLER_H_
#define COMPONENTS_HTML_VIEWER_TOUCH_HANDLER_H_

#include "base/basictypes.h"
#include "components/mus/public/interfaces/input_events.mojom.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"

namespace blink {
class WebWidget;
}

namespace ui {
class MotionEventGeneric;
}

namespace html_viewer {

// TouchHandler is responsible for converting touch events into gesture events.
// It does this by converting mus::mojom::Events into a MotionEventGeneric and
// using
// FilteredGestureProvider.
class TouchHandler : public ui::GestureProviderClient {
 public:
  explicit TouchHandler(blink::WebWidget* web_widget);
  ~TouchHandler() override;

  void OnTouchEvent(const mus::mojom::Event& event);

  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;

 private:
  // Updates |current_motion_event_| from |event|. Returns true on success.
  bool UpdateMotionEvent(const mus::mojom::Event& event);

  // Sends |current_motion_event_| to the GestureProvider and WebView.
  void SendMotionEventToGestureProvider();

  // Does post processing after sending |current_motion_event_| to the
  // GestureProvider.
  void PostProcessMotionEvent(const mus::mojom::Event& event);

  blink::WebWidget* web_widget_;

  ui::FilteredGestureProvider gesture_provider_;

  // As touch events are received they are converted to this event. If null no
  // touch events are in progress.
  scoped_ptr<ui::MotionEventGeneric> current_motion_event_;

  DISALLOW_COPY_AND_ASSIGN(TouchHandler);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_TOUCH_HANDLER_H_
