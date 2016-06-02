// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/chooser_dialog_cocoa_controller.h"

#import <Cocoa/Cocoa.h>

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/chooser_content_view.h"
#import "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/extensions/chooser_dialog_cocoa.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

class MockChooserController : public ChooserController {
 public:
  explicit MockChooserController(content::RenderFrameHost* owner)
      : ChooserController(owner) {}
  ~MockChooserController() override {}

  // ChooserController:
  size_t NumOptions() const override { return option_names_.size(); }

  // ChooserController:
  const base::string16& GetOption(size_t index) const override {
    return option_names_[index];
  }

  // ChooserController:
  MOCK_METHOD1(Select, void(size_t index));

  // ChooserController:
  MOCK_METHOD0(Cancel, void());

  // ChooserController:
  MOCK_METHOD0(Close, void());

  // ChooserController:
  MOCK_CONST_METHOD0(OpenHelpCenterUrl, void());

  void OptionAdded(const base::string16 option_name) {
    option_names_.push_back(option_name);
    if (observer())
      observer()->OnOptionAdded(option_names_.size() - 1);
  }

  void OptionRemoved(const base::string16 option_name) {
    for (auto it = option_names_.begin(); it != option_names_.end(); ++it) {
      if (*it == option_name) {
        size_t index = it - option_names_.begin();
        option_names_.erase(it);
        if (observer())
          observer()->OnOptionRemoved(index);
        return;
      }
    }
  }

 private:
  std::vector<base::string16> option_names_;

  DISALLOW_COPY_AND_ASSIGN(MockChooserController);
};

}  // namespace

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
    chooser_controller_.reset(
        new MockChooserController(web_contents->GetMainFrame()));
    ASSERT_TRUE(chooser_controller_);
    chooser_dialog_.reset(
        new ChooserDialogCocoa(web_contents, chooser_controller_.get()));
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

  std::unique_ptr<MockChooserController> chooser_controller_;
  std::unique_ptr<ChooserDialogCocoa> chooser_dialog_;

  ChooserDialogCocoaController* chooser_dialog_controller_;
  ChooserContentView* chooser_content_view_;
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
  EXPECT_EQ(table_view_.numberOfRows, 1);
  EXPECT_EQ(table_view_.numberOfColumns, 1);
  // |table_view_| should be disabled since there is no option shown.
  ASSERT_FALSE(table_view_.enabled);
  // No option selected.
  EXPECT_EQ(table_view_.selectedRow, -1);
  // |connect_button_| should be disabled since no option selected.
  ASSERT_FALSE(connect_button_.enabled);
  ASSERT_TRUE(cancel_button_.enabled);
  ASSERT_TRUE(help_button_.enabled);
}

TEST_F(ChooserDialogCocoaControllerTest, AddOption) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  EXPECT_EQ(table_view_.numberOfRows, 1);
  EXPECT_EQ(table_view_.numberOfColumns, 1);
  // |table_view_| should be enabled since there is an option.
  ASSERT_TRUE(table_view_.enabled);
  EXPECT_EQ(table_view_.selectedRow, -1);
  ASSERT_FALSE(connect_button_.enabled);
  ASSERT_TRUE(cancel_button_.enabled);
  ASSERT_TRUE(help_button_.enabled);

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  EXPECT_EQ(table_view_.numberOfRows, 2);
  EXPECT_EQ(table_view_.numberOfColumns, 1);
  ASSERT_TRUE(table_view_.enabled);
  EXPECT_EQ(table_view_.selectedRow, -1);

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));
  EXPECT_EQ(table_view_.numberOfRows, 3);
  EXPECT_EQ(table_view_.numberOfColumns, 1);
  ASSERT_TRUE(table_view_.enabled);
  EXPECT_EQ(table_view_.selectedRow, -1);
}

TEST_F(ChooserDialogCocoaControllerTest, RemoveOption) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("b"));
  EXPECT_EQ(table_view_.numberOfRows, 2);
  EXPECT_EQ(table_view_.numberOfColumns, 1);
  ASSERT_TRUE(table_view_.enabled);
  EXPECT_EQ(table_view_.selectedRow, -1);

  // Remove a non-existent option, the number of rows should not change.
  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("non-existent"));
  EXPECT_EQ(table_view_.numberOfRows, 2);
  EXPECT_EQ(table_view_.numberOfColumns, 1);
  ASSERT_TRUE(table_view_.enabled);
  EXPECT_EQ(table_view_.selectedRow, -1);

  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("c"));
  EXPECT_EQ(table_view_.numberOfRows, 1);
  EXPECT_EQ(table_view_.numberOfColumns, 1);
  ASSERT_TRUE(table_view_.enabled);
  EXPECT_EQ(table_view_.selectedRow, -1);

  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("a"));
  // There is no option shown now. But since "No devices found."
  // needs to be displayed on the |table_view_|, the number of rows is 1.
  EXPECT_EQ(table_view_.numberOfRows, 1);
  EXPECT_EQ(table_view_.numberOfColumns, 1);
  // |table_view_| should be disabled since all options are removed.
  ASSERT_FALSE(table_view_.enabled);
  EXPECT_EQ(table_view_.selectedRow, -1);
}

TEST_F(ChooserDialogCocoaControllerTest, AddAndRemoveOption) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  EXPECT_EQ(table_view_.numberOfRows, 1);
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  EXPECT_EQ(table_view_.numberOfRows, 2);
  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("b"));
  EXPECT_EQ(table_view_.numberOfRows, 1);
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));
  EXPECT_EQ(table_view_.numberOfRows, 2);
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("d"));
  EXPECT_EQ(table_view_.numberOfRows, 3);
  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("d"));
  EXPECT_EQ(table_view_.numberOfRows, 2);
  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("c"));
  // There is no option shown now. But since "No devices found."
  // needs to be displayed on the |table_view_|, the number of rows is 1.
  EXPECT_EQ(table_view_.numberOfRows, 1);
}

TEST_F(ChooserDialogCocoaControllerTest, SelectAndDeselectAnOption) {
  CreateChooserDialog();

  chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  // Select option 0.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
           byExtendingSelection:NO];
  EXPECT_EQ(table_view_.selectedRow, 0);
  ASSERT_TRUE(connect_button_.enabled);

  // Disselect option 0.
  [table_view_ deselectRow:0];
  EXPECT_EQ(table_view_.selectedRow, -1);
  ASSERT_FALSE(connect_button_.enabled);

  // Select option 1.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:1]
           byExtendingSelection:NO];
  EXPECT_EQ(table_view_.selectedRow, 1);
  ASSERT_TRUE(connect_button_.enabled);

  // Disselect option 1.
  [table_view_ deselectRow:1];
  EXPECT_EQ(table_view_.selectedRow, -1);
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
  EXPECT_EQ(table_view_.selectedRow, 0);
  ASSERT_TRUE(connect_button_.enabled);

  // Select option 1.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:1]
           byExtendingSelection:NO];
  EXPECT_EQ(table_view_.selectedRow, 1);
  ASSERT_TRUE(connect_button_.enabled);

  // Select option 2.
  [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:2]
           byExtendingSelection:NO];
  EXPECT_EQ(table_view_.selectedRow, 2);
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
  EXPECT_EQ(table_view_.numberOfRows, 3);
  EXPECT_EQ(table_view_.selectedRow, 1);
  ASSERT_TRUE(connect_button_.enabled);

  // Remove option 1
  chooser_controller_->OptionRemoved(base::ASCIIToUTF16("b"));
  EXPECT_EQ(table_view_.numberOfRows, 2);
  // No option selected.
  EXPECT_EQ(table_view_.selectedRow, -1);
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
