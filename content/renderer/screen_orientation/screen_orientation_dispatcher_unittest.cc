// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "screen_orientation_dispatcher.h"

#include <list>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/screen_orientation_messages.h"
#include "content/public/test/mock_render_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebLockOrientationCallback.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationListener.h"

namespace content {

class MockScreenOrientationListener :
    public blink::WebScreenOrientationListener {
 public:
  MockScreenOrientationListener();
  virtual ~MockScreenOrientationListener() {}

  virtual void didChangeScreenOrientation(
      blink::WebScreenOrientationType) OVERRIDE;

  bool did_change_screen_orientation() const {
    return did_change_screen_orientation_;
  }

  blink::WebScreenOrientationType screen_orientation() const {
    return screen_orientation_;
  }

 private:
  bool did_change_screen_orientation_;
  blink::WebScreenOrientationType screen_orientation_;

  DISALLOW_COPY_AND_ASSIGN(MockScreenOrientationListener);
};

// MockLockOrientationCallback is an implementation of
// WebLockOrientationCallback and takes a LockOrientationResultHolder* as a
// parameter when being constructed. The |results_| pointer is owned by the
// caller and not by the callback object. The intent being that as soon as the
// callback is resolved, it will be killed so we use the
// LockOrientationResultHolder to know in which state the callback object is at
// any time.
class MockLockOrientationCallback :
    public blink::WebLockOrientationCallback {
 public:
  struct LockOrientationResultHolder {
    LockOrientationResultHolder()
        : succeeded_(false), failed_(false) {}

    bool succeeded_;
    bool failed_;
    unsigned angle_;
    blink::WebScreenOrientationType orientation_;
    blink::WebLockOrientationCallback::ErrorType error_;
  };

  static scoped_ptr<blink::WebLockOrientationCallback> CreateScoped(
      LockOrientationResultHolder* results) {
    return scoped_ptr<blink::WebLockOrientationCallback>(
        new MockLockOrientationCallback(results));
  }

  virtual void onSuccess(unsigned angle,
                         blink::WebScreenOrientationType orientation) {
    results_->succeeded_ = true;
    results_->angle_ = angle;
    results_->orientation_ = orientation;
  }

  virtual void onError(
      blink::WebLockOrientationCallback::ErrorType error) {
    results_->failed_ = true;
    results_->error_ = error;
  }

 private:
  explicit MockLockOrientationCallback(LockOrientationResultHolder* results)
      : results_(results) {}

  virtual ~MockLockOrientationCallback() {}

  LockOrientationResultHolder* results_;
};

MockScreenOrientationListener::MockScreenOrientationListener()
    : did_change_screen_orientation_(false),
      screen_orientation_(blink::WebScreenOrientationPortraitPrimary) {
}

void MockScreenOrientationListener::didChangeScreenOrientation(
    blink::WebScreenOrientationType orientation) {
  did_change_screen_orientation_ = true;
  screen_orientation_ = orientation;
}

class ScreenOrientationDispatcherTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    render_thread_.reset(new MockRenderThread);
    listener_.reset(new MockScreenOrientationListener);
    dispatcher_.reset(new ScreenOrientationDispatcher);
    dispatcher_->setListener(listener_.get());
  }

  int GetFirstLockRequestIdFromSink() {
    const IPC::Message* msg = render_thread_->sink().GetFirstMessageMatching(
        ScreenOrientationHostMsg_LockRequest::ID);
    EXPECT_TRUE(msg != NULL);

    Tuple2<blink::WebScreenOrientationLockType,int> params;
    ScreenOrientationHostMsg_LockRequest::Read(msg, &params);
    return params.b;
  }

  scoped_ptr<MockRenderThread> render_thread_;
  scoped_ptr<MockScreenOrientationListener> listener_;
  scoped_ptr<ScreenOrientationDispatcher> dispatcher_;
};

TEST_F(ScreenOrientationDispatcherTest, ListensToMessages) {
  EXPECT_TRUE(render_thread_->observers().HasObserver(dispatcher_.get()));

  render_thread_->OnControlMessageReceived(
      ScreenOrientationMsg_OrientationChange(
          blink::WebScreenOrientationPortraitPrimary));

  EXPECT_TRUE(listener_->did_change_screen_orientation());
}

