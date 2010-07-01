// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_url_index.h"

#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

class InMemoryURLIndexTest : public testing::Test {
 protected:
  scoped_ptr<InMemoryURLIndex> url_index_;
};

TEST_F(InMemoryURLIndexTest, Construction) {
  url_index_.reset(new InMemoryURLIndex);
  EXPECT_TRUE(url_index_.get());
}

}  // namespace history
