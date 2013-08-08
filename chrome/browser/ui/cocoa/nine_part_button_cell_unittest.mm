// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/nine_part_button_cell.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

class NinePartButtonCellTest : public CocoaTest {
 public:
  NinePartButtonCellTest() {
    const int resourceIds[9] = {
      IDR_MANAGED_USER_LABEL_TOP_LEFT, IDR_MANAGED_USER_LABEL_TOP,
      IDR_MANAGED_USER_LABEL_TOP_RIGHT, IDR_MANAGED_USER_LABEL_LEFT,
      IDR_MANAGED_USER_LABEL_CENTER, IDR_MANAGED_USER_LABEL_RIGHT,
      IDR_MANAGED_USER_LABEL_BOTTOM_LEFT, IDR_MANAGED_USER_LABEL_BOTTOM,
      IDR_MANAGED_USER_LABEL_BOTTOM_RIGHT
    };
    NSRect content_frame = [[test_window() contentView] frame];
    button_.reset([[NSButton alloc] initWithFrame:content_frame]);
    base::scoped_nsobject<NinePartButtonCell> cell(
        [[NinePartButtonCell alloc] initWithResourceIds:resourceIds]);
    [button_ setCell:cell.get()];
    [[test_window() contentView] addSubview:button_];
  }

 protected:
  base::scoped_nsobject<NSButton> button_;
};

TEST_VIEW(NinePartButtonCellTest, button_)
