// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class RenderbufferManagerTest : public testing::Test {
 public:
  RenderbufferManagerTest() {
  }

 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  RenderbufferManager manager_;
};

TEST_F(RenderbufferManagerTest, Basic) {
  const GLuint kRenderbuffer1Id = 1;
  const GLuint kRenderbuffer2Id = 2;
  // Check we can create renderbuffer.
  manager_.CreateRenderbufferInfo(kRenderbuffer1Id);
  // Check renderbuffer got created.
  RenderbufferManager::RenderbufferInfo* info1 =
      manager_.GetRenderbufferInfo(kRenderbuffer1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_FALSE(info1->cleared());
  info1->set_cleared();
  EXPECT_TRUE(info1->cleared());
  EXPECT_FALSE(info1->IsDeleted());
  EXPECT_EQ(kRenderbuffer1Id, info1->renderbuffer_id());
  // Check we get nothing for a non-existent renderbuffer.
  EXPECT_TRUE(manager_.GetRenderbufferInfo(kRenderbuffer2Id) == NULL);
  // Check trying to a remove non-existent renderbuffers does not crash.
  manager_.RemoveRenderbufferInfo(kRenderbuffer2Id);
  // Check we can't get the renderbuffer after we remove it.
  manager_.RemoveRenderbufferInfo(kRenderbuffer1Id);
  EXPECT_TRUE(manager_.GetRenderbufferInfo(kRenderbuffer1Id) == NULL);
}

}  // namespace gles2
}  // namespace gpu


