// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_MOCK_COMPOSITOR_FRAME_SINK_SUPPORT_CLIENT_H_
#define COMPONENTS_VIZ_TEST_MOCK_COMPOSITOR_FRAME_SINK_SUPPORT_CLIENT_H_

#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace viz {
namespace test {

class MockCompositorFrameSinkSupportClient
    : public CompositorFrameSinkSupportClient {
 public:
  MockCompositorFrameSinkSupportClient();
  ~MockCompositorFrameSinkSupportClient() override;

  // CompositorFrameSinkSupportClient implementation.
  MOCK_METHOD1(DidReceiveCompositorFrameAck,
               void(const std::vector<ReturnedResource>&));
  MOCK_METHOD1(OnBeginFrame, void(const BeginFrameArgs&));
  MOCK_METHOD1(ReclaimResources, void(const std::vector<ReturnedResource>&));
  MOCK_METHOD2(WillDrawSurface, void(const LocalSurfaceId&, const gfx::Rect&));
  MOCK_METHOD1(OnBeginFramePausedChanged, void(bool paused));
};

}  // namespace test
}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_MOCK_COMPOSITOR_FRAME_SINK_SUPPORT_CLIENT_H_
