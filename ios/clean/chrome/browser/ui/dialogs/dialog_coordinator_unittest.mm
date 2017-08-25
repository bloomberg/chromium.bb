// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator.h"

#import "base/mac/foundation_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_style.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_text_field_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_view_controller.h"
#import "ios/clean/chrome/browser/ui/dialogs/test_helpers/test_dialog_mediator.h"
#import "ios/clean/chrome/browser/ui/overlays/test_helpers/test_overlay_queue.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test version of DialogCoordinator that populates its consumer with the dummy
// values from TestDialogMediator.
@interface TestDialogCoordinator : DialogCoordinator
@property(nonatomic, readonly, strong) TestDialogMediator* testMediator;
@property(nonatomic, readwrite) UIAlertControllerStyle alertStyle;
@end

@implementation TestDialogCoordinator
@synthesize testMediator = _testMediator;
@synthesize alertStyle = _alertStyle;

- (instancetype)init {
  if ((self = [super init])) {
    _testMediator = [[TestDialogMediator alloc] init];
    _alertStyle = UIAlertControllerStyleAlert;
  }
  return self;
}

@end

@implementation TestDialogCoordinator (DialogCoordinatorSubclassing)

- (DialogMediator*)mediator {
  return self.testMediator;
}

@end

// A test fixture for DialogCoordinators.
class DialogCoordinatorTest : public PlatformTest {
 public:
  DialogCoordinatorTest()
      : PlatformTest(),
        coordinator_([[TestDialogCoordinator alloc] init]),
        browser_(ios::ChromeBrowserState::FromBrowserState(&browser_state_)) {
    // DialogConsumers require at least one button.
    coordinator_.testMediator.buttonConfigs = @[ [DialogButtonConfiguration
        configWithText:@"OK"
                 style:DialogButtonStyle::DEFAULT] ];
    // Add the coordinator to the queue.
    queue_.SetBrowser(&browser_);
    queue_.AddOverlay(coordinator_);
  }

  ~DialogCoordinatorTest() override { queue_.CancelOverlays(); }

  void StartCoordinator() { queue_.StartNextOverlay(); }

  DialogViewController* alert_controller() {
    return base::mac::ObjCCastStrict<DialogViewController>(
        coordinator_.viewController);
  }

 protected:
  __strong TestDialogCoordinator* coordinator_;
  web::TestBrowserState browser_state_;
  Browser browser_;
  TestOverlayQueue queue_;
};

// Tests that the alert style is used by default.
TEST_F(DialogCoordinatorTest, DefaultStyle) {
  StartCoordinator();
  ASSERT_TRUE(alert_controller());
  EXPECT_EQ(UIAlertControllerStyleAlert, alert_controller().preferredStyle);
}

// Tests that the action sheet style is used if specified.
TEST_F(DialogCoordinatorTest, ActionSheetStyle) {
  coordinator_.alertStyle = UIAlertControllerStyleActionSheet;
  StartCoordinator();
  ASSERT_TRUE(alert_controller());
  EXPECT_EQ(UIAlertControllerStyleActionSheet,
            alert_controller().preferredStyle);
}
