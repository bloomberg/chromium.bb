// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_cell.h"

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

namespace {

class OmniboxPopupCellTest : public CocoaTest {
 public:
  OmniboxPopupCellTest() {
  }

  virtual void SetUp() OVERRIDE {
    CocoaTest::SetUp();
    cell_.reset([[OmniboxPopupCell alloc] initTextCell:@""]);
    button_.reset([[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 200, 20)]);
    [button_ setCell:cell_];
    [[test_window() contentView] addSubview:button_];
  };

 protected:
  base::scoped_nsobject<OmniboxPopupCell> cell_;
  base::scoped_nsobject<NSButton> button_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupCellTest);
};

TEST_VIEW(OmniboxPopupCellTest, button_);

TEST_F(OmniboxPopupCellTest, Image) {
  [cell_ setImage:[NSImage imageNamed:NSImageNameInfo]];
  [button_ display];
}

TEST_F(OmniboxPopupCellTest, Title) {
  base::scoped_nsobject<NSAttributedString> text([[NSAttributedString alloc]
      initWithString:@"The quick brown fox jumps over the lazy dog."]);
  [cell_ setAttributedTitle:text];
  [button_ display];
}

}  // namespace
