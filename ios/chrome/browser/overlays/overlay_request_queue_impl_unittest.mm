// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/overlay_request_queue_impl.h"

#include "ios/chrome/browser/overlays/overlay_request_queue_impl_observer.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Mock queue observer.
class MockOverlayRequestQueueImplObserver
    : public OverlayRequestQueueImplObserver {
 public:
  MockOverlayRequestQueueImplObserver() {}
  ~MockOverlayRequestQueueImplObserver() {}

  MOCK_METHOD2(OnRequestAdded, void(OverlayRequestQueueImpl*, OverlayRequest*));
  MOCK_METHOD3(OnRequestRemoved,
               void(OverlayRequestQueueImpl*, OverlayRequest*, bool));
};
}  // namespace

// Test fixture for RequestQueueImpl.
class OverlayRequestQueueImplTest : public PlatformTest {
 public:
  OverlayRequestQueueImplTest() : PlatformTest() {
    OverlayRequestQueueImpl::Container::CreateForWebState(&web_state_);
    queue()->AddObserver(&observer_);
  }
  ~OverlayRequestQueueImplTest() override {
    queue()->RemoveObserver(&observer_);
  }

  OverlayRequestQueueImpl* queue() {
    // Use the kWebContentArea queue for testing.
    return OverlayRequestQueueImpl::Container::FromWebState(&web_state_)
        ->QueueForModality(OverlayModality::kWebContentArea);
  }
  MockOverlayRequestQueueImplObserver& observer() { return observer_; }

 private:
  web::TestWebState web_state_;
  MockOverlayRequestQueueImplObserver observer_;
};

// Tests that state is updated correctly and observer callbacks are received
// when adding requests to the back of the queue.
TEST_F(OverlayRequestQueueImplTest, AddRequest) {
  std::unique_ptr<OverlayRequest> first_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  OverlayRequest* first_request_ptr = first_request.get();
  std::unique_ptr<OverlayRequest> second_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  OverlayRequest* second_request_ptr = second_request.get();

  // Add two requests and pop the first, verifying that the size and front
  // requests are updated.
  EXPECT_CALL(observer(), OnRequestAdded(queue(), first_request_ptr));
  queue()->AddRequest(std::move(first_request));

  EXPECT_CALL(observer(), OnRequestAdded(queue(), second_request_ptr));
  queue()->AddRequest(std::move(second_request));

  EXPECT_EQ(first_request_ptr, queue()->front_request());
  EXPECT_EQ(2U, queue()->size());
}

// Tests that state is updated correctly and observer callbacks are received
// when popping the frontmost request.
TEST_F(OverlayRequestQueueImplTest, PopFrontRequest) {
  std::unique_ptr<OverlayRequest> first_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  OverlayRequest* first_request_ptr = first_request.get();
  std::unique_ptr<OverlayRequest> second_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  OverlayRequest* second_request_ptr = second_request.get();

  // Add two requests and pop the first.
  EXPECT_CALL(observer(), OnRequestAdded(queue(), first_request_ptr));
  queue()->AddRequest(std::move(first_request));

  EXPECT_CALL(observer(), OnRequestAdded(queue(), second_request_ptr));
  queue()->AddRequest(std::move(second_request));

  EXPECT_CALL(observer(), OnRequestRemoved(queue(), first_request_ptr, true));
  queue()->PopFrontRequest();

  // Verify that the size and front request have been updated.
  EXPECT_EQ(second_request_ptr, queue()->front_request());
  EXPECT_EQ(1U, queue()->size());
}

// Tests that state is updated correctly and observer callbacks are received
// when popping the back request.
TEST_F(OverlayRequestQueueImplTest, PopBackRequest) {
  std::unique_ptr<OverlayRequest> first_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  OverlayRequest* first_request_ptr = first_request.get();
  std::unique_ptr<OverlayRequest> second_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  OverlayRequest* second_request_ptr = second_request.get();

  // Add two requests and pop the second.
  EXPECT_CALL(observer(), OnRequestAdded(queue(), first_request_ptr));
  queue()->AddRequest(std::move(first_request));

  EXPECT_CALL(observer(), OnRequestAdded(queue(), second_request_ptr));
  queue()->AddRequest(std::move(second_request));

  EXPECT_CALL(observer(), OnRequestRemoved(queue(), second_request_ptr, false));
  queue()->PopBackRequest();

  // Verify that the size and front request have been updated.
  EXPECT_EQ(first_request_ptr, queue()->front_request());
  EXPECT_EQ(1U, queue()->size());
}
