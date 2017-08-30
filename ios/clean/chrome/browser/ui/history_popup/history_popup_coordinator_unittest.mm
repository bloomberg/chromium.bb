// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/history_popup/history_popup_coordinator.h"

#import "ios/chrome/browser/ui/coordinators/browser_coordinator_test.h"
#import "ios/chrome/browser/ui/history_popup/tab_history_popup_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HistoryPopupCoordinator (ExposedForTesting)
@property(nonatomic, strong)
    TabHistoryPopupController* tabHistoryPopupController;
@end

namespace {

class HistoryPopupCoordinatorTest : public BrowserCoordinatorTest {
 public:
  HistoryPopupCoordinatorTest()
      : coordinator_([[HistoryPopupCoordinator alloc] init]),
        controller_(
            [OCMockObject niceMockForClass:[TabHistoryPopupController class]]) {
    coordinator_.tabHistoryPopupController = controller_;
  }

 protected:
  HistoryPopupCoordinator* coordinator_;
  TabHistoryPopupController* controller_;
};

// Tests that the History Popup controller is nil after stop.
TEST_F(HistoryPopupCoordinatorTest, TestStopCoordinator) {
  EXPECT_NSNE(nil, coordinator_.tabHistoryPopupController);
  [coordinator_ stop];
  EXPECT_NSEQ(nil, coordinator_.tabHistoryPopupController);
}

}  // namespace
