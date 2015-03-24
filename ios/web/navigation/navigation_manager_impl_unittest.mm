// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/test/test_browser_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace web {
namespace {

class NavigationManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    manager_.reset(new NavigationManagerImpl(NULL, &browser_state_));
  }
  scoped_ptr<NavigationManagerImpl> manager_;
  TestBrowserState browser_state_;
};

// TODO(stuartmorgan): Remove this once NavigationManager has actual
// functionality to test, instead of just being pass-throughs.
TEST_F(NavigationManagerTest, Dummy) {
  EXPECT_FALSE(manager_->CanGoBack());
  EXPECT_FALSE(manager_->CanGoForward());
}

}  // namespace
}  // namespace web
