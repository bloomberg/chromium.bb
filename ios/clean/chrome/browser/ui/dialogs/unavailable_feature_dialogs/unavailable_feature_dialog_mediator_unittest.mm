// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/unavailable_feature_dialogs/unavailable_feature_dialog_mediator.h"

#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_style.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator+subclassing.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using UnavailableFeatureDialogMediatorTest = PlatformTest;

// Tests that an UnavailableFeatureDialogMediator correctly uses the feature
// name as the message and has one OK button.
TEST_F(UnavailableFeatureDialogMediatorTest, Simple) {
  NSString* kFeatureName = @"Feature Name";
  DialogMediator* mediator = [[UnavailableFeatureDialogMediator alloc]
      initWithFeatureName:kFeatureName];
  EXPECT_NSEQ(kFeatureName, [mediator dialogMessage]);
  EXPECT_EQ(1U, [mediator buttonConfigs].count);
  DialogButtonConfiguration* button = [mediator buttonConfigs][0];
  EXPECT_NSEQ(@"OK", button.text);
  EXPECT_EQ(DialogButtonStyle::DEFAULT, button.style);
}