TEST_F(ScreenOrientationDispatcherTest, NullListener) {
  dispatcher_->setListener(NULL);

  render_thread_->OnControlMessageReceived(
      ScreenOrientationMsg_OrientationChange(
          blink::WebScreenOrientationPortraitPrimary));

  EXPECT_FALSE(listener_->did_change_screen_orientation());
}

TEST_F(ScreenOrientationDispatcherTest, ValidValues) {
  render_thread_->OnControlMessageReceived(
      ScreenOrientationMsg_OrientationChange(
          blink::WebScreenOrientationPortraitPrimary));
  EXPECT_EQ(blink::WebScreenOrientationPortraitPrimary,
               listener_->screen_orientation());

  render_thread_->OnControlMessageReceived(
      ScreenOrientationMsg_OrientationChange(
          blink::WebScreenOrientationLandscapePrimary));
  EXPECT_EQ(blink::WebScreenOrientationLandscapePrimary,
               listener_->screen_orientation());

  render_thread_->OnControlMessageReceived(
      ScreenOrientationMsg_OrientationChange(
          blink::WebScreenOrientationPortraitSecondary));
  EXPECT_EQ(blink::WebScreenOrientationPortraitSecondary,
               listener_->screen_orientation());

  render_thread_->OnControlMessageReceived(
      ScreenOrientationMsg_OrientationChange(
          blink::WebScreenOrientationLandscapeSecondary));
  EXPECT_EQ(blink::WebScreenOrientationLandscapeSecondary,
               listener_->screen_orientation());
}

// Test that calling LockOrientation() followed by UnlockOrientation() cancel
// the LockOrientation().
TEST_F(ScreenOrientationDispatcherTest, CancelPending_Unlocking) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  dispatcher_->LockOrientation(
      blink::WebScreenOrientationLockPortraitPrimary,
      MockLockOrientationCallback::CreateScoped(&callback_results));
  dispatcher_->UnlockOrientation();

  EXPECT_FALSE(callback_results.succeeded_);
  EXPECT_TRUE(callback_results.failed_);
  EXPECT_EQ(blink::WebLockOrientationCallback::ErrorTypeCanceled,
            callback_results.error_);
}

// Test that calling LockOrientation() twice cancel the first LockOrientation().
TEST_F(ScreenOrientationDispatcherTest, CancelPending_DoubleLock) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  // We create the object to prevent leaks but never actually use it.
  MockLockOrientationCallback::LockOrientationResultHolder callback_results2;

  dispatcher_->LockOrientation(
      blink::WebScreenOrientationLockPortraitPrimary,
      MockLockOrientationCallback::CreateScoped(&callback_results));
  dispatcher_->LockOrientation(
      blink::WebScreenOrientationLockPortraitPrimary,
      MockLockOrientationCallback::CreateScoped(&callback_results2));

  EXPECT_FALSE(callback_results.succeeded_);
  EXPECT_TRUE(callback_results.failed_);
  EXPECT_EQ(blink::WebLockOrientationCallback::ErrorTypeCanceled,
            callback_results.error_);
}

// Test that when a LockError message is received, the request is set as failed
// with the correct values.
TEST_F(ScreenOrientationDispatcherTest, LockRequest_Error) {
  std::list<blink::WebLockOrientationCallback::ErrorType> errors;
  errors.push_back(blink::WebLockOrientationCallback::ErrorTypeNotAvailable);
  errors.push_back(
      blink::WebLockOrientationCallback::ErrorTypeFullScreenRequired);
  errors.push_back(blink::WebLockOrientationCallback::ErrorTypeCanceled);

  for (std::list<blink::WebLockOrientationCallback::ErrorType>::const_iterator
          it = errors.begin(); it != errors.end(); ++it) {
    render_thread_->sink().ClearMessages();

    MockLockOrientationCallback::LockOrientationResultHolder callback_results;
    dispatcher_->LockOrientation(
        blink::WebScreenOrientationLockPortraitPrimary,
        MockLockOrientationCallback::CreateScoped(&callback_results));

    int request_id = GetFirstLockRequestIdFromSink();
    render_thread_->OnControlMessageReceived(
        ScreenOrientationMsg_LockError(request_id, *it));

    EXPECT_FALSE(callback_results.succeeded_);
    EXPECT_TRUE(callback_results.failed_);
    EXPECT_EQ(*it, callback_results.error_);
  }
}

