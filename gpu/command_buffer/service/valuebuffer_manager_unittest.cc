// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/valuebuffer_manager.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_mock.h"

namespace gpu {
namespace gles2 {

class ValuebufferManagerTest : public GpuServiceTest {
 public:
  ValuebufferManagerTest() : manager_() {}
  ~ValuebufferManagerTest() override { manager_.Destroy(); }

 protected:
  ValuebufferManager manager_;
};

TEST_F(ValuebufferManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kClient2Id = 2;
  // Check we can create a Valuebuffer
  manager_.CreateValuebuffer(kClient1Id);
  Valuebuffer* valuebuffer0 = manager_.GetValuebuffer(kClient1Id);
  ASSERT_TRUE(valuebuffer0 != NULL);
  EXPECT_EQ(kClient1Id, valuebuffer0->client_id());
  // Check we get nothing for a non-existent Valuebuffer.
  // Check trying to a remove non-existent Valuebuffer does not crash
  manager_.RemoveValuebuffer(kClient2Id);
  // Check we can't get the renderbuffer after we remove it.
  manager_.RemoveValuebuffer(kClient1Id);
  EXPECT_TRUE(manager_.GetValuebuffer(kClient1Id) == NULL);
}

TEST_F(ValuebufferManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  // Check we can create Valuebuffer.
  manager_.CreateValuebuffer(kClient1Id);
  Valuebuffer* valuebuffer0 = manager_.GetValuebuffer(kClient1Id);
  ASSERT_TRUE(valuebuffer0 != NULL);
  EXPECT_EQ(kClient1Id, valuebuffer0->client_id());
  manager_.Destroy();
  // Check the resources were released.
  Valuebuffer* valuebuffer1 = manager_.GetValuebuffer(kClient1Id);
  ASSERT_TRUE(valuebuffer1 == NULL);
}

TEST_F(ValuebufferManagerTest, ValueBuffer) {
  const GLuint kClient1Id = 1;
  // Check we can create a Valuebuffer
  manager_.CreateValuebuffer(kClient1Id);
  Valuebuffer* valuebuffer0 = manager_.GetValuebuffer(kClient1Id);
  ASSERT_TRUE(valuebuffer0 != NULL);
  EXPECT_EQ(kClient1Id, valuebuffer0->client_id());
  EXPECT_FALSE(valuebuffer0->IsValid());
}

TEST_F(ValuebufferManagerTest, UpdateState) {
  const GLuint kClient1Id = 1;
  ValueState valuestate1;
  valuestate1.int_value[0] = 111;
  ValueState valuestate2;
  valuestate2.int_value[0] = 222;
  manager_.CreateValuebuffer(kClient1Id);
  Valuebuffer* valuebuffer0 = manager_.GetValuebuffer(kClient1Id);
  ASSERT_TRUE(valuebuffer0 != NULL);
  EXPECT_EQ(kClient1Id, valuebuffer0->client_id());
  valuebuffer0->AddSubscription(GL_MOUSE_POSITION_CHROMIUM);
  ASSERT_TRUE(valuebuffer0->GetState(GL_MOUSE_POSITION_CHROMIUM) == NULL);
  manager_.UpdateValueState(GL_MOUSE_POSITION_CHROMIUM, valuestate1);
  manager_.UpdateValuebufferState(valuebuffer0);
  const ValueState* new_state1 =
      valuebuffer0->GetState(GL_MOUSE_POSITION_CHROMIUM);
  ASSERT_TRUE(new_state1 != NULL);
  ASSERT_TRUE(new_state1->int_value[0] == 111);
  // Ensure state changes
  manager_.UpdateValueState(GL_MOUSE_POSITION_CHROMIUM, valuestate2);
  manager_.UpdateValuebufferState(valuebuffer0);
  const ValueState* new_state2 =
      valuebuffer0->GetState(GL_MOUSE_POSITION_CHROMIUM);
  ASSERT_TRUE(new_state2 != NULL);
  ASSERT_TRUE(new_state2->int_value[0] == 222);
}

}  // namespace gles2
}  // namespace gpu
