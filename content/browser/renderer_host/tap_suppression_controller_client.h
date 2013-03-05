// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TAP_SUPPRESSION_CONTROLLER_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_TAP_SUPPRESSION_CONTROLLER_CLIENT_H_

namespace content {

// This class provides an interface for callbacks made by
// TapSuppressionController.
class TapSuppressionControllerClient {
 public:
  virtual ~TapSuppressionControllerClient() {}

  // Derived classes should implement this function to return the maximum time
  // they allow between a GestureFlingCancel and its corresponding tap down.
  virtual int MaxCancelToDownTimeInMs() = 0;

  // Derived classes should implement this function to return the maximum time
  // they allow between a single tap's down and up events.
  virtual int MaxTapGapTimeInMs() = 0;

  // Called whenever the deferred tap down (if saved) should be dropped totally.
  virtual void DropStashedTapDown() = 0;

  // Called whenever the deferred tap down (if saved) should be forwarded to the
  // renderer. In this case, the tap down should go back to normal path it was
  // on before being deferred.
  virtual void ForwardStashedTapDownForDeferral() = 0;

  // Called whenever the deferred tap down (if saved) should be forwarded to the
  // renderer. In this case, the tap down should skip deferral filter, because
  // it is handled here, and there is no need to delay it more.
  virtual void ForwardStashedTapDownSkipDeferral() = 0;

 protected:
  TapSuppressionControllerClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TapSuppressionControllerClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TAP_SUPPRESSION_CONTROLLER_CLIENT_H_
