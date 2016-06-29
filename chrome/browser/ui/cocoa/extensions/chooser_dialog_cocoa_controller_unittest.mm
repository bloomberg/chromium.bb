// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/chooser_dialog_cocoa_controller.h"

#import <Cocoa/Cocoa.h>

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/chooser_content_view_cocoa.h"
#import "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/extensions/chooser_dialog_cocoa.h"
#include "components/chooser_controller/mock_chooser_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

class ChooserDialogCocoaControllerTest : public CocoaProfileTest {
 protected:
  ChooserDialogCocoaControllerTest() {}
  ~ChooserDialogCocoaControllerTest() override {}

  void SetUp() override {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());
  }

  // Create a ChooserDialogCocoa.
  void CreateChooserDialog() {
    content::WebContents* web_contents =
        content::WebContents::Create(content::WebContents::CreateParams(
            profile(), content::SiteInstance::Create(profile())));
    ASSERT_TRUE(web_contents);
    std::unique_ptr<MockChooserController> chooser_controller(
        new MockChooserController(web_contents->GetMainFrame()));
    ASSERT_TRUE(chooser_controller);
    chooser_controller_ = chooser_controller.get();
    chooser_dialog_.reset(
        new ChooserDialogCocoa(web_contents, std::move(chooser_controller)));
    ASSERT_TRUE(chooser_dialog_);
    chooser_dialog_controller_ =
        chooser_dialog_->chooser_dialog_cocoa_controller_.get();
    ASSERT_TRUE(chooser_dialog_controller_);
    chooser_content_view_ = [chooser_dialog_controller_ chooserContentView];
    ASSERT_TRUE(chooser_content_view_);
    table_view_ = [chooser_content_view_ tableView];
    ASSERT_TRUE(table_view_);
    connect_button_ = [chooser_content_view_ connectButton];
    ASSERT_TRUE(connect_button_);
    cancel_button_ = [chooser_content_view_ cancelButton];
    ASSERT_TRUE(cancel_button_);
    help_button_ = [chooser_content_view_ helpButton];
    ASSERT_TRUE(help_button_);
  }

  std::unique_ptr<ChooserDialogCocoa> chooser_dialog_;

  MockChooserController* chooser_controller_;
  ChooserDialogCocoaController* chooser_dialog_controller_;
  ChooserContentViewCocoa* chooser_content_view_;
  NSTableView* table_view_;
  NSButton* connect_button_;
  NSButton* cancel_button_;
  NSButton* help_button_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChooserDialogCocoaControllerTest);
};

TEST_F(ChooserDialogCocoaControllerTest, InitialState) {
  CreateChooserDialog();

  // Since "No devices found." needs to be displayed on the |table_view_|,
  // the number of rows is 1.
  EXPECT_EQ(1, table_view_.numberOfRows);
  EXPECT_EQ(1, table_view_.numberOfColumns);
  // |table_view_| should be disabled since there is no option shown.
  ASSERT_FALSE(table_view_.enabled);
  // No option selected.
  EXPECT_EQ(-1, table_view_.selectedRow);
  // |connect_button_| should be disabled since no option selected.
  ASSERT_FALSE(connect_button_.enabled);
  ASSERT_TRUE(cancel_button_.enabled);
  ASSERT_TRUE(help_button_.enabled);
}

TEST_F(ChooserDialogCocoaControllerTest, AddOption) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  EXPECT_EQ(1, table_view_.numberOfRows);
  EXPECT_EQ(1, table_view_.numberOfColumns);
  // |table_view_| should be enabled since there is an option.
  ASSERT_TRUE(table_view_.enabled);
  EXPECT_EQ(-1, table_view_.selectedRow);
  ASSERT_FALSE(connect_button_.enabled);
  ASSERT_TRUE(cancel_button_.enabled);
  ASSERT_TRUE(help_button_.enabled);

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  EXPECT_EQ(2, table_view_.numberOfRows);
  EXPECT_EQ(1, table_view_.numberOfColumns);
  ASSERT_TRUE(table_view_.enabled);
  EXPECT_EQ(-1, table_view_.selectedRow);

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));
  EXPECT_EQ(3, table_view_.numberOfRows);
  EXPECT_EQ(1, table_view_.numberOfColumns);
  ASSERT_TRUE(table_view_.enabled);
  EXPECT_EQ(-1, table_view_.selectedRow);
}

