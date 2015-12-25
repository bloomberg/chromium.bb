// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/test/test_simple_task_runner.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_channel_test_common.h"
#include "content/common/gpu/gpu_messages.h"
#include "ipc/ipc_test_sink.h"

namespace content {

class GpuChannelTest : public GpuChannelTestCommon {
 public:
  GpuChannelTest() : GpuChannelTestCommon() {}
  ~GpuChannelTest() override {}

  GpuChannel* CreateChannel(int32_t client_id, bool allow_real_time_streams) {
    DCHECK(channel_manager());
    uint64_t kClientTracingId = 1;
    GpuMsg_EstablishChannel_Params params;
    params.client_id = client_id;
    params.client_tracing_id = kClientTracingId;
    params.preempts = false;
    params.preempted = false;
    params.allow_future_sync_points = false;
    params.allow_real_time_streams = allow_real_time_streams;
    EXPECT_TRUE(
        channel_manager()->OnMessageReceived(GpuMsg_EstablishChannel(params)));
    return channel_manager()->LookupChannel(client_id);
  }
};

TEST_F(GpuChannelTest, CreateViewCommandBuffer) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, false);
  ASSERT_TRUE(channel);

  gfx::GLSurfaceHandle surface_handle;
  int32_t kRouteId = 1;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = 0;
  init_params.stream_priority = GpuStreamPriority::NORMAL;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      surface_handle, kClientId, init_params, kRouteId));

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

TEST_F(GpuChannelTest, IncompatibleStreamIds) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, false);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId1 = 1;
  int32_t kStreamId1 = 1;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId1;
  init_params.stream_priority = GpuStreamPriority::NORMAL;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kClientId, init_params, kRouteId1));

  const IPC::Message* msg =
      sink()->GetUniqueMessageMatching(GpuHostMsg_CommandBufferCreated::ID);
  ASSERT_TRUE(msg);

  base::Tuple<CreateCommandBufferResult> result;
  ASSERT_TRUE(GpuHostMsg_CommandBufferCreated::Read(msg, &result));

  EXPECT_EQ(CREATE_COMMAND_BUFFER_SUCCEEDED, base::get<0>(result));

  sink()->ClearMessages();

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId1);
  ASSERT_TRUE(stub);

  // Create second context in same share group but different stream.
  int32_t kRouteId2 = 2;
  int32_t kStreamId2 = 2;

  init_params.share_group_id = kRouteId1;
  init_params.stream_id = kStreamId2;
  init_params.stream_priority = GpuStreamPriority::NORMAL;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kClientId, init_params, kRouteId2));

  msg = sink()->GetUniqueMessageMatching(GpuHostMsg_CommandBufferCreated::ID);
  ASSERT_TRUE(msg);

  ASSERT_TRUE(GpuHostMsg_CommandBufferCreated::Read(msg, &result));

  EXPECT_EQ(CREATE_COMMAND_BUFFER_FAILED, base::get<0>(result));

  sink()->ClearMessages();

  stub = channel->LookupCommandBuffer(kRouteId2);
  ASSERT_FALSE(stub);
}

TEST_F(GpuChannelTest, IncompatibleStreamPriorities) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, false);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId1 = 1;
  int32_t kStreamId1 = 1;
  GpuStreamPriority kStreamPriority1 = GpuStreamPriority::NORMAL;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId1;
  init_params.stream_priority = kStreamPriority1;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kClientId, init_params, kRouteId1));

  const IPC::Message* msg =
      sink()->GetUniqueMessageMatching(GpuHostMsg_CommandBufferCreated::ID);
  ASSERT_TRUE(msg);

  base::Tuple<CreateCommandBufferResult> result;
  ASSERT_TRUE(GpuHostMsg_CommandBufferCreated::Read(msg, &result));

  EXPECT_EQ(CREATE_COMMAND_BUFFER_SUCCEEDED, base::get<0>(result));

  sink()->ClearMessages();

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId1);
  ASSERT_TRUE(stub);

  // Create second context in same share group but different stream.
  int32_t kRouteId2 = 2;
  int32_t kStreamId2 = kStreamId1;
  GpuStreamPriority kStreamPriority2 = GpuStreamPriority::LOW;

  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId2;
  init_params.stream_priority = kStreamPriority2;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kClientId, init_params, kRouteId2));

  msg = sink()->GetUniqueMessageMatching(GpuHostMsg_CommandBufferCreated::ID);
  ASSERT_TRUE(msg);

  ASSERT_TRUE(GpuHostMsg_CommandBufferCreated::Read(msg, &result));

  EXPECT_EQ(CREATE_COMMAND_BUFFER_FAILED, base::get<0>(result));

  sink()->ClearMessages();

  stub = channel->LookupCommandBuffer(kRouteId2);
  ASSERT_FALSE(stub);
}

