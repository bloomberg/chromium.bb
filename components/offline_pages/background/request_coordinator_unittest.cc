// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/background/save_page_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
// put test constants here
const int64_t kRequestId = 42;
const GURL kUrl("http://universe.com/everything");
const ClientId kClientId("bookmark", "1234");
}  // namespace

class RequestCoordinatorTest
    : public testing::Test {
 public:
  RequestCoordinatorTest();
  ~RequestCoordinatorTest() override;

  void CallbackFunction() {
  }
};

RequestCoordinatorTest::RequestCoordinatorTest() {}

RequestCoordinatorTest::~RequestCoordinatorTest() {}

TEST_F(RequestCoordinatorTest, StartProcessingWithNoRequests) {
  RequestCoordinator::ProcessingDoneCallback callback =
      base::Bind(
          &RequestCoordinatorTest::CallbackFunction,
          base::Unretained(this));
  RequestCoordinator coordinator;
  EXPECT_FALSE(coordinator.StartProcessing(callback));
}

TEST_F(RequestCoordinatorTest, SavePageLater) {
  RequestCoordinator coordinator;
  SavePageRequest request(kRequestId, kUrl, kClientId, base::Time::Now());
  EXPECT_TRUE(coordinator.SavePageLater(request));
}

}  // namespace offline_pages
