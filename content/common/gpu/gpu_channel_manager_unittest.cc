// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_channel_test_common.h"
#include "content/common/gpu/gpu_messages.h"
#include "gpu/command_buffer/common/value_state.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "ipc/ipc_test_sink.h"

using gpu::gles2::ValuebufferManager;
using gpu::ValueState;

namespace content {

class GpuChannelManagerTest : public GpuChannelTestCommon {
 public:
  GpuChannelManagerTest() : GpuChannelTestCommon() {}
  ~GpuChannelManagerTest() override {}
};

TEST_F(GpuChannelManagerTest, EstablishChannel) {
  int32_t kClientId = 1;
  uint64_t kClientTracingId = 1;

  ASSERT_TRUE(channel_manager());

  GpuMsg_EstablishChannel_Params params;
  params.client_id = kClientId;
  params.client_tracing_id = kClientTracingId;
  params.preempts = false;
  params.preempted = false;
  params.allow_future_sync_points = false;
  params.allow_real_time_streams = false;
  EXPECT_TRUE(
      channel_manager()->OnMessageReceived(GpuMsg_EstablishChannel(params)));
  EXPECT_EQ((size_t)1, sink()->message_count());
  const IPC::Message* msg =
      sink()->GetUniqueMessageMatching(GpuHostMsg_ChannelEstablished::ID);
  ASSERT_TRUE(msg);
  base::Tuple<IPC::ChannelHandle> handle;
  ASSERT_TRUE(GpuHostMsg_ChannelEstablished::Read(msg, &handle));
  EXPECT_NE("", base::get<0>(handle).name);
  sink()->ClearMessages();

  GpuChannel* channel = channel_manager()->LookupChannel(kClientId);
  ASSERT_TRUE(channel);
  EXPECT_EQ(base::get<0>(handle).name, channel->channel_id());
}

TEST_F(GpuChannelManagerTest, SecureValueStateForwarding) {
  int32_t kClientId1 = 111;
  uint64_t kClientTracingId1 = 11111;
  int32_t kClientId2 = 222;
  uint64_t kClientTracingId2 = 22222;
  ValueState value_state1;
  value_state1.int_value[0] = 1111;
  value_state1.int_value[1] = 0;
  value_state1.int_value[2] = 0;
  value_state1.int_value[3] = 0;
  ValueState value_state2;
  value_state2.int_value[0] = 3333;
  value_state2.int_value[1] = 0;
  value_state2.int_value[2] = 0;
  value_state2.int_value[3] = 0;

  ASSERT_TRUE(channel_manager());

  // Initialize gpu channels
  GpuMsg_EstablishChannel_Params params;
  params.client_id = kClientId1;
  params.client_tracing_id = kClientTracingId1;
  params.preempts = false;
  params.preempted = false;
  params.allow_future_sync_points = false;
  params.allow_real_time_streams = false;
  EXPECT_TRUE(
      channel_manager()->OnMessageReceived(GpuMsg_EstablishChannel(params)));
  GpuChannel* channel1 = channel_manager()->LookupChannel(kClientId1);
  ASSERT_TRUE(channel1);

  params.client_id = kClientId2;
  params.client_tracing_id = kClientTracingId2;
  EXPECT_TRUE(
      channel_manager()->OnMessageReceived(GpuMsg_EstablishChannel(params)));
  GpuChannel* channel2 = channel_manager()->LookupChannel(kClientId2);
  ASSERT_TRUE(channel2);

  EXPECT_NE(channel1, channel2);

  // Make sure value states are only accessible by proper channels
  channel_manager()->OnMessageReceived(GpuMsg_UpdateValueState(
      kClientId1, GL_MOUSE_POSITION_CHROMIUM, value_state1));
  channel_manager()->OnMessageReceived(GpuMsg_UpdateValueState(
      kClientId2, GL_MOUSE_POSITION_CHROMIUM, value_state2));

  const gpu::ValueStateMap* pending_value_buffer_state1 =
      channel1->pending_valuebuffer_state();
  const gpu::ValueStateMap* pending_value_buffer_state2 =
      channel2->pending_valuebuffer_state();
  EXPECT_NE(pending_value_buffer_state1, pending_value_buffer_state2);

  const ValueState* state1 =
      pending_value_buffer_state1->GetState(GL_MOUSE_POSITION_CHROMIUM);
  const ValueState* state2 =
      pending_value_buffer_state2->GetState(GL_MOUSE_POSITION_CHROMIUM);
  EXPECT_NE(state1, state2);

  EXPECT_EQ(state1->int_value[0], value_state1.int_value[0]);
  EXPECT_EQ(state2->int_value[0], value_state2.int_value[0]);
  EXPECT_NE(state1->int_value[0], state2->int_value[0]);
}

}  // namespace content
