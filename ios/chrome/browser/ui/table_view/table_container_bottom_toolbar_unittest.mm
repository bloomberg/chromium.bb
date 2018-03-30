// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/table_container_bottom_toolbar.h"

#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using TableContainerBottomToolbarTest = PlatformTest;

TEST_F(TableContainerBottomToolbarTest, ButtonsText) {
  TableContainerBottomToolbar* toolbar =
      [[TableContainerBottomToolbar alloc] initWithLeadingButtonText:@"Left"
                                                  trailingButtonText:@"Right"];

  EXPECT_EQ(@"Left", toolbar.leadingButton.titleLabel.text);
  EXPECT_EQ(@"Right", toolbar.trailingButton.titleLabel.text);
}

TEST_F(TableContainerBottomToolbarTest, NoButtonsWithNil) {
  TableContainerBottomToolbar* toolbar =
      [[TableContainerBottomToolbar alloc] initWithLeadingButtonText:@"Left"
                                                  trailingButtonText:nil];
  EXPECT_EQ(@"Left", toolbar.leadingButton.titleLabel.text);
  EXPECT_EQ(nil, toolbar.trailingButton);

  toolbar =
      [[TableContainerBottomToolbar alloc] initWithLeadingButtonText:nil
                                                  trailingButtonText:@"Right"];
  EXPECT_EQ(nil, toolbar.leadingButton);
  EXPECT_EQ(@"Right", toolbar.trailingButton.titleLabel.text);

  toolbar = [[TableContainerBottomToolbar alloc] initWithLeadingButtonText:nil
                                                        trailingButtonText:nil];
  EXPECT_EQ(nil, toolbar.leadingButton);
  EXPECT_EQ(nil, toolbar.trailingButton);
}

TEST_F(TableContainerBottomToolbarTest, NoButtonsWithEmptyString) {
  TableContainerBottomToolbar* toolbar =
      [[TableContainerBottomToolbar alloc] initWithLeadingButtonText:@"Left"
                                                  trailingButtonText:@""];
  EXPECT_EQ(@"Left", toolbar.leadingButton.titleLabel.text);
  EXPECT_EQ(nil, toolbar.trailingButton);

  toolbar =
      [[TableContainerBottomToolbar alloc] initWithLeadingButtonText:@""
                                                  trailingButtonText:@"Right"];
  EXPECT_EQ(nil, toolbar.leadingButton);
  EXPECT_EQ(@"Right", toolbar.trailingButton.titleLabel.text);

  toolbar = [[TableContainerBottomToolbar alloc] initWithLeadingButtonText:@""
                                                        trailingButtonText:@""];
  EXPECT_EQ(nil, toolbar.leadingButton);
  EXPECT_EQ(nil, toolbar.trailingButton);
}

}  // namespace
