// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "content/common/gpu/establish_channel_params.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_channel_test_common.h"
#include "gpu/command_buffer/common/value_state.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"

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

  EstablishChannelParams params;
  params.client_id = kClientId;
  params.client_tracing_id = kClientTracingId;
  params.preempts = false;
  params.allow_view_command_buffers = false;
  params.allow_real_time_streams = false;
  IPC::ChannelHandle channel_handle =
      channel_manager()->EstablishChannel(params);
  EXPECT_NE("", channel_handle.name);

  GpuChannel* channel = channel_manager()->LookupChannel(kClientId);
  ASSERT_TRUE(channel);
  EXPECT_EQ(channel_handle.name, channel->channel_id());
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
  EstablishChannelParams params;
  params.client_id = kClientId1;
  params.client_tracing_id = kClientTracingId1;
  params.preempts = false;
  params.allow_view_command_buffers = false;
  params.allow_real_time_streams = false;
  channel_manager()->EstablishChannel(params);
  GpuChannel* channel1 = channel_manager()->LookupChannel(kClientId1);
  ASSERT_TRUE(channel1);

  params.client_id = kClientId2;
  params.client_tracing_id = kClientTracingId2;
  channel_manager()->EstablishChannel(params);
  GpuChannel* channel2 = channel_manager()->LookupChannel(kClientId2);
  ASSERT_TRUE(channel2);

  EXPECT_NE(channel1, channel2);

  // Make sure value states are only accessible by proper channels
  channel_manager()->UpdateValueState(kClientId1, GL_MOUSE_POSITION_CHROMIUM,
                                      value_state1);
  channel_manager()->UpdateValueState(kClientId2, GL_MOUSE_POSITION_CHROMIUM,
                                      value_state2);

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
