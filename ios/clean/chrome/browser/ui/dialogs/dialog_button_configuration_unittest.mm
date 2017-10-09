// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"

#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_style.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_configuration_identifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Const values from which to create configurations.
NSString* const kButtonText = @"button text";
const DialogButtonStyle kButtonStyle = DialogButtonStyle::DESTRUCTIVE;
}

using DialogButtonConfigurationTest = PlatformTest;

// Tests that the values passed to the factory method are reflected in the
// returned value.
TEST_F(DialogButtonConfigurationTest, FactoryMethod) {
  DialogButtonConfiguration* config =
      [DialogButtonConfiguration configWithText:kButtonText style:kButtonStyle];
  EXPECT_NSEQ(kButtonText, config.text);
  EXPECT_EQ(kButtonStyle, config.style);
}

// Tests that two DialogButtonConfigurations created with the same values have
// unequal identifiers.
TEST_F(DialogButtonConfigurationTest, Identifiers) {
  DialogButtonConfiguration* config1 =
      [DialogButtonConfiguration configWithText:kButtonText style:kButtonStyle];
  DialogButtonConfiguration* config2 =
      [DialogButtonConfiguration configWithText:kButtonText style:kButtonStyle];
  EXPECT_FALSE([config1.identifier isEqual:config2.identifier]);
}
