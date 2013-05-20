// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_INPUT_HANDLER_PROXY_CLIENT_H_
#define CONTENT_RENDERER_GPU_INPUT_HANDLER_PROXY_CLIENT_H_

namespace WebKit {
class WebGestureCurve;
struct WebActiveWheelFlingParameters;
struct WebFloatPoint;
struct WebSize;
}

namespace content {

// All callbacks invoked from the compositor thread.
class InputHandlerProxyClient {
 public:
  // Called just before the InputHandlerProxy shuts down.
  virtual void WillShutdown() = 0;

  // Exactly one of the following two callbacks will be invoked after every
  // call to InputHandlerProxy::HandleInputEvent():

  // Called when the InputHandlerProxy handled the input event and no
  // further processing is required.
  virtual void DidHandleInputEvent() = 0;

  // Called when the InputHandlerProxy did not handle the input event.
  // If send_to_widget is true, the input event should be forwarded to the
  // WebWidget associated with this compositor for further processing.
  virtual void DidNotHandleInputEvent(bool send_to_widget) = 0;

  // Transfers an active wheel fling animation initiated by a previously
  // handled input event out to the client.
  virtual void TransferActiveWheelFlingAnimation(
      const WebKit::WebActiveWheelFlingParameters& params) = 0;

  // Creates a new fling animation curve instance for device |device_source|
  // with |velocity| and already scrolled |cumulative_scroll| pixels.
  virtual WebKit::WebGestureCurve* CreateFlingAnimationCurve(
      int device_source,
      const WebKit::WebFloatPoint& velocity,
      const WebKit::WebSize& cumulative_scroll) = 0;

 protected:
  virtual ~InputHandlerProxyClient() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_INPUT_HANDLER_PROXY_CLIENT_H_
