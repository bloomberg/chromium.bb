// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/input.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {
class CompositorFrameMetadata;
}

namespace content {

class RenderFrameHostImpl;

namespace protocol {

class InputHandler : public Input::Backend {
 public:
  InputHandler();
  ~InputHandler() override;

  void Wire(UberDispatcher*);
  void SetRenderFrameHost(RenderFrameHostImpl* host);
  void OnSwapCompositorFrame(const cc::CompositorFrameMetadata& frame_metadata);
  Response Disable() override;

  Response DispatchKeyEvent(const std::string& type,
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
                            Maybe<bool> is_system_key) override;

  Response DispatchMouseEvent(const std::string& type,
                              int x,
                              int y,
                              Maybe<int> modifiers,
                              Maybe<double> timestamp,
                              Maybe<std::string> button,
                              Maybe<int> click_count) override;

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

  RenderFrameHostImpl* host_;
  float page_scale_factor_;
  gfx::SizeF scrollable_viewport_size_;
  int last_id_;
  base::WeakPtrFactory<InputHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
