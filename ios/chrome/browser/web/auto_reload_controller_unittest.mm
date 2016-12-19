// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/auto_reload_controller.h"

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

@interface AutoReloadController (Testing)

- (void)setTimerForTesting:(std::unique_ptr<base::Timer>)timer;

@end

@interface TestAutoReloadDelegate : NSObject<AutoReloadDelegate>
@end

@implementation TestAutoReloadDelegate {
  int reloads_;
}

- (void)reload {
  reloads_++;
}

- (int)reloads {
  return reloads_;
}

@end

class AutoReloadControllerTest : public testing::Test {
 public:
  AutoReloadControllerTest() : timer_(new base::MockTimer(false, false)) {}

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    delegate_.reset([[TestAutoReloadDelegate alloc] init]);
    controller_.reset([[AutoReloadController alloc]
        initWithDelegate:delegate_.get()
            onlineStatus:YES]);
    // Note: even though setTimerForTesting theoretically passes ownership of
    // the timer to the controller, this class retains a weak pointer to the
    // timer so it can query or fire it for testing. The only reason this is
    // safe is that this class owns the controller which now owns the timer, so
    // the timer has the same lifetime as this class.
    [controller_ setTimerForTesting:std::unique_ptr<base::Timer>(timer_)];
  }

  // Synthesize a failing page load.
  void DoFailingLoad(const GURL& url) {
    [controller_ loadStartedForURL:url];
    [controller_ loadFailedForURL:url wasPost:NO];
  }

  // Synthesize a succeeding page load.
  void DoSucceedingLoad(const GURL& url) {
    [controller_ loadStartedForURL:url];
    [controller_ loadFinishedForURL:url wasPost:NO];
  }

  base::scoped_nsobject<TestAutoReloadDelegate> delegate_;
  base::scoped_nsobject<AutoReloadController> controller_;
  base::MockTimer* timer_;  // weak
};

TEST_F(AutoReloadControllerTest, AutoReloadSucceeds) {
  const GURL kTestUrl("https://www.google.com");
  DoFailingLoad(kTestUrl);
  EXPECT_TRUE(timer_->IsRunning());
  EXPECT_EQ(0, [delegate_ reloads]);
  timer_->Fire();
  EXPECT_EQ(1, [delegate_ reloads]);
  DoSucceedingLoad(kTestUrl);
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(AutoReloadControllerTest, AutoReloadRetries) {
  const GURL kTestUrl("https://www.google.com");
  DoFailingLoad(kTestUrl);
  EXPECT_EQ(0, [delegate_ reloads]);
  timer_->Fire();
  DoFailingLoad(kTestUrl);
  EXPECT_EQ(1, [delegate_ reloads]);
  timer_->Fire();
  DoFailingLoad(kTestUrl);
  EXPECT_EQ(2, [delegate_ reloads]);
  timer_->Fire();
  DoSucceedingLoad(kTestUrl);
}

TEST_F(AutoReloadControllerTest, AutoReloadBacksOff) {
  const GURL kTestUrl("https://www.google.com");
  DoFailingLoad(kTestUrl);
  base::TimeDelta previous = timer_->GetCurrentDelay();
  timer_->Fire();
  DoFailingLoad(kTestUrl);
  int tries = 0;
  const int kMaxTries = 20;
  while (tries < kMaxTries && timer_->GetCurrentDelay() != previous) {
    previous = timer_->GetCurrentDelay();
    timer_->Fire();
    DoFailingLoad(kTestUrl);
    tries++;
  }

  EXPECT_NE(tries, 20);
}

TEST_F(AutoReloadControllerTest, AutoReloadStopsOnUserLoadStart) {
  const GURL kTestUrl("https://www.google.com");
  const GURL kOtherUrl("https://mail.google.com");
  DoFailingLoad(kTestUrl);
  EXPECT_TRUE(timer_->IsRunning());
  [controller_ loadStartedForURL:kOtherUrl];
  EXPECT_FALSE(timer_->IsRunning());
}

TEST_F(AutoReloadControllerTest, AutoReloadBackoffResetsOnUserLoadStart) {
  const GURL kTestUrl("https://www.google.com");
  const GURL kOtherUrl("https://mail.google.com");
  base::TimeDelta first_delay;
  base::TimeDelta second_delay;
  DoFailingLoad(kTestUrl);
  EXPECT_TRUE(timer_->IsRunning());
  first_delay = timer_->GetCurrentDelay();
  timer_->Fire();
  DoFailingLoad(kTestUrl);
  EXPECT_TRUE(timer_->IsRunning());
  second_delay = timer_->GetCurrentDelay();
  EXPECT_NE(first_delay, second_delay);

  DoFailingLoad(kOtherUrl);
  EXPECT_TRUE(timer_->IsRunning());
  EXPECT_EQ(first_delay, timer_->GetCurrentDelay());
  timer_->Fire();
  DoFailingLoad(kOtherUrl);
  EXPECT_EQ(second_delay, timer_->GetCurrentDelay());
}

TEST_F(AutoReloadControllerTest, AutoReloadStopsAtOffline) {
  const GURL kTestUrl("https://www.google.com");
  DoFailingLoad(kTestUrl);
  EXPECT_TRUE(timer_->IsRunning());
  [controller_ networkStateChangedToOnline:NO];
  EXPECT_FALSE(timer_->IsRunning());
  [controller_ networkStateChangedToOnline:YES];
  EXPECT_TRUE(timer_->IsRunning());
}

TEST_F(AutoReloadControllerTest, AutoReloadDoesntStartWhileOffline) {
  const GURL kTestUrl("https://www.google.com");
  [controller_ loadStartedForURL:kTestUrl];
  [controller_ networkStateChangedToOnline:NO];
  [controller_ loadFailedForURL:kTestUrl wasPost:NO];
  EXPECT_FALSE(timer_->IsRunning());
  [controller_ networkStateChangedToOnline:YES];
  EXPECT_TRUE(timer_->IsRunning());
}

TEST_F(AutoReloadControllerTest, AutoReloadDoesNotBackoffAtNetworkChange) {
  const GURL kTestUrl("https://www.google.com");
  DoFailingLoad(kTestUrl);
  EXPECT_TRUE(timer_->IsRunning());
  base::TimeDelta delay = timer_->GetCurrentDelay();
  [controller_ networkStateChangedToOnline:NO];
  [controller_ networkStateChangedToOnline:YES];
  EXPECT_TRUE(timer_->IsRunning());
  EXPECT_EQ(delay, timer_->GetCurrentDelay());
}

TEST_F(AutoReloadControllerTest, AutoReloadDoesNotReloadPosts) {
  const GURL kTestUrl("https://www.google.com");
  [controller_ loadStartedForURL:kTestUrl];
  [controller_ loadFailedForURL:kTestUrl wasPost:YES];
  EXPECT_FALSE(timer_->IsRunning());
}
