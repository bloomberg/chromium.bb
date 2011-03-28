// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/location_bar/instant_opt_in_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface InstantOptInController (ExposedForTesting)
- (NSButton*)okButton;
- (NSButton*)cancelButton;
@end

@implementation InstantOptInController (ExposedForTesting)
- (NSButton*)okButton {
  return okButton_;
}

- (NSButton*)cancelButton {
  return cancelButton_;
}
@end


namespace {

class MockDelegate : public InstantOptInControllerDelegate {
 public:
  MOCK_METHOD1(UserPressedOptIn, void(bool opt_in));
};

class InstantOptInControllerTest : public CocoaTest {
 public:
  void SetUp() {
    CocoaTest::SetUp();

    controller_.reset(
        [[InstantOptInController alloc] initWithDelegate:&delegate_]);

    NSView* parent = [test_window() contentView];
    [parent addSubview:[controller_ view]];
  }

  MockDelegate delegate_;
  scoped_nsobject<InstantOptInController> controller_;
};

TEST_F(InstantOptInControllerTest, OkButtonCallback) {
  EXPECT_CALL(delegate_, UserPressedOptIn(true));
  [[controller_ okButton] performClick:nil];
}

TEST_F(InstantOptInControllerTest, CancelButtonCallback) {
  EXPECT_CALL(delegate_, UserPressedOptIn(false));
  [[controller_ cancelButton] performClick:nil];
}

}  // namespace
