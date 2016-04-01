// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/test/test_simple_task_runner.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_channel_test_common.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "ipc/ipc_test_sink.h"

namespace content {

class GpuChannelTest : public GpuChannelTestCommon {
 public:
  GpuChannelTest() : GpuChannelTestCommon() {}
  ~GpuChannelTest() override {}

  GpuChannel* CreateChannel(int32_t client_id,
                            bool allow_view_command_buffers,
                            bool allow_real_time_streams) {
    DCHECK(channel_manager());
    uint64_t kClientTracingId = 1;
    channel_manager()->EstablishChannel(
        client_id, kClientTracingId, false /* preempts */,
        allow_view_command_buffers, allow_real_time_streams);
    return channel_manager()->LookupChannel(client_id);
  }

  void HandleMessage(GpuChannel* channel, IPC::Message* msg) {
    TestGpuChannel* test_channel = static_cast<TestGpuChannel*>(channel);
    test_channel->HandleMessageForTesting(*msg);

    if (msg->is_sync()) {
      const IPC::Message* reply_msg = test_channel->sink()->GetMessageAt(0);
      ASSERT_TRUE(reply_msg);

      EXPECT_TRUE(IPC::SyncMessage::IsMessageReplyTo(
          *reply_msg, IPC::SyncMessage::GetMessageId(*msg)));

      IPC::MessageReplyDeserializer* deserializer =
          static_cast<IPC::SyncMessage*>(msg)->GetReplyDeserializer();
      ASSERT_TRUE(deserializer);
      deserializer->SerializeOutputParameters(*reply_msg);

      delete deserializer;

      test_channel->sink()->ClearMessages();
    }

    delete msg;
  }
};

#if defined(OS_WIN)
const gpu::SurfaceHandle kFakeSurfaceHandle =
    reinterpret_cast<gpu::SurfaceHandle>(1);
#else
const gpu::SurfaceHandle kFakeSurfaceHandle = 1;
#endif

TEST_F(GpuChannelTest, CreateViewCommandBufferAllowed) {
  int32_t kClientId = 1;
  bool allow_view_command_buffers = true;
  GpuChannel* channel =
      CreateChannel(kClientId, allow_view_command_buffers, false);
  ASSERT_TRUE(channel);

  gpu::SurfaceHandle surface_handle = kFakeSurfaceHandle;
  DCHECK_NE(surface_handle, gpu::kNullSurfaceHandle);

  int32_t kRouteId = 1;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = 0;
  init_params.stream_priority = gpu::GpuStreamPriority::NORMAL;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  bool succeeded = false;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             surface_handle, gfx::Size(), init_params, kRouteId,
                             &succeeded));
  EXPECT_TRUE(succeeded);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  ASSERT_TRUE(stub);
}

TEST_F(GpuChannelTest, CreateViewCommandBufferDisallowed) {
  int32_t kClientId = 1;
  bool allow_view_command_buffers = false;
  GpuChannel* channel =
      CreateChannel(kClientId, allow_view_command_buffers, false);
  ASSERT_TRUE(channel);

  gpu::SurfaceHandle surface_handle = kFakeSurfaceHandle;
  DCHECK_NE(surface_handle, gpu::kNullSurfaceHandle);

  int32_t kRouteId = 1;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = 0;
  init_params.stream_priority = gpu::GpuStreamPriority::NORMAL;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  bool succeeded = false;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             surface_handle, gfx::Size(), init_params, kRouteId,
                             &succeeded));
  EXPECT_FALSE(succeeded);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  EXPECT_FALSE(stub);
}

TEST_F(GpuChannelTest, CreateOffscreenCommandBuffer) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true, false);
  ASSERT_TRUE(channel);

  int32_t kRouteId = 1;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = 0;
  init_params.stream_priority = gpu::GpuStreamPriority::NORMAL;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  bool succeeded = false;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             gpu::kNullSurfaceHandle, gfx::Size(1, 1),
                             init_params, kRouteId, &succeeded));
  EXPECT_TRUE(succeeded);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  EXPECT_TRUE(stub);
}

