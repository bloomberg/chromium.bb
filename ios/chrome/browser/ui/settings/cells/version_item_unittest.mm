// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/version_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(VersionItemTest, TextLabelGetsText) {
  VersionItem* item = [[VersionItem alloc] initWithType:0];
  VersionCell* cell = [[[item cellClass] alloc] init];
  EXPECT_TRUE([cell isMemberOfClass:[VersionCell class]]);

  item.text = @"Foo";
  [item configureCell:cell];
  EXPECT_EQ(@"Foo", cell.textLabel.text);
}

}  // namespace
