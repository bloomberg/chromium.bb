// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_DELEGATE_H_
#define CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_DELEGATE_H_

#include <memory>

#include "content/common/content_export.h"
#include "content/common/input/input_event_ack.h"
#include "third_party/WebKit/public/platform/WebInputEventResult.h"

namespace blink {
class WebGestureEvent;
class WebMouseEvent;
}

namespace gfx {
class Point;
class Vector2dF;
}

namespace content {

class RenderWidgetInputHandler;

// Consumers of RenderWidgetInputHandler implement this delegate in order to
// transport input handling information across processes.
class CONTENT_EXPORT RenderWidgetInputHandlerDelegate {
 public:
  // Called when animations due to focus change have completed (if any).
  virtual void FocusChangeComplete() = 0;

  // Check whether the WebWidget has any touch event handlers registered
  // at the given point.
  virtual bool HasTouchEventHandlersAt(const gfx::Point& point) const = 0;

  // Called to forward a gesture event to the compositor thread, to effect
  // the elastic overscroll effect.
  virtual void ObserveGestureEventAndResult(
      const blink::WebGestureEvent& gesture_event,
      const gfx::Vector2dF& unused_delta,
      bool event_processed) = 0;

  // Notifies that a key event was just handled.
  virtual void OnDidHandleKeyEvent() = 0;

  // Notifies that an overscroll was completed from Blink.
  virtual void OnDidOverscroll(const ui::DidOverscrollParams& params) = 0;

  // Called when an ACK is ready to be sent to the input event provider.
  virtual void OnInputEventAck(
      std::unique_ptr<InputEventAck> input_event_ack) = 0;

  // Called when an event with a notify dispatch type
  // (DISPATCH_TYPE_*_NOTIFY_MAIN) of |handled_type| has been processed
  // by the main thread.
  virtual void NotifyInputEventHandled(blink::WebInputEvent::Type handled_type,
                                       blink::WebInputEventResult result,
                                       InputEventAckState ack_result) = 0;

  // Notifies the delegate of the |input_handler| managing it.
  virtual void SetInputHandler(RenderWidgetInputHandler* input_handler) = 0;

  // Call this if virtual keyboard should be displayed, e.g. after a tap on an
  // input field on mobile. Note that we do this just by setting a bit in
  // text input state and update it to the browser such that browser process can
  // be up to date when showing virtual keyboard.
  virtual void ShowVirtualKeyboard() = 0;

  // Send an update of text input state to the browser process.
  virtual void UpdateTextInputState() = 0;

  // Clear the text input state.
  virtual void ClearTextInputState() = 0;

  // Notifies that a gesture event is about to be handled.
  // Returns true if no further handling is needed. In that case, the event
  // won't be sent to WebKit.
  virtual bool WillHandleGestureEvent(const blink::WebGestureEvent& event) = 0;

  // Notifies that a mouse event is about to be handled.
  // Returns true if no further handling is needed. In that case, the event
  // won't be sent to WebKit or trigger DidHandleMouseEvent().
  virtual bool WillHandleMouseEvent(const blink::WebMouseEvent& event) = 0;

 protected:
  virtual ~RenderWidgetInputHandlerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_DELEGATE_H_
