// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_EVENT_PACKET_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_EVENT_PACKET_H_

#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

// Acts as a transport container for gestures created (directly or indirectly)
// by a touch event.
class CONTENT_EXPORT GestureEventPacket {
 public:
  enum GestureSource {
    UNDEFINED = -1,        // Used only for a default-constructed packet.
    INVALID,               // The source of the gesture was invalid.
    TOUCH_SEQUENCE_BEGIN,  // The start of a new gesture sequence.
    TOUCH_SEQUENCE_END,    // The end of gesture sequence.
    TOUCH_BEGIN,           // A touch down occured during a gesture sequence.
    TOUCH_MOVE,            // A touch move occured during a gesture sequence.
    TOUCH_END,             // A touch up occured during a gesture sequence.
    TOUCH_TIMEOUT,         // Timeout from an existing gesture sequence.
  };

  GestureEventPacket();
  GestureEventPacket(const GestureEventPacket& other);
  ~GestureEventPacket();
  GestureEventPacket& operator=(const GestureEventPacket& other);

  // Factory methods for creating a packet from a particular event.
  static GestureEventPacket FromTouch(const blink::WebTouchEvent& event);
  static GestureEventPacket FromTouchTimeout(
      const blink::WebGestureEvent& event);

  void Push(const blink::WebGestureEvent& gesture);

  const blink::WebGestureEvent& gesture(size_t i) const { return gestures_[i]; }
  size_t gesture_count() const { return gesture_count_; }
  GestureSource gesture_source() const { return gesture_source_; }

private:
  explicit GestureEventPacket(GestureSource source);

  enum { kMaxGesturesPerTouch = 5 };
  blink::WebGestureEvent gestures_[kMaxGesturesPerTouch];
  size_t gesture_count_;
  GestureSource gesture_source_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_EVENT_PACKET_H_
