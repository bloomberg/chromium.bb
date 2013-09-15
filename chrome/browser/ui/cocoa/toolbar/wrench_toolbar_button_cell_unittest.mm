// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/wrench_toolbar_button_cell.h"

#include "base/message_loop/message_loop.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

@interface TestWrenchToolbarButton : NSButton
@end

@implementation TestWrenchToolbarButton

+ (Class)cellClass {
  return [WrenchToolbarButtonCell class];
}

@end

class WrenchToolbarButtonCellTest : public CocoaTest {
 protected:
  WrenchToolbarButtonCellTest() {
    base::scoped_nsobject<NSButton> button([[TestWrenchToolbarButton alloc]
        initWithFrame:NSMakeRect(0, 0, 29, 29)]);
    button_ = button;
    [[test_window() contentView] addSubview:button_];
  }

  NSButton* button_;
  base::scoped_nsobject<WrenchToolbarButtonCell> cell_;
  base::MessageLoopForUI message_loop_;  // Needed for gfx::Animation.

 private:
  DISALLOW_COPY_AND_ASSIGN(WrenchToolbarButtonCellTest);
};

TEST_VIEW(WrenchToolbarButtonCellTest, button_)