TEST_F(GpuChannelTest, StreamLifetime) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, false);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId1 = 1;
  int32_t kStreamId1 = 1;
  GpuStreamPriority kStreamPriority1 = GpuStreamPriority::NORMAL;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId1;
  init_params.stream_priority = kStreamPriority1;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kClientId, init_params, kRouteId1));

  const IPC::Message* msg =
      sink()->GetUniqueMessageMatching(GpuHostMsg_CommandBufferCreated::ID);
  ASSERT_TRUE(msg);

  base::Tuple<CreateCommandBufferResult> result;
  ASSERT_TRUE(GpuHostMsg_CommandBufferCreated::Read(msg, &result));

  EXPECT_EQ(CREATE_COMMAND_BUFFER_SUCCEEDED, base::get<0>(result));

  sink()->ClearMessages();

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId1);
  ASSERT_TRUE(stub);

  {
    // GpuChannelHost always calls set_unblock(false) on messages sent to the
    // GPU process.
    IPC::Message m = GpuChannelMsg_DestroyCommandBuffer(kRouteId1);
    m.set_unblock(false);
    EXPECT_TRUE(channel->filter()->OnMessageReceived(m));
    task_runner()->RunPendingTasks();
  }

  stub = channel->LookupCommandBuffer(kRouteId1);
  ASSERT_FALSE(stub);

  // Create second context in same share group but different stream.
  int32_t kRouteId2 = 2;
  int32_t kStreamId2 = 2;
  GpuStreamPriority kStreamPriority2 = GpuStreamPriority::LOW;

  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId2;
  init_params.stream_priority = kStreamPriority2;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kClientId, init_params, kRouteId2));

  msg = sink()->GetUniqueMessageMatching(GpuHostMsg_CommandBufferCreated::ID);
  ASSERT_TRUE(msg);

  ASSERT_TRUE(GpuHostMsg_CommandBufferCreated::Read(msg, &result));

  EXPECT_EQ(CREATE_COMMAND_BUFFER_SUCCEEDED, base::get<0>(result));

  sink()->ClearMessages();

  stub = channel->LookupCommandBuffer(kRouteId2);
  ASSERT_TRUE(stub);
}

TEST_F(GpuChannelTest, RealTimeStreamsDisallowed) {
  int32_t kClientId = 1;
  bool allow_real_time_streams = false;
  GpuChannel* channel = CreateChannel(kClientId, allow_real_time_streams);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId = 1;
  int32_t kStreamId = 1;
  GpuStreamPriority kStreamPriority = GpuStreamPriority::REAL_TIME;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId;
  init_params.stream_priority = kStreamPriority;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kClientId, init_params, kRouteId));

  const IPC::Message* msg =
      sink()->GetUniqueMessageMatching(GpuHostMsg_CommandBufferCreated::ID);
  ASSERT_TRUE(msg);

  base::Tuple<CreateCommandBufferResult> result;
  ASSERT_TRUE(GpuHostMsg_CommandBufferCreated::Read(msg, &result));

  EXPECT_EQ(CREATE_COMMAND_BUFFER_FAILED, base::get<0>(result));

  sink()->ClearMessages();

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  ASSERT_FALSE(stub);
}

TEST_F(GpuChannelTest, RealTimeStreamsAllowed) {
  int32_t kClientId = 1;
  bool allow_real_time_streams = true;
  GpuChannel* channel = CreateChannel(kClientId, allow_real_time_streams);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId = 1;
  int32_t kStreamId = 1;
  GpuStreamPriority kStreamPriority = GpuStreamPriority::REAL_TIME;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId;
  init_params.stream_priority = kStreamPriority;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kClientId, init_params, kRouteId));

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
