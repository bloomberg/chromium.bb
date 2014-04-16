// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "screen_orientation_dispatcher.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/screen_orientation_messages.h"
#include "content/public/test/mock_render_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
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
    dispatcher_.reset(new ScreenOrientationDispatcher(render_thread_.get()));
    dispatcher_->setListener(listener_.get());
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

}  // namespace content