TEST_F(GpuChannelTest, IncompatibleStreamIds) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true, false);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId1 = 1;
  int32_t kStreamId1 = 1;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId1;
  init_params.stream_priority = gpu::GpuStreamPriority::NORMAL;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  bool succeeded = false;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             gpu::kNullSurfaceHandle, gfx::Size(1, 1),
                             init_params, kRouteId1, &succeeded));
  EXPECT_TRUE(succeeded);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId1);
  EXPECT_TRUE(stub);

  // Create second context in same share group but different stream.
  int32_t kRouteId2 = 2;
  int32_t kStreamId2 = 2;

  init_params.share_group_id = kRouteId1;
  init_params.stream_id = kStreamId2;
  init_params.stream_priority = gpu::GpuStreamPriority::NORMAL;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  succeeded = false;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             gpu::kNullSurfaceHandle, gfx::Size(1, 1),
                             init_params, kRouteId2, &succeeded));
  EXPECT_FALSE(succeeded);

  stub = channel->LookupCommandBuffer(kRouteId2);
  EXPECT_FALSE(stub);
}

TEST_F(GpuChannelTest, StreamLifetime) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true, false);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId1 = 1;
  int32_t kStreamId1 = 1;
  gpu::GpuStreamPriority kStreamPriority1 = gpu::GpuStreamPriority::NORMAL;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId1;
  init_params.stream_priority = kStreamPriority1;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  bool succeeded = false;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             gpu::kNullSurfaceHandle, gfx::Size(1, 1),
                             init_params, kRouteId1, &succeeded));
  EXPECT_TRUE(succeeded);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId1);
  EXPECT_TRUE(stub);

  HandleMessage(channel, new GpuChannelMsg_DestroyCommandBuffer(kRouteId1));
  stub = channel->LookupCommandBuffer(kRouteId1);
  EXPECT_FALSE(stub);

  // Create second context in same share group but different stream.
  int32_t kRouteId2 = 2;
  int32_t kStreamId2 = 2;
  gpu::GpuStreamPriority kStreamPriority2 = gpu::GpuStreamPriority::LOW;

  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId2;
  init_params.stream_priority = kStreamPriority2;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  succeeded = false;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             gpu::kNullSurfaceHandle, gfx::Size(1, 1),
                             init_params, kRouteId2, &succeeded));
  EXPECT_TRUE(succeeded);

  stub = channel->LookupCommandBuffer(kRouteId2);
  EXPECT_TRUE(stub);
}

TEST_F(GpuChannelTest, RealTimeStreamsDisallowed) {
  int32_t kClientId = 1;
  bool allow_real_time_streams = false;
  GpuChannel* channel = CreateChannel(kClientId, true, allow_real_time_streams);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId = 1;
  int32_t kStreamId = 1;
  gpu::GpuStreamPriority kStreamPriority = gpu::GpuStreamPriority::REAL_TIME;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId;
  init_params.stream_priority = kStreamPriority;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  bool succeeded = false;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             gpu::kNullSurfaceHandle, gfx::Size(1, 1),
                             init_params, kRouteId, &succeeded));
  EXPECT_FALSE(succeeded);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  EXPECT_FALSE(stub);
}

TEST_F(GpuChannelTest, RealTimeStreamsAllowed) {
  int32_t kClientId = 1;
  bool allow_real_time_streams = true;
  GpuChannel* channel = CreateChannel(kClientId, true, allow_real_time_streams);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId = 1;
  int32_t kStreamId = 1;
  gpu::GpuStreamPriority kStreamPriority = gpu::GpuStreamPriority::REAL_TIME;
  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id = MSG_ROUTING_NONE;
  init_params.stream_id = kStreamId;
  init_params.stream_priority = kStreamPriority;
  init_params.attribs = std::vector<int>();
  init_params.active_url = GURL();
  init_params.gpu_preference = gfx::PreferIntegratedGpu;
  bool succeeded = false;
  HandleMessage(channel, new GpuChannelMsg_CreateCommandBuffer(
                             gpu::kNullSurfaceHandle, gfx::Size(1, 1),
                             init_params, kRouteId, &succeeded));
  EXPECT_TRUE(succeeded);

  GpuCommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  EXPECT_TRUE(stub);
}

}  // namespace content
