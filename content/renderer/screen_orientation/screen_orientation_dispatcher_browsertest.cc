// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/screen_orientation/screen_orientation_dispatcher.h"

#include <list>
#include <memory>
#include <tuple>

#include "base/logging.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebLockOrientationCallback.h"

namespace content {

using LockOrientationCallback =
    mojom::ScreenOrientation::LockOrientationCallback;
using LockResult = ::blink::mojom::ScreenOrientationLockResult;

// MockLockOrientationCallback is an implementation of
// WebLockOrientationCallback and takes a LockOrientationResultHolder* as a
// parameter when being constructed. The |results_| pointer is owned by the
// caller and not by the callback object. The intent being that as soon as the
// callback is resolved, it will be killed so we use the
// LockOrientationResultHolder to know in which state the callback object is at
// any time.
class MockLockOrientationCallback : public blink::WebLockOrientationCallback {
 public:
  struct LockOrientationResultHolder {
    LockOrientationResultHolder() : succeeded_(false), failed_(false) {}

    bool succeeded_;
    bool failed_;
    blink::WebLockOrientationError error_;
  };

  explicit MockLockOrientationCallback(LockOrientationResultHolder* results)
      : results_(results) {}

  void onSuccess() override { results_->succeeded_ = true; }

  void onError(blink::WebLockOrientationError error) override {
    results_->failed_ = true;
    results_->error_ = error;
  }

 private:
  LockOrientationResultHolder* results_;
};

// TODO(lunalu): When available, test mojo service without needing a
// RenderViewTest.
class ScreenOrientationDispatcherTest : public RenderViewTest {
 protected:
  void SetUp() override {
    RenderViewTest::SetUp();
    dispatcher_.reset(new ScreenOrientationDispatcher(nullptr));
    ScreenOrientationAssociatedPtr screen_orientation;
    mojo::GetDummyProxyForTesting(&screen_orientation);
    dispatcher_->SetScreenOrientationForTests(screen_orientation);
  }

  void LockOrientation(
      blink::WebScreenOrientationLockType orientation,
      std::unique_ptr<blink::WebLockOrientationCallback> callback) {
    dispatcher_->lockOrientation(orientation, std::move(callback));
  }

  void UnlockOrientation() { dispatcher_->unlockOrientation(); }

  int GetRequestId() { return dispatcher_->GetRequestIdForTests(); }

  void RunLockResultCallback(int request_id, LockResult result) {
    dispatcher_->OnLockOrientationResult(request_id, result);
  }

  std::unique_ptr<ScreenOrientationDispatcher> dispatcher_;
};

// Test that calling lockOrientation() followed by unlockOrientation() cancel
// the lockOrientation().
TEST_F(ScreenOrientationDispatcherTest, CancelPending_Unlocking) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;

  LockOrientation(
      blink::WebScreenOrientationLockPortraitPrimary,
      base::MakeUnique<MockLockOrientationCallback>(&callback_results));
  UnlockOrientation();

  EXPECT_FALSE(callback_results.succeeded_);
  EXPECT_TRUE(callback_results.failed_);
  EXPECT_EQ(blink::WebLockOrientationErrorCanceled, callback_results.error_);
}

// Test that calling lockOrientation() twice cancel the first lockOrientation().
TEST_F(ScreenOrientationDispatcherTest, CancelPending_DoubleLock) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  // We create the object to prevent leaks but never actually use it.
  MockLockOrientationCallback::LockOrientationResultHolder callback_results2;

  LockOrientation(
      blink::WebScreenOrientationLockPortraitPrimary,
      base::MakeUnique<MockLockOrientationCallback>(&callback_results));

  LockOrientation(
      blink::WebScreenOrientationLockPortraitPrimary,
      base::MakeUnique<MockLockOrientationCallback>(&callback_results2));

  EXPECT_FALSE(callback_results.succeeded_);
  EXPECT_TRUE(callback_results.failed_);
  EXPECT_EQ(blink::WebLockOrientationErrorCanceled, callback_results.error_);
}

// Test that when a LockError message is received, the request is set as failed
// with the correct values.
TEST_F(ScreenOrientationDispatcherTest, LockRequest_Error) {
  std::map<LockResult, blink::WebLockOrientationError> errors;
  errors[LockResult::SCREEN_ORIENTATION_LOCK_RESULT_ERROR_NOT_AVAILABLE] =
      blink::WebLockOrientationErrorNotAvailable;
  errors[LockResult::SCREEN_ORIENTATION_LOCK_RESULT_ERROR_FULLSCREEN_REQUIRED] =
      blink::WebLockOrientationErrorFullscreenRequired;
  errors[LockResult::SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED] =
      blink::WebLockOrientationErrorCanceled;

  for (std::map<LockResult, blink::WebLockOrientationError>::const_iterator it =
           errors.begin();
       it != errors.end(); ++it) {
    MockLockOrientationCallback::LockOrientationResultHolder callback_results;
    LockOrientation(
        blink::WebScreenOrientationLockPortraitPrimary,
        base::MakeUnique<MockLockOrientationCallback>(&callback_results));
    RunLockResultCallback(GetRequestId(), it->first);
    EXPECT_FALSE(callback_results.succeeded_);
    EXPECT_TRUE(callback_results.failed_);
    EXPECT_EQ(it->second, callback_results.error_);
  }
}

// Test that when a LockSuccess message is received, the request is set as
// succeeded.
TEST_F(ScreenOrientationDispatcherTest, LockRequest_Success) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  LockOrientation(
      blink::WebScreenOrientationLockPortraitPrimary,
      base::MakeUnique<MockLockOrientationCallback>(&callback_results));

  RunLockResultCallback(GetRequestId(),
                        LockResult::SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS);

  EXPECT_TRUE(callback_results.succeeded_);
  EXPECT_FALSE(callback_results.failed_);
}

// Test the following scenario:
// - request1 is received by the dispatcher;
// - request2 is received by the dispatcher;
// - request1 is rejected;
// - request1 success response is received.
// Expected: request1 is still rejected, request2 has not been set as succeeded.
TEST_F(ScreenOrientationDispatcherTest, RaceScenario) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results1;
  MockLockOrientationCallback::LockOrientationResultHolder callback_results2;

  LockOrientation(
      blink::WebScreenOrientationLockPortraitPrimary,
      base::MakeUnique<MockLockOrientationCallback>(&callback_results1));
  int request_id1 = GetRequestId();

  LockOrientation(
      blink::WebScreenOrientationLockLandscapePrimary,
      base::MakeUnique<MockLockOrientationCallback>(&callback_results2));

  // callback_results1 must be rejected, tested in CancelPending_DoubleLock.

  RunLockResultCallback(request_id1,
                        LockResult::SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS);

  // First request is still rejected.
  EXPECT_FALSE(callback_results1.succeeded_);
  EXPECT_TRUE(callback_results1.failed_);
  EXPECT_EQ(blink::WebLockOrientationErrorCanceled, callback_results1.error_);

  // Second request is still pending.
  EXPECT_FALSE(callback_results2.succeeded_);
  EXPECT_FALSE(callback_results2.failed_);
}

}  // namespace content
