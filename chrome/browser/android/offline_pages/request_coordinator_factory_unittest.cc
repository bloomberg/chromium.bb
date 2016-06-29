// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/offline_pages/background/request_coordinator.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class RequestCoordinatorFactoryTest : public ChromeRenderViewHostTestHarness {
 public:
  RequestCoordinatorFactoryTest();
  ~RequestCoordinatorFactoryTest() override;

  void FlushBlockingPool();
};

RequestCoordinatorFactoryTest::RequestCoordinatorFactoryTest() {}

RequestCoordinatorFactoryTest::~RequestCoordinatorFactoryTest() {}

void RequestCoordinatorFactoryTest::FlushBlockingPool() {
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
}

TEST_F(RequestCoordinatorFactoryTest, BuildRequestCoordinator) {
  RequestCoordinatorFactory* factory = RequestCoordinatorFactory::GetInstance();
  EXPECT_NE(nullptr, factory);
  RequestCoordinator* coordinator1 =
      factory->GetForBrowserContext(browser_context());
  // We should actually get a coordinator.
  EXPECT_NE(nullptr, coordinator1);

  RequestCoordinator* coordinator2 =
      factory->GetForBrowserContext(browser_context());
  // Calling twice gives us the same coordinator.
  EXPECT_EQ(coordinator1, coordinator2);

  FlushBlockingPool();
}

}  // namespace offline_pages
