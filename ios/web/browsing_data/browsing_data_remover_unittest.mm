// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/browsing_data/browsing_data_remover.h"

#include "ios/web/public/test/fakes/test_browser_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

class BrowsingDataRemoverTest : public PlatformTest {
 protected:
  BrowsingDataRemover* GetRemover() {
    return BrowsingDataRemover::FromBrowserState(&browser_state_);
  }
  TestBrowserState browser_state_;
};

TEST_F(BrowsingDataRemoverTest, DifferentRemoverForDifferentBrowserState) {
  TestBrowserState browser_state_1;
  TestBrowserState browser_state_2;

  BrowsingDataRemover* remover_1 =
      BrowsingDataRemover::FromBrowserState(&browser_state_1);
  BrowsingDataRemover* remover_2 =
      BrowsingDataRemover::FromBrowserState(&browser_state_2);

  EXPECT_NE(remover_1, remover_2);

  BrowsingDataRemover* remover_1_again =
      BrowsingDataRemover::FromBrowserState(&browser_state_1);
  EXPECT_EQ(remover_1_again, remover_1);
}

}  // namespace web
