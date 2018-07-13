// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_gesture_dispatcher.h"

#include "chromecast/base/chromecast_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"

// Gmock matchers and actions that we use below.
using testing::_;
using testing::AnyOf;
using testing::Eq;
using testing::Return;
using testing::SetArgPointee;
using testing::WithArg;

namespace chromecast {
namespace shell {

namespace {

constexpr gfx::Point kLeftSidePoint(5, 50);
constexpr gfx::Point kOngoingBackGesturePoint1(70, 50);
constexpr gfx::Point kOngoingBackGesturePoint2(75, 50);
constexpr gfx::Point kValidBackGestureEndPoint(90, 50);
constexpr gfx::Point kPastTheEndPoint1(105, 50);
constexpr gfx::Point kPastTheEndPoint2(200, 50);

constexpr gfx::Point kTopSidePoint(100, 5);
constexpr gfx::Point kOngoingTopGesturePoint1(100, 70);
constexpr gfx::Point kOngoingTopGesturePoint2(100, 75);
constexpr gfx::Point kTopGestureEndPoint(100, 90);

}  // namespace

class MockCastContentWindowDelegate : public CastContentWindow::Delegate {
 public:
  ~MockCastContentWindowDelegate() override = default;

  MOCK_METHOD1(CanHandleGesture, bool(GestureType gesture_type));
  MOCK_METHOD1(ConsumeGesture, bool(GestureType gesture_type));
  MOCK_METHOD2(CancelGesture,
               void(GestureType gesture_type,
                    const gfx::Point& touch_location));
  MOCK_METHOD2(GestureProgress,
               void(GestureType gesture_type,
                    const gfx::Point& touch_location));
  std::string GetId() override { return "mockContentWindowDelegate"; }
};

class CastGestureDispatcherTest : public testing::Test {
 public:
  CastGestureDispatcherTest() : dispatcher_(&delegate_, true) {}

 protected:
  MockCastContentWindowDelegate delegate_;
  CastGestureDispatcher dispatcher_;
};

// Verify the simple case of a left swipe with the right horizontal leads to
// back.
TEST_F(CastGestureDispatcherTest, VerifySimpleBackSuccess) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, GestureProgress(Eq(GestureType::GO_BACK),
                                         Eq(kOngoingBackGesturePoint1)));
  EXPECT_CALL(delegate_, ConsumeGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher_.HandleSideSwipeBegin(CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                      kOngoingBackGesturePoint1);
  dispatcher_.HandleSideSwipeEnd(CastSideSwipeOrigin::LEFT,
                                 kValidBackGestureEndPoint);
}

// Verify that if the finger is not lifted, that's not a back gesture.
TEST_F(CastGestureDispatcherTest, VerifyNoDispatchOnNoLift) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, ConsumeGesture(Eq(GestureType::GO_BACK))).Times(0);
  EXPECT_CALL(delegate_, GestureProgress(Eq(GestureType::GO_BACK),
                                         Eq(kValidBackGestureEndPoint)));
  EXPECT_CALL(delegate_,
              GestureProgress(Eq(GestureType::GO_BACK), Eq(kPastTheEndPoint1)));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher_.HandleSideSwipeBegin(CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                      kValidBackGestureEndPoint);
  dispatcher_.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                      kPastTheEndPoint1);
}

// Verify that multiple 'continue' events still only lead to one back
// invocation.
TEST_F(CastGestureDispatcherTest, VerifyOnlySingleDispatch) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, GestureProgress(Eq(GestureType::GO_BACK),
                                         Eq(kValidBackGestureEndPoint)));
  EXPECT_CALL(delegate_,
              GestureProgress(Eq(GestureType::GO_BACK), Eq(kPastTheEndPoint1)));
  EXPECT_CALL(delegate_, ConsumeGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher_.HandleSideSwipeBegin(CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                      kValidBackGestureEndPoint);
  dispatcher_.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                      kPastTheEndPoint1);
  dispatcher_.HandleSideSwipeEnd(CastSideSwipeOrigin::LEFT, kPastTheEndPoint2);
}

// Verify that if the delegate says it doesn't handle back that we won't try to
// ask them to consume it.
TEST_F(CastGestureDispatcherTest, VerifyDelegateDoesNotConsumeUnwanted) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(false));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher_.HandleSideSwipeBegin(CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                      kValidBackGestureEndPoint);
  dispatcher_.HandleSideSwipeEnd(CastSideSwipeOrigin::LEFT, kPastTheEndPoint2);
}

// Verify that a not-left gesture doesn't lead to a swipe.
TEST_F(CastGestureDispatcherTest, VerifyNotLeftSwipeIsNotBack) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::TOP);
  dispatcher_.HandleSideSwipeBegin(CastSideSwipeOrigin::TOP, kTopSidePoint);
  dispatcher_.HandleSideSwipeContinue(CastSideSwipeOrigin::TOP,
                                      kOngoingTopGesturePoint2);
}

// Verify that if the gesture doesn't go far enough horizontally that we will
// not consider it a swipe.
TEST_F(CastGestureDispatcherTest, VerifyNotFarEnoughRightIsNotBack) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, GestureProgress(Eq(GestureType::GO_BACK),
                                         Eq(kOngoingBackGesturePoint1)));
  EXPECT_CALL(delegate_, CancelGesture(Eq(GestureType::GO_BACK),
                                       Eq(kOngoingBackGesturePoint2)));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher_.HandleSideSwipeBegin(CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                      kOngoingBackGesturePoint1);
  dispatcher_.HandleSideSwipeEnd(CastSideSwipeOrigin::LEFT,
                                 kOngoingBackGesturePoint2);
}

// Verify that if the gesture ends before going far enough, that's also not a
// swipe.
TEST_F(CastGestureDispatcherTest, VerifyNotFarEnoughRightAndEndIsNotBack) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, GestureProgress(Eq(GestureType::GO_BACK),
                                         Eq(kOngoingBackGesturePoint1)));
  EXPECT_CALL(delegate_, CancelGesture(Eq(GestureType::GO_BACK),
                                       Eq(kOngoingBackGesturePoint2)));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher_.HandleSideSwipeBegin(CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_.HandleSideSwipeContinue(CastSideSwipeOrigin::LEFT,
                                      kOngoingBackGesturePoint1);
  dispatcher_.HandleSideSwipeEnd(CastSideSwipeOrigin::LEFT,
                                 kOngoingBackGesturePoint2);
}

// Verify simple top-down drag.
TEST_F(CastGestureDispatcherTest, VerifySimpleTopSuccess) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, GestureProgress(Eq(GestureType::TOP_DRAG),
                                         Eq(kOngoingTopGesturePoint1)));
  EXPECT_CALL(delegate_, ConsumeGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(true));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::TOP);
  dispatcher_.HandleSideSwipeBegin(CastSideSwipeOrigin::TOP, kTopSidePoint);
  dispatcher_.HandleSideSwipeContinue(CastSideSwipeOrigin::TOP,
                                      kOngoingTopGesturePoint1);
  dispatcher_.HandleSideSwipeEnd(CastSideSwipeOrigin::LEFT,
                                 kTopGestureEndPoint);
}
}  // namespace shell
}  // namespace chromecast
