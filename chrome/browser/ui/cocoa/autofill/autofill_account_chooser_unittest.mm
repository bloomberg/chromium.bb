// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_account_chooser.h"

#import <Cocoa/Cocoa.h>

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_controller.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/models/simple_menu_model.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

namespace {

class AutofillAccountChooserTest : public ui::CocoaTest {
 public:
  AutofillAccountChooserTest() {
    NSRect frame = NSMakeRect(0, 0, 500, 200);
    view_.reset([[AutofillAccountChooser alloc] initWithFrame:frame
                                                   controller:&controller_]);
    [[test_window() contentView] addSubview:view_];
 }

 protected:
  scoped_nsobject<AutofillAccountChooser> view_;
  testing::NiceMock<autofill::MockAutofillDialogController> controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillAccountChooserTest);
};

class MenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }

  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    return true;
  }

  virtual bool GetAcceleratorForCommandId(
        int command_id,
        ui::Accelerator* accelerator) OVERRIDE { return false; }

  virtual void ExecuteCommand(int command_id, int event_flags) {}
};

}  // namespace

TEST_VIEW(AutofillAccountChooserTest, view_);

// Make sure account chooser has all required subviews
TEST_F(AutofillAccountChooserTest, SubViews) {
  bool hasIcon = false;
  bool hasLink = false;
  bool hasPopup = false;

  for (id view in [view_ subviews]) {
    if ([view isKindOfClass:[NSImageView class]]) {
      hasIcon = true;
    } else if ([view isKindOfClass:[MenuButton class]]) {
      hasPopup = true;
    } else if ([view isKindOfClass:[NSButton class]])  {
      if ([[view cell] isKindOfClass:[HyperlinkButtonCell class]])
        hasLink = true;
    }
  }

  EXPECT_TRUE(hasIcon);
  EXPECT_TRUE(hasLink);
  EXPECT_TRUE(hasPopup);
}

// Validate that the account menu is properly populated.
TEST_F(AutofillAccountChooserTest, PopulatesMenu) {
  using namespace testing;
  MenuDelegate delegate;
  ui::SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  model.AddItem(2, ASCIIToUTF16("two"));

  EXPECT_CALL(controller_, MenuModelForAccountChooser())
      .WillOnce(Return(&model));
  [view_ update];

  MenuButton* button = nil;
  for (id view in [view_ subviews]) {
    if ([view isKindOfClass:[MenuButton class]]) {
      button = view;
      break;
    }
  }

  NSArray* items = [[button attachedMenu] itemArray];
  EXPECT_EQ(3U, [items count]);
  EXPECT_NSEQ(@"", [[items objectAtIndex:0] title]);
  EXPECT_NSEQ(@"one", [[items objectAtIndex:1] title]);
  EXPECT_NSEQ(@"two", [[items objectAtIndex:2] title]);
}
