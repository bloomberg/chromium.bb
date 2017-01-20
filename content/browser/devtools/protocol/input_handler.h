// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/input.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/public/browser/render_widget_host.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {
class CompositorFrameMetadata;
}

namespace content {

class DevToolsSession;
class RenderFrameHostImpl;

namespace protocol {

class InputHandler : public DevToolsDomainHandler,
                     public Input::Backend,
                     public RenderWidgetHost::InputEventObserver {
 public:
  InputHandler();
  ~InputHandler() override;

  static InputHandler* FromSession(DevToolsSession* session);

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderFrameHost(RenderFrameHostImpl* host) override;
  void OnSwapCompositorFrame(const cc::CompositorFrameMetadata& frame_metadata);
  Response Disable() override;

  void DispatchKeyEvent(
      const std::string& type,
      Maybe<int> modifiers,
      Maybe<double> timestamp,
      Maybe<std::string> text,
      Maybe<std::string> unmodified_text,
      Maybe<std::string> key_identifier,
      Maybe<std::string> code,
      Maybe<std::string> key,
      Maybe<int> windows_virtual_key_code,
      Maybe<int> native_virtual_key_code,
      Maybe<bool> auto_repeat,
      Maybe<bool> is_keypad,
      Maybe<bool> is_system_key,
      std::unique_ptr<DispatchKeyEventCallback> callback) override;

  void DispatchMouseEvent(
      const std::string& type,
      int x,
      int y,
      Maybe<int> modifiers,
      Maybe<double> timestamp,
      Maybe<std::string> button,
      Maybe<int> click_count,
      std::unique_ptr<DispatchMouseEventCallback> callback) override;

  Response EmulateTouchFromMouseEvent(const std::string& type,
                                      int x,
                                      int y,
                                      double timestamp,
                                      const std::string& button,
                                      Maybe<double> delta_x,
                                      Maybe<double> delta_y,
                                      Maybe<int> modifiers,
                                      Maybe<int> click_count) override;

  void SynthesizePinchGesture(
      int x,
      int y,
      double scale_factor,
      Maybe<int> relative_speed,
      Maybe<std::string> gesture_source_type,
      std::unique_ptr<SynthesizePinchGestureCallback> callback) override;

  void SynthesizeScrollGesture(
      int x,
      int y,
      Maybe<int> x_distance,
      Maybe<int> y_distance,
      Maybe<int> x_overscroll,
      Maybe<int> y_overscroll,
      Maybe<bool> prevent_fling,
      Maybe<int> speed,
      Maybe<std::string> gesture_source_type,
      Maybe<int> repeat_count,
      Maybe<int> repeat_delay_ms,
      Maybe<std::string> interaction_marker_name,
      std::unique_ptr<SynthesizeScrollGestureCallback> callback) override;

  void SynthesizeTapGesture(
      int x,
      int y,
      Maybe<int> duration,
      Maybe<int> tap_count,
      Maybe<std::string> gesture_source_type,
      std::unique_ptr<SynthesizeTapGestureCallback> callback) override;

 private:
  // InputEventObserver
  void OnInputEvent(const blink::WebInputEvent& event) override;
  void OnInputEventAck(const blink::WebInputEvent& event) override;

  void SynthesizeRepeatingScroll(
      SyntheticSmoothScrollGestureParams gesture_params,
      int repeat_count,
      base::TimeDelta repeat_delay,
      std::string interaction_marker_name,
      int id,
      std::unique_ptr<SynthesizeScrollGestureCallback> callback);

  void OnScrollFinished(
      SyntheticSmoothScrollGestureParams gesture_params,
      int repeat_count,
      base::TimeDelta repeat_delay,
      std::string interaction_marker_name,
      int id,
      std::unique_ptr<SynthesizeScrollGestureCallback> callback,
      SyntheticGesture::Result result);

  void ClearPendingKeyAndMouseCallbacks();

  RenderFrameHostImpl* host_;
  // Callbacks for calls to Input.dispatchKey/MouseEvent that have been sent to
  // the renderer, but that we haven't yet received an ack for.
  bool input_queued_;
  std::deque<std::unique_ptr<DispatchKeyEventCallback>> pending_key_callbacks_;
  std::deque<std::unique_ptr<DispatchMouseEventCallback>>
      pending_mouse_callbacks_;
  float page_scale_factor_;
  gfx::SizeF scrollable_viewport_size_;
  int last_id_;
  base::WeakPtrFactory<InputHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
