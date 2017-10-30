// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_HOST_FRAME_SINK_CLIENT_H_
#define COMPONENTS_VIZ_TEST_FAKE_HOST_FRAME_SINK_CLIENT_H_

#include "base/macros.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/host_frame_sink_client.h"

namespace viz {

// HostFrameSinkClient implementation that does nothing.
class FakeHostFrameSinkClient : public HostFrameSinkClient {
 public:
  FakeHostFrameSinkClient();
  ~FakeHostFrameSinkClient() override;

  // HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeHostFrameSinkClient);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_HOST_FRAME_SINK_CLIENT_H_
