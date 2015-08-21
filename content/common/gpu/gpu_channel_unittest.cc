// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_channel_test_common.h"
#include "content/common/gpu/gpu_messages.h"
#include "ipc/ipc_test_sink.h"

namespace content {

class GpuChannelTest : public GpuChannelTestCommon {
 public:
  GpuChannelTest() : GpuChannelTestCommon() {}
};

TEST_F(GpuChannelTest, CreateViewCommandBuffer) {
  const int kClientId = 1;
  const uint64 kClientTracingId = 1;

  ASSERT_TRUE(channel_manager());

  EXPECT_TRUE(channel_manager()->OnMessageReceived(
      GpuMsg_EstablishChannel(kClientId, kClientTracingId, false, false)));
  GpuChannel* channel = channel_manager()->LookupChannel(kClientId);
  ASSERT_TRUE(channel);

  gfx::GLSurfaceHandle surface_handle;
  const int kSurfaceId = 1;
  const int kRouteId = 1;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      surface_handle, kSurfaceId, kClientId, init_params, kRouteId));

  const IPC::Message* msg =
      sink()->GetUniqueMessageMatching(GpuHostMsg_CommandBufferCreated::ID);
  ASSERT_TRUE(msg);

  base::Tuple<CreateCommandBufferResult> result;
  ASSERT_TRUE(GpuHostMsg_CommandBufferCreated::Read(msg, &result));

  EXPECT_EQ(CREATE_COMMAND_BUFFER_SUCCEEDED, base::get<0>(result));

  sink()->ClearMessages();

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  ASSERT_TRUE(stub);
}

}  // namespace content
