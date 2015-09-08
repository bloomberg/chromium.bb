// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
};

TEST_F(GpuChannelTest, CreateViewCommandBuffer) {
  int32 kClientId = 1;
  uint64 kClientTracingId = 1;

  ASSERT_TRUE(channel_manager());

  EXPECT_TRUE(channel_manager()->OnMessageReceived(GpuMsg_EstablishChannel(
      kClientId, kClientTracingId, false, false, false)));
  GpuChannel* channel = channel_manager()->LookupChannel(kClientId);
  ASSERT_TRUE(channel);

  gfx::GLSurfaceHandle surface_handle;
  int32 kSurfaceId = 1;
  int32 kRouteId = 1;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = 0;
  init_params.stream_priority = GpuStreamPriority::NORMAL;
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

TEST_F(GpuChannelTest, IncompatibleStreamIds) {
  int32 kClientId = 1;
  uint64 kClientTracingId = 1;

  ASSERT_TRUE(channel_manager());

  EXPECT_TRUE(channel_manager()->OnMessageReceived(GpuMsg_EstablishChannel(
      kClientId, kClientTracingId, false, false, false)));
  GpuChannel* channel = channel_manager()->LookupChannel(kClientId);
  ASSERT_TRUE(channel);

  // Create first context.
  int32 kSurfaceId1 = 1;
  int32 kRouteId1 = 1;
  int32 kStreamId1 = 1;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId1;
  init_params.stream_priority = GpuStreamPriority::NORMAL;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kSurfaceId1, kClientId, init_params, kRouteId1));

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
  int32 kSurfaceId2 = 2;
  int32 kRouteId2 = 2;
  int32 kStreamId2 = 2;

  init_params.share_group_id = kRouteId1;
  init_params.stream_id = kStreamId2;
  init_params.stream_priority = GpuStreamPriority::NORMAL;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kSurfaceId2, kClientId, init_params, kRouteId2));

  msg = sink()->GetUniqueMessageMatching(GpuHostMsg_CommandBufferCreated::ID);
  ASSERT_TRUE(msg);

  ASSERT_TRUE(GpuHostMsg_CommandBufferCreated::Read(msg, &result));

  EXPECT_EQ(CREATE_COMMAND_BUFFER_FAILED, base::get<0>(result));

  sink()->ClearMessages();

  stub = channel->LookupCommandBuffer(kRouteId2);
  ASSERT_FALSE(stub);
}

TEST_F(GpuChannelTest, IncompatibleStreamPriorities) {
  int32 kClientId = 1;
  uint64 kClientTracingId = 1;

  ASSERT_TRUE(channel_manager());

  EXPECT_TRUE(channel_manager()->OnMessageReceived(GpuMsg_EstablishChannel(
      kClientId, kClientTracingId, false, false, false)));
  GpuChannel* channel = channel_manager()->LookupChannel(kClientId);
  ASSERT_TRUE(channel);

  // Create first context.
  int32 kSurfaceId1 = 1;
  int32 kRouteId1 = 1;
  int32 kStreamId1 = 1;
  GpuStreamPriority kStreamPriority1 = GpuStreamPriority::NORMAL;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId1;
  init_params.stream_priority = kStreamPriority1;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kSurfaceId1, kClientId, init_params, kRouteId1));

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
  int32 kSurfaceId2 = 2;
  int32 kRouteId2 = 2;
  int32 kStreamId2 = kStreamId1;
  GpuStreamPriority kStreamPriority2 = GpuStreamPriority::LOW;

  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId2;
  init_params.stream_priority = kStreamPriority2;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kSurfaceId2, kClientId, init_params, kRouteId2));

  msg = sink()->GetUniqueMessageMatching(GpuHostMsg_CommandBufferCreated::ID);
  ASSERT_TRUE(msg);

  ASSERT_TRUE(GpuHostMsg_CommandBufferCreated::Read(msg, &result));

  EXPECT_EQ(CREATE_COMMAND_BUFFER_FAILED, base::get<0>(result));

  sink()->ClearMessages();

  stub = channel->LookupCommandBuffer(kRouteId2);
  ASSERT_FALSE(stub);
}

TEST_F(GpuChannelTest, StreamLifetime) {
  int32 kClientId = 1;
  uint64 kClientTracingId = 1;

  ASSERT_TRUE(channel_manager());

  EXPECT_TRUE(channel_manager()->OnMessageReceived(GpuMsg_EstablishChannel(
      kClientId, kClientTracingId, false, false, false)));
  GpuChannel* channel = channel_manager()->LookupChannel(kClientId);
  ASSERT_TRUE(channel);

  // Create first context.
  int32 kSurfaceId1 = 1;
  int32 kRouteId1 = 1;
  int32 kStreamId1 = 1;
  GpuStreamPriority kStreamPriority1 = GpuStreamPriority::NORMAL;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId1;
  init_params.stream_priority = kStreamPriority1;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kSurfaceId1, kClientId, init_params, kRouteId1));

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
  int32 kSurfaceId2 = 2;
  int32 kRouteId2 = 2;
  int32 kStreamId2 = 2;
  GpuStreamPriority kStreamPriority2 = GpuStreamPriority::LOW;

  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId2;
  init_params.stream_priority = kStreamPriority2;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kSurfaceId2, kClientId, init_params, kRouteId2));

  msg = sink()->GetUniqueMessageMatching(GpuHostMsg_CommandBufferCreated::ID);
  ASSERT_TRUE(msg);

  ASSERT_TRUE(GpuHostMsg_CommandBufferCreated::Read(msg, &result));

  EXPECT_EQ(CREATE_COMMAND_BUFFER_SUCCEEDED, base::get<0>(result));

  sink()->ClearMessages();

  stub = channel->LookupCommandBuffer(kRouteId2);
  ASSERT_TRUE(stub);
}

TEST_F(GpuChannelTest, RealTimeStreamsDisallowed) {
  int32 kClientId = 1;
  uint64 kClientTracingId = 1;
  bool allow_real_time_streams = false;

  ASSERT_TRUE(channel_manager());

  EXPECT_TRUE(channel_manager()->OnMessageReceived(GpuMsg_EstablishChannel(
      kClientId, kClientTracingId, false, false, allow_real_time_streams)));
  GpuChannel* channel = channel_manager()->LookupChannel(kClientId);
  ASSERT_TRUE(channel);

  // Create first context.
  int32 kSurfaceId = 1;
  int32 kRouteId = 1;
  int32 kStreamId = 1;
  GpuStreamPriority kStreamPriority = GpuStreamPriority::REAL_TIME;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId;
  init_params.stream_priority = kStreamPriority;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kSurfaceId, kClientId, init_params, kRouteId));

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
  int32 kClientId = 1;
  uint64 kClientTracingId = 1;

  ASSERT_TRUE(channel_manager());

  bool allow_real_time_streams = true;
  EXPECT_TRUE(channel_manager()->OnMessageReceived(GpuMsg_EstablishChannel(
      kClientId, kClientTracingId, false, false, allow_real_time_streams)));
  GpuChannel* channel = channel_manager()->LookupChannel(kClientId);
  ASSERT_TRUE(channel);

  // Create first context.
  int32 kSurfaceId = 1;
  int32 kRouteId = 1;
  int32 kStreamId = 1;
  GpuStreamPriority kStreamPriority = GpuStreamPriority::REAL_TIME;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId;
  init_params.stream_priority = kStreamPriority;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  channel_manager()->OnMessageReceived(GpuMsg_CreateViewCommandBuffer(
      gfx::GLSurfaceHandle(), kSurfaceId, kClientId, init_params, kRouteId));

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
