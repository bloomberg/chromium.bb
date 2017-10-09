// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/dialog_configuration_identifier.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using DialogConfigurationIdentifierTest = PlatformTest;

// Tests that two DialogConfigurationIdentifiers created sequentially are not
// equal to each other.
TEST_F(DialogConfigurationIdentifierTest, NotEqual) {
  DialogConfigurationIdentifier* ID1 =
      [[DialogConfigurationIdentifier alloc] init];
  DialogConfigurationIdentifier* ID2 =
      [[DialogConfigurationIdentifier alloc] init];
  EXPECT_FALSE([ID1 isEqual:ID2]);
}

// Tests that copying a DialogConfigurationIdentifier creates one that is equal
// to the original.
TEST_F(DialogConfigurationIdentifierTest, EqualCopies) {
  DialogConfigurationIdentifier* identifier =
      [[DialogConfigurationIdentifier alloc] init];
  DialogConfigurationIdentifier* copy = [identifier copy];
  EXPECT_NSEQ(identifier, copy);
}
