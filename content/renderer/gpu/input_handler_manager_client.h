// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_INPUT_HANDLER_MANAGER_CLIENT_H_
#define CONTENT_RENDERER_GPU_INPUT_HANDLER_MANAGER_CLIENT_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "ui/gfx/vector2d_f.h"

namespace ui {
struct LatencyInfo;
}

namespace cc {
class InputHandler;
struct DidOverscrollParams;
}

namespace WebKit {
class WebInputEvent;
}

namespace content {

class CONTENT_EXPORT InputHandlerManagerClient {
 public:
  virtual ~InputHandlerManagerClient() {}

  // The Manager will supply a |handler| when bound to the client. This is valid
  // until the manager shuts down, at which point it supplies a null |handler|.
  // The client should only makes calls to |handler| on the compositor thread.
  typedef base::Callback<
      InputEventAckState(int /*routing_id*/,
                         const WebKit::WebInputEvent*,
                         const ui::LatencyInfo& latency_info)> Handler;

  // Called from the main thread.
  virtual void SetBoundHandler(const Handler& handler) = 0;

  // Called from the compositor thread.
  virtual void DidAddInputHandler(int routing_id,
                                  cc::InputHandler* input_handler) = 0;
  virtual void DidRemoveInputHandler(int routing_id) = 0;
  virtual void DidOverscroll(int routing_id,
                             const cc::DidOverscrollParams& params) = 0;

 protected:
  InputHandlerManagerClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InputHandlerManagerClient);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_INPUT_HANDLER_MANAGER_CLIENT_H_
