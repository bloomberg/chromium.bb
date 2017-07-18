// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"

namespace gpu {

class GpuChannelManagerTest : public GpuChannelTestCommon {
 public:
  GpuChannelManagerTest() : GpuChannelTestCommon() {}
  ~GpuChannelManagerTest() override {}

#if defined(OS_ANDROID)
  void TestOnApplicationStateChange(gles2::ContextType type,
                                    bool should_clear_stub) {
    ASSERT_TRUE(channel_manager());
    int32_t kClientId = 1;
    GpuChannel* channel = CreateChannel(kClientId, true);
    EXPECT_TRUE(channel);

    int32_t kRouteId = 1;
    const SurfaceHandle kFakeSurfaceHandle = 1;
    SurfaceHandle surface_handle = kFakeSurfaceHandle;
    GPUCreateCommandBufferConfig init_params;
    init_params.surface_handle = surface_handle;
    init_params.share_group_id = MSG_ROUTING_NONE;
    init_params.stream_id = 0;
    init_params.stream_priority = SchedulingPriority::kNormal;
    init_params.attribs = gles2::ContextCreationAttribHelper();
    init_params.attribs.context_type = type;
    init_params.active_url = GURL();
    bool result = false;
    gpu::Capabilities capabilities;
    HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                               init_params, kRouteId, GetSharedHandle(),
                               &result, &capabilities));
    EXPECT_TRUE(result);

    GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
    EXPECT_TRUE(stub);

    channel_manager()->is_backgrounded_for_testing_ = true;
    channel_manager()->OnApplicationBackgrounded();
    stub = channel->LookupCommandBuffer(kRouteId);
    if (should_clear_stub) {
      EXPECT_FALSE(stub);
    } else {
      EXPECT_TRUE(stub);
    }
  }
#endif
};

TEST_F(GpuChannelManagerTest, EstablishChannel) {
  int32_t kClientId = 1;
  uint64_t kClientTracingId = 1;

  ASSERT_TRUE(channel_manager());
  GpuChannel* channel =
      channel_manager()->EstablishChannel(kClientId, kClientTracingId, false);
  EXPECT_TRUE(channel);
  EXPECT_EQ(channel_manager()->LookupChannel(kClientId), channel);
}

#if defined(OS_ANDROID)
TEST_F(GpuChannelManagerTest, OnLowEndBackgroundedWithoutWebGL) {
  channel_manager()->set_low_end_mode_for_testing(true);
  TestOnApplicationStateChange(gles2::CONTEXT_TYPE_OPENGLES2, true);
}

TEST_F(GpuChannelManagerTest, OnLowEndBackgroundedWithWebGL) {
  channel_manager()->set_low_end_mode_for_testing(true);
  TestOnApplicationStateChange(gles2::CONTEXT_TYPE_WEBGL2, false);
}

TEST_F(GpuChannelManagerTest, OnHighEndBackgrounded) {
  channel_manager()->set_low_end_mode_for_testing(false);
  TestOnApplicationStateChange(gles2::CONTEXT_TYPE_OPENGLES2, false);
}
#endif

}  // namespace gpu
