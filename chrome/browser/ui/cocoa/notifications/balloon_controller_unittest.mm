// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/notifications/balloon_controller.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Subclass balloon controller and mock out the initialization of the RVH.
@interface TestBalloonController : BalloonController {
}
- (void)initializeHost;
@end

@implementation TestBalloonController
- (void)initializeHost {}
@end

namespace {

// Use a dummy balloon collection for testing.
class MockBalloonCollection : public BalloonCollection {
  virtual void Add(const Notification& notification,
                   Profile* profile) {}
  virtual bool RemoveById(const std::string& id) { return false; }
  virtual bool RemoveBySourceOrigin(const GURL& origin) { return false; }
  virtual void RemoveAll() {}
  virtual bool HasSpace() const { return true; }
  virtual void ResizeBalloon(Balloon* balloon, const gfx::Size& size) {}
  virtual void DisplayChanged() {}
  virtual void SetPositionPreference(PositionPreference preference) {}
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
      ui_thread_(BrowserThread::UI, MessageLoop::current()),
      io_thread_(BrowserThread::IO, MessageLoop::current()) {
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
  BrowserThread ui_thread_;
  BrowserThread io_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<Browser> browser_;
  scoped_ptr<BalloonCollection> collection_;
};

TEST_F(BalloonControllerTest, ShowAndCloseTest) {
  Notification n(GURL("http://www.google.com"), GURL("http://www.google.com"),
      ASCIIToUTF16("http://www.google.com"), string16(),
      new NotificationObjectProxy(-1, -1, -1, false));
  scoped_ptr<Balloon> balloon(
      new Balloon(n, profile_.get(), collection_.get()));
  balloon->SetPosition(gfx::Point(1, 1), false);
  balloon->set_content_size(gfx::Size(100, 100));

  BalloonController* controller =
      [[TestBalloonController alloc] initWithBalloon:balloon.get()];

  [controller showWindow:nil];
  [controller closeBalloon:YES];
}

TEST_F(BalloonControllerTest, SizesTest) {
  Notification n(GURL("http://www.google.com"), GURL("http://www.google.com"),
      ASCIIToUTF16("http://www.google.com"), string16(),
      new NotificationObjectProxy(-1, -1, -1, false));
  scoped_ptr<Balloon> balloon(
      new Balloon(n, profile_.get(), collection_.get()));
  balloon->SetPosition(gfx::Point(1, 1), false);
  balloon->set_content_size(gfx::Size(100, 100));

  BalloonController* controller =
      [[TestBalloonController alloc] initWithBalloon:balloon.get()];

  [controller showWindow:nil];

  EXPECT_TRUE([controller desiredTotalWidth] > 100);
  EXPECT_TRUE([controller desiredTotalHeight] > 100);

  [controller closeBalloon:YES];
}

}
