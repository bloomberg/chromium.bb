// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the ShareGroup.

#include "gpu/command_buffer/client/share_group.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {
namespace gles2 {

class ShareGroupTest : public testing::Test {
 protected:

  virtual void SetUp() {
    share_group_ = ShareGroup::Ref(new ShareGroup(false, false));
  }

  virtual void TearDown() {
  }

  scoped_refptr<ShareGroup> share_group_;
};

TEST_F(ShareGroupTest, Basic) {
  EXPECT_TRUE(ShareGroup::ImplementsThreadSafeReferenceCounting());
}

}  // namespace gles2
}  // namespace gpu


