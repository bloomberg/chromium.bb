// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/notifications/balloon_controller.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/test/testing_profile.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Use a dummy balloon collection for testing.
class MockBalloonCollection : public BalloonCollection {
  virtual void Add(const Notification& notification,
                   Profile* profile) {}
  virtual bool Remove(const Notification& notification) { return false; }
  virtual bool HasSpace() const { return true; }
  virtual void ResizeBalloon(Balloon* balloon, const gfx::Size& size) {};
  virtual void DisplayChanged() {}
  virtual void OnBalloonClosed(Balloon* source) {};
  virtual const Balloons& GetActiveBalloons() {
    NOTREACHED();
    return balloons_;
  }
 private:
  Balloons balloons_;
};

class BalloonControllerTest : public RenderViewHostTestHarness {
 public:
  BalloonControllerTest() :
      ui_thread_(ChromeThread::UI, MessageLoop::current()),
      io_thread_(ChromeThread::IO, MessageLoop::current()) {
  }

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    CocoaTest::BootstrapCocoa();
    profile_.reset(new TestingProfile());
    profile_->CreateRequestContext();
    browser_.reset(new Browser(Browser::TYPE_NORMAL, profile_.get()));
    collection_.reset(new MockBalloonCollection());
  }

  virtual void TearDown() {
    MessageLoop::current()->RunAllPending();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  ChromeThread ui_thread_;
  ChromeThread io_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<Browser> browser_;
  scoped_ptr<BalloonCollection> collection_;
};

TEST_F(BalloonControllerTest, ShowAndCloseTest) {
  Notification n(GURL("http://www.google.com"), GURL("http://www.google.com"),
      L"http://www.google.com", ASCIIToUTF16(""),
      new NotificationObjectProxy(-1, -1, -1, false));
  scoped_ptr<Balloon> balloon(
      new Balloon(n, profile_.get(), collection_.get()));

  BalloonController* controller = [BalloonController alloc];

  id mock = [OCMockObject partialMockForObject:controller];
  [[mock expect] initializeHost];

  [controller initWithBalloon:balloon.get()];
  [controller showWindow:nil];
  [controller closeBalloon:YES];

  [mock verify];
}

TEST_F(BalloonControllerTest, SizesTest) {
  Notification n(GURL("http://www.google.com"), GURL("http://www.google.com"),
      L"http://www.google.com", ASCIIToUTF16(""),
      new NotificationObjectProxy(-1, -1, -1, false));
  scoped_ptr<Balloon> balloon(
      new Balloon(n, profile_.get(), collection_.get()));
  balloon->set_content_size(gfx::Size(100, 100));

  BalloonController* controller = [BalloonController alloc];

  id mock = [OCMockObject partialMockForObject:controller];
  [[mock expect] initializeHost];

  [controller initWithBalloon:balloon.get()];

  EXPECT_TRUE([controller desiredTotalWidth] > 100);
  EXPECT_TRUE([controller desiredTotalHeight] > 100);
  [mock verify];
}

}
