// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/browsing_data_partition_impl.h"

#include "base/memory/scoped_ptr.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {
namespace {

class BrowsingDataPartitionImplTest : public PlatformTest {
 private:
  // Used to create TestWebThreads.
  TestWebThreadBundle thread_bundle_;
};

}  // namespace

// Tests that a BrowsingDataPartitionImplTest is succesfully created with a
// BrowserState. And that it returns a non-nil BrowsingDataStore.
TEST_F(BrowsingDataPartitionImplTest, CreationAndBrowsingDataStore) {
  scoped_ptr<BrowserState> browser_state(new TestBrowserState());

  BrowsingDataPartition* browsing_data_partition =
      BrowserState::GetBrowsingDataPartition(browser_state.get());
  ASSERT_TRUE(browsing_data_partition);

  EXPECT_TRUE(browsing_data_partition->GetBrowsingDataStore());
}

}  // namespace web
