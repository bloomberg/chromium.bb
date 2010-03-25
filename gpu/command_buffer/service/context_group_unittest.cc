// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_group.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class ContextGroupTest : public testing::Test {
 public:
  ContextGroupTest() {
  }

 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  ContextGroup group_;
};

TEST_F(ContextGroupTest, Basic) {
  // Test it starts off uninitialized.
  EXPECT_EQ(0u, group_.max_vertex_attribs());
  EXPECT_EQ(0u, group_.max_texture_units());
  EXPECT_TRUE(group_.id_manager() == NULL);
  EXPECT_TRUE(group_.buffer_manager() == NULL);
  EXPECT_TRUE(group_.framebuffer_manager() == NULL);
  EXPECT_TRUE(group_.renderbuffer_manager() == NULL);
  EXPECT_TRUE(group_.texture_manager() == NULL);
  EXPECT_TRUE(group_.program_manager() == NULL);
  EXPECT_TRUE(group_.shader_manager() == NULL);
}

}  // namespace gles2
}  // namespace gpu