TEST_F(ChooserDialogCocoaControllerTest, RemoveOption) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("b"));
  EXPECT_EQ(2, table_view_.numberOfRows);
  EXPECT_EQ(1, table_view_.numberOfColumns);
  ASSERT_TRUE(table_view_.enabled);
  EXPECT_EQ(-1, table_view_.selectedRow);

  // Remove a non-existent option, the number of rows should not change.
  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("non-existent"));
  EXPECT_EQ(2, table_view_.numberOfRows);
  EXPECT_EQ(1, table_view_.numberOfColumns);
  ASSERT_TRUE(table_view_.enabled);
  EXPECT_EQ(-1, table_view_.selectedRow);

  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("c"));
  EXPECT_EQ(1, table_view_.numberOfRows);
  EXPECT_EQ(1, table_view_.numberOfColumns);
  ASSERT_TRUE(table_view_.enabled);
  EXPECT_EQ(-1, table_view_.selectedRow);

  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("a"));
  // There is no option shown now. But since "No devices found."
  // needs to be displayed on the |table_view_|, the number of rows is 1.
  EXPECT_EQ(1, table_view_.numberOfRows);
  EXPECT_EQ(1, table_view_.numberOfColumns);
  // |table_view_| should be disabled since all options are removed.
  ASSERT_FALSE(table_view_.enabled);
  EXPECT_EQ(-1, table_view_.selectedRow);
}

TEST_F(ChooserDialogCocoaControllerTest, AddAndRemoveOption) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  EXPECT_EQ(1, table_view_.numberOfRows);
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  EXPECT_EQ(2, table_view_.numberOfRows);
  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("b"));
  EXPECT_EQ(1, table_view_.numberOfRows);
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));
  EXPECT_EQ(2, table_view_.numberOfRows);
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("d"));
  EXPECT_EQ(3, table_view_.numberOfRows);
  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("d"));
  EXPECT_EQ(2, table_view_.numberOfRows);
  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("c"));
  EXPECT_EQ(1, table_view_.numberOfRows);
}

TEST_F(ChooserDialogCocoaControllerTest, SelectAndDeselectAnOption) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  // Select option 0.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
           byExtendingSelection:NO];
  EXPECT_EQ(0, table_view_.selectedRow);
  ASSERT_TRUE(connect_button_.enabled);

  // Deselect option 0.
  [table_view_ deselectRow:0];
  EXPECT_EQ(-1, table_view_.selectedRow);
  ASSERT_FALSE(connect_button_.enabled);

  // Select option 1.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:1]
           byExtendingSelection:NO];
  EXPECT_EQ(1, table_view_.selectedRow);
  ASSERT_TRUE(connect_button_.enabled);

  // Deselect option 1.
  [table_view_ deselectRow:1];
  EXPECT_EQ(-1, table_view_.selectedRow);
  ASSERT_FALSE(connect_button_.enabled);
}

TEST_F(ChooserDialogCocoaControllerTest,
       SelectAnOptionAndThenSelectAnotherOption) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  // Select option 0.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
           byExtendingSelection:NO];
  EXPECT_EQ(0, table_view_.selectedRow);
  ASSERT_TRUE(connect_button_.enabled);

  // Select option 1.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:1]
           byExtendingSelection:NO];
  EXPECT_EQ(1, table_view_.selectedRow);
  ASSERT_TRUE(connect_button_.enabled);

  // Select option 2.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:2]
           byExtendingSelection:NO];
  EXPECT_EQ(2, table_view_.selectedRow);
  ASSERT_TRUE(connect_button_.enabled);
}

