// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_IN_FLIGHT_FRAME_DELIVERY_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_IN_FLIGHT_FRAME_DELIVERY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// Represents an in-flight frame delivery to the consumer. Its main purpose is
// to proxy callbacks from the consumer back to the relevant capturer
// components owned and operated by FrameSinkVideoCapturerImpl.
//
// TODO(crbug.com/754872): This will extend a mojo-generated interface in an
// upcoming change.
class VIZ_SERVICE_EXPORT InFlightFrameDelivery {
 public:
  InFlightFrameDelivery(base::OnceClosure post_delivery_callback,
                        base::OnceCallback<void(double)> feedback_callback);

  virtual ~InFlightFrameDelivery();

  // TODO(crbug.com/754872): mojom::FrameSinkVideoFrameCallbacks implementation:
  void Done();
  void ProvideFeedback(double utilization);

 private:
  base::OnceClosure post_delivery_callback_;
  base::OnceCallback<void(double)> feedback_callback_;

  DISALLOW_COPY_AND_ASSIGN(InFlightFrameDelivery);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_IN_FLIGHT_FRAME_DELIVERY_H_
