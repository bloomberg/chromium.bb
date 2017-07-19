// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_CLIENT_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_CLIENT_H_

#include "components/viz/service/viz_service_export.h"

namespace cc {
class BeginFrameSource;
}

namespace viz {

class VIZ_SERVICE_EXPORT FrameSinkManagerClient {
 public:
  virtual ~FrameSinkManagerClient() = default;

  // This allows the FrameSinkManagerImpl to pass a BeginFrameSource to use.
  virtual void SetBeginFrameSource(
      cc::BeginFrameSource* begin_frame_source) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_CLIENT_H_
