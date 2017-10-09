// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/dialog_text_field_configuration.h"

#import "ios/clean/chrome/browser/ui/dialogs/dialog_configuration_identifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Const values from which to create configurations.
NSString* const kDefaultText = @"default text";
NSString* const kPlaceholderText = @"placeholder text";
const BOOL kSecure = YES;
}

using DialogTextFieldConfigurationTest = PlatformTest;

// Tests that the values passed to the factory method are reflected in the
// returned value.
TEST_F(DialogTextFieldConfigurationTest, FactoryMethod) {
  DialogTextFieldConfiguration* config =
      [DialogTextFieldConfiguration configWithDefaultText:kDefaultText
                                          placeholderText:kPlaceholderText
                                                   secure:kSecure];
  EXPECT_NSEQ(kDefaultText, config.defaultText);
  EXPECT_NSEQ(kPlaceholderText, config.placeholderText);
  EXPECT_EQ(kSecure, config.secure);
}

// Tests that two DialogTextFieldConfigurations created with the same values
// have unequal identifiers.
TEST_F(DialogTextFieldConfigurationTest, Identifiers) {
  DialogTextFieldConfiguration* config1 =
      [DialogTextFieldConfiguration configWithDefaultText:kDefaultText
                                          placeholderText:kPlaceholderText
                                                   secure:kSecure];
  DialogTextFieldConfiguration* config2 =
      [DialogTextFieldConfiguration configWithDefaultText:kDefaultText
                                          placeholderText:kPlaceholderText
                                                   secure:kSecure];
  EXPECT_FALSE([config1.identifier isEqual:config2.identifier]);
}