TEST_F(ChooserDialogCocoaControllerTest, SelectAnOptionAndRemoveAnotherOption) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  // Select option 1.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:1]
           byExtendingSelection:NO];
  EXPECT_EQ(3, table_view_.numberOfRows);
  EXPECT_EQ(1, table_view_.selectedRow);
  ASSERT_TRUE(connect_button_.enabled);

  // Remove option 0. The list becomes: b c. And the index of the previously
  // selected item "b" becomes 0.
  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("a"));
  EXPECT_EQ(2, table_view_.numberOfRows);
  EXPECT_EQ(0, table_view_.selectedRow);
  ASSERT_TRUE(connect_button_.enabled);

  // Remove option 1. The list becomes: b.
  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("c"));
  EXPECT_EQ(1, table_view_.numberOfRows);
  EXPECT_EQ(0, table_view_.selectedRow);
  ASSERT_TRUE(connect_button_.enabled);
}

TEST_F(ChooserDialogCocoaControllerTest,
       SelectAnOptionAndRemoveTheSelectedOption) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  // Select option 1.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:1]
           byExtendingSelection:NO];
  EXPECT_EQ(3, table_view_.numberOfRows);
  EXPECT_EQ(1, table_view_.selectedRow);
  ASSERT_TRUE(connect_button_.enabled);

  // Remove option 1
  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("b"));
  EXPECT_EQ(2, table_view_.numberOfRows);
  // No option selected.
  EXPECT_EQ(-1, table_view_.selectedRow);
  // Since no option selected, the "Connect" button should be disabled.
  ASSERT_FALSE(connect_button_.enabled);
}

TEST_F(ChooserDialogCocoaControllerTest,
       AddAnOptionAndSelectItAndRemoveTheSelectedOption) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));

  // Select option 0.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
           byExtendingSelection:NO];
  EXPECT_EQ(1, table_view_.numberOfRows);
  EXPECT_EQ(0, table_view_.selectedRow);
  ASSERT_TRUE(connect_button_.enabled);

  // Remove option 0.
  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("a"));
  // There is no option shown now. But since "No devices found."
  // needs to be displayed on the |table_view_|, the number of rows is 1.
  EXPECT_EQ(1, table_view_.numberOfRows);
  // No option selected.
  EXPECT_EQ(-1, table_view_.selectedRow);
  // |table_view_| should be disabled since there is no option shown.
  ASSERT_FALSE(table_view_.enabled);
  // Since no option selected, the "Connect" button should be disabled.
  ASSERT_FALSE(connect_button_.enabled);
}

TEST_F(ChooserDialogCocoaControllerTest, NoOptionSelectedAndPressCancelButton) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  EXPECT_CALL(*chooser_controller_, Select(testing::_)).Times(0);
  EXPECT_CALL(*chooser_controller_, Cancel()).Times(1);
  [cancel_button_ performClick:chooser_dialog_controller_];
}

TEST_F(ChooserDialogCocoaControllerTest, SelectAnOptionAndPressConnectButton) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  // Select option 0 and press "Connect" button.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
           byExtendingSelection:NO];
  EXPECT_CALL(*chooser_controller_, Select(0)).Times(1);
  EXPECT_CALL(*chooser_controller_, Cancel()).Times(0);
  [connect_button_ performClick:chooser_dialog_controller_];

  // Select option 2 and press "Connect" button.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:2]
           byExtendingSelection:NO];
  EXPECT_CALL(*chooser_controller_, Select(2)).Times(1);
  EXPECT_CALL(*chooser_controller_, Cancel()).Times(0);
  [connect_button_ performClick:chooser_dialog_controller_];
}

TEST_F(ChooserDialogCocoaControllerTest, SelectAnOptionAndPressCancelButton) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  // Select option 0 and press "Cancel" button.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
           byExtendingSelection:NO];
  EXPECT_CALL(*chooser_controller_, Select(testing::_)).Times(0);
  EXPECT_CALL(*chooser_controller_, Cancel()).Times(1);
  [cancel_button_ performClick:chooser_dialog_controller_];
}

TEST_F(ChooserDialogCocoaControllerTest, PressHelpButton) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  // Select option 0 and press "Get help" button.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
           byExtendingSelection:NO];
  EXPECT_CALL(*chooser_controller_, Select(testing::_)).Times(0);
  EXPECT_CALL(*chooser_controller_, Cancel()).Times(0);
  EXPECT_CALL(*chooser_controller_, OpenHelpCenterUrl()).Times(1);
  [help_button_ performClick:chooser_dialog_controller_];
}
