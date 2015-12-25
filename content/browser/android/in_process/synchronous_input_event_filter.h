// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_INPUT_EVENT_FILTER_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_INPUT_EVENT_FILTER_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/renderer/input/input_handler_manager_client.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {
class WebInputEvent;
}

namespace ui {
class SynchronousInputHandlerProxy;
}

namespace content {

// This class perform synchronous, in-process InputEvent handling.
//
// The provided |handler| process WebInputEvents synchronously on the merged
// UI and compositing thread. If the event goes unhandled, that is reflected in
// the InputEventAckState; no forwarding is performed.
class SynchronousInputEventFilter : public InputHandlerManagerClient {
 public:
  SynchronousInputEventFilter();
  ~SynchronousInputEventFilter() override;

  InputEventAckState HandleInputEvent(int routing_id,
                                      const blink::WebInputEvent& input_event);

  // InputHandlerManagerClient implementation.
  void SetBoundHandler(const Handler& handler) override;
  void DidAddInputHandler(
      int routing_id,
      ui::SynchronousInputHandlerProxy*
          synchronous_input_handler_proxy) override;
  void DidRemoveInputHandler(int routing_id) override;
  void DidOverscroll(int routing_id,
                     const DidOverscrollParams& params) override;
  void DidStopFlinging(int routing_id) override;

 private:
  void SetBoundHandlerOnUIThread(const Handler& handler);

  Handler handler_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_INPUT_EVENT_FILTER_H_
