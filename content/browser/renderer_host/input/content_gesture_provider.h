// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_CONTENT_GESTURE_PROVIDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_CONTENT_GESTURE_PROVIDER_H_

#include "base/basictypes.h"
#include "content/port/common/input_event_ack_state.h"
#include "ui/events/gesture_detection/gesture_event_data_packet.h"
#include "ui/events/gesture_detection/gesture_provider.h"
#include "ui/events/gesture_detection/touch_disposition_gesture_filter.h"

namespace blink {
class WebGestureEvent;
}

namespace content {

class ContentGestureProviderClient {
 public:
  virtual ~ContentGestureProviderClient() {}
  virtual void OnGestureEvent(const blink::WebGestureEvent& event) = 0;
};

// Provides gesture detection and dispatch given a sequence of touch events
// and touch event acks.
class ContentGestureProvider : public ui::TouchDispositionGestureFilterClient,
                               public ui::GestureProviderClient {
 public:
  // TODO(jdduke): Move the scaling constant out of this class, instead hosting
  // it on the generating MotionEvent.
  ContentGestureProvider(ContentGestureProviderClient* client,
                         float touch_to_gesture_scale);

  // Returns true if |event| was both valid and successfully handled by the
  // gesture detector.  Otherwise, returns false, in which case the caller
  // should drop |event|, not forwarding it to the renderer.
  bool OnTouchEvent(const ui::MotionEvent& event);

  // To be called upon ack of an event that was forwarded after a successful
  // call to |OnTouchEvent()|.
  void OnTouchEventAck(InputEventAckState ack_state);

  // Methods delegated to |gesture_provider_|.
  void ResetGestureDetectors();
  void SetMultiTouchSupportEnabled(bool enabled);
  void SetDoubleTapSupportForPlatformEnabled(bool enabled);
  void SetDoubleTapSupportForPageEnabled(bool enabled);
  const ui::MotionEvent* GetCurrentDownEvent() const;

 private:
  // ui::GestureProviderClient
  virtual void OnGestureEvent(const ui::GestureEventData& event) OVERRIDE;

  // TouchDispositionGestureFilterClient
  virtual void ForwardGestureEvent(const ui::GestureEventData& event) OVERRIDE;

  ContentGestureProviderClient* const client_;
  float touch_to_gesture_scale_;

  ui::GestureProvider gesture_provider_;
  ui::TouchDispositionGestureFilter gesture_filter_;

  bool handling_event_;
  ui::GestureEventDataPacket pending_gesture_packet_;

  DISALLOW_COPY_AND_ASSIGN(ContentGestureProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_CONTENT_GESTURE_PROVIDER_H_