// Test that when a LockSuccess message is received, the request is set as
// succeeded with the correct values.
TEST_F(ScreenOrientationDispatcherTest, LockRequest_Success) {
  struct ScreenOrientationInformation {
    unsigned angle;
    blink::WebScreenOrientationType type;
  } orientations[] = {
    { 0, blink::WebScreenOrientationPortraitPrimary },
    { 0, blink::WebScreenOrientationLandscapePrimary },
    { 90, blink::WebScreenOrientationPortraitSecondary },
    { 90, blink::WebScreenOrientationLandscapePrimary }
  };

  int orientationsCount = 4;

  for (int i = 0; i < orientationsCount; ++i) {
    render_thread_->sink().ClearMessages();

    MockLockOrientationCallback::LockOrientationResultHolder callback_results;
    dispatcher_->LockOrientation(
        blink::WebScreenOrientationLockPortraitPrimary,
        MockLockOrientationCallback::CreateScoped(&callback_results));

    int request_id = GetFirstLockRequestIdFromSink();
    render_thread_->OnControlMessageReceived(
        ScreenOrientationMsg_LockSuccess(request_id,
                                         orientations[i].angle,
                                         orientations[i].type));

    EXPECT_TRUE(callback_results.succeeded_);
    EXPECT_FALSE(callback_results.failed_);
    EXPECT_EQ(orientations[i].angle, callback_results.angle_);
    EXPECT_EQ(orientations[i].type, callback_results.orientation_);
  }
}

// Test an edge case: a LockSuccess is received but it matches no pending
// callback.
TEST_F(ScreenOrientationDispatcherTest, SuccessForUnknownRequest) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  dispatcher_->LockOrientation(
      blink::WebScreenOrientationLockPortraitPrimary,
      MockLockOrientationCallback::CreateScoped(&callback_results));

  int request_id = GetFirstLockRequestIdFromSink();
  render_thread_->OnControlMessageReceived(ScreenOrientationMsg_LockSuccess(
      request_id + 1,
      90,
      blink::WebScreenOrientationLandscapePrimary));

  EXPECT_FALSE(callback_results.succeeded_);
  EXPECT_FALSE(callback_results.failed_);
}

// Test an edge case: a LockError is received but it matches no pending
// callback.
TEST_F(ScreenOrientationDispatcherTest, ErrorForUnknownRequest) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  dispatcher_->LockOrientation(
      blink::WebScreenOrientationLockPortraitPrimary,
      MockLockOrientationCallback::CreateScoped(&callback_results));

  int request_id = GetFirstLockRequestIdFromSink();
  render_thread_->OnControlMessageReceived(ScreenOrientationMsg_LockError(
      request_id + 1,
      blink::WebLockOrientationCallback::ErrorTypeCanceled));

  EXPECT_FALSE(callback_results.succeeded_);
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

  dispatcher_->LockOrientation(
      blink::WebScreenOrientationLockPortraitPrimary,
      MockLockOrientationCallback::CreateScoped(&callback_results1));
  int request_id1 = GetFirstLockRequestIdFromSink();

  dispatcher_->LockOrientation(
      blink::WebScreenOrientationLockLandscapePrimary,
      MockLockOrientationCallback::CreateScoped(&callback_results2));

  // callback_results1 must be rejected, tested in CancelPending_DoubleLock.

  render_thread_->OnControlMessageReceived(ScreenOrientationMsg_LockSuccess(
      request_id1,
      0,
      blink::WebScreenOrientationPortraitPrimary));

  // First request is still rejected.
  EXPECT_FALSE(callback_results1.succeeded_);
  EXPECT_TRUE(callback_results1.failed_);
  EXPECT_EQ(blink::WebLockOrientationCallback::ErrorTypeCanceled,
            callback_results1.error_);

  // Second request is still pending.
  EXPECT_FALSE(callback_results2.succeeded_);
  EXPECT_FALSE(callback_results2.failed_);
}

}  // namespace content
