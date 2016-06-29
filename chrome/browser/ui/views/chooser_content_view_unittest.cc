// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chooser_content_view.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/chooser_controller/mock_chooser_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"

namespace {

class MockTableViewObserver : public views::TableViewObserver {
 public:
  // views::TableViewObserver:
  MOCK_METHOD0(OnSelectionChanged, void());
};

class MockStyledLabelListener : public views::StyledLabelListener {
 public:
  // views::StyledLabelListener:
  MOCK_METHOD3(StyledLabelLinkClicked,
               void(views::StyledLabel* label,
                    const gfx::Range& range,
                    int event_flags));
};

}  // namespace

class ChooserContentViewTest : public testing::Test {
 public:
  ChooserContentViewTest() {}

  // testing::Test:
  void SetUp() override {
    std::unique_ptr<MockChooserController> mock_chooser_controller(
        new MockChooserController(nullptr));
    mock_chooser_controller_ = mock_chooser_controller.get();
    mock_table_view_observer_.reset(new MockTableViewObserver());
    chooser_content_view_.reset(new ChooserContentView(
        mock_table_view_observer_.get(), std::move(mock_chooser_controller)));
    table_view_ = chooser_content_view_->table_view_for_test();
    ASSERT_TRUE(table_view_);
    table_model_ = table_view_->model();
    ASSERT_TRUE(table_model_);
    mock_styled_label_listener_.reset(new MockStyledLabelListener());
    styled_label_.reset(chooser_content_view_->CreateFootnoteView(
        mock_styled_label_listener_.get()));
    ASSERT_TRUE(styled_label_);
  }

 protected:
  std::unique_ptr<MockTableViewObserver> mock_table_view_observer_;
  std::unique_ptr<ChooserContentView> chooser_content_view_;
  MockChooserController* mock_chooser_controller_;
  views::TableView* table_view_;
  ui::TableModel* table_model_;
  std::unique_ptr<MockStyledLabelListener> mock_styled_label_listener_;
  std::unique_ptr<views::StyledLabel> styled_label_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChooserContentViewTest);
};

TEST_F(ChooserContentViewTest, InitialState) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(0);

  // Since "No devices found." needs to be displayed on the |table_view_|,
  // the number of rows is 1.
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT),
      table_model_->GetText(0, 0));
  // |table_view_| should be disabled since there is no option shown.
  EXPECT_FALSE(table_view_->enabled());
  // No option selected.
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(ChooserContentViewTest, AddOption) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(0);

  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(base::ASCIIToUTF16("a"), table_model_->GetText(0, 0));
  // |table_view_| should be enabled since there is an option.
  EXPECT_TRUE(table_view_->enabled());
  // No option selected.
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  EXPECT_EQ(2, table_view_->RowCount());
  EXPECT_EQ(base::ASCIIToUTF16("b"), table_model_->GetText(1, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));
  EXPECT_EQ(3, table_view_->RowCount());
  EXPECT_EQ(base::ASCIIToUTF16("c"), table_model_->GetText(2, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(ChooserContentViewTest, RemoveOption) {
  // Called from TableView::OnItemsRemoved().
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(3);

  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("b"));
  EXPECT_EQ(2, table_view_->RowCount());
  EXPECT_EQ(base::ASCIIToUTF16("a"), table_model_->GetText(0, 0));
  EXPECT_EQ(base::ASCIIToUTF16("c"), table_model_->GetText(1, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  // Remove a non-existent option, the number of rows should not change.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("non-existent"));
  EXPECT_EQ(2, table_view_->RowCount());
  EXPECT_EQ(base::ASCIIToUTF16("a"), table_model_->GetText(0, 0));
  EXPECT_EQ(base::ASCIIToUTF16("c"), table_model_->GetText(1, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("c"));
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(base::ASCIIToUTF16("a"), table_model_->GetText(0, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("a"));
  // There is no option shown now. But since "No devices found."
  // needs to be displayed on the |table_view_|, the number of rows is 1.
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT),
      table_model_->GetText(0, 0));
  // |table_view_| should be disabled since all options are removed.
  EXPECT_FALSE(table_view_->enabled());
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(ChooserContentViewTest, AddAndRemoveOption) {
  // Called from TableView::OnItemsRemoved().
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(3);

  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  EXPECT_EQ(1, table_view_->RowCount());
  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  EXPECT_EQ(2, table_view_->RowCount());
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("b"));
  EXPECT_EQ(1, table_view_->RowCount());
  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));
  EXPECT_EQ(2, table_view_->RowCount());
  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("d"));
  EXPECT_EQ(3, table_view_->RowCount());
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("d"));
  EXPECT_EQ(2, table_view_->RowCount());
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("c"));
  EXPECT_EQ(1, table_view_->RowCount());
}

TEST_F(ChooserContentViewTest, SelectAndDeselectAnOption) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(4);

  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  // Select option 0.
  table_view_->Select(0);
  EXPECT_EQ(1, table_view_->SelectedRowCount());
  EXPECT_EQ(0, table_view_->FirstSelectedRow());

  // Unselect option 0.
  table_view_->Select(-1);
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  // Select option 1.
  table_view_->Select(1);
  EXPECT_EQ(1, table_view_->SelectedRowCount());
  EXPECT_EQ(1, table_view_->FirstSelectedRow());

  // Unselect option 1.
  table_view_->Select(-1);
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(ChooserContentViewTest, SelectAnOptionAndThenSelectAnotherOption) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(3);

  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  // Select option 0.
  table_view_->Select(0);
  EXPECT_EQ(1, table_view_->SelectedRowCount());
  EXPECT_EQ(0, table_view_->FirstSelectedRow());

  // Select option 1.
  table_view_->Select(1);
  EXPECT_EQ(1, table_view_->SelectedRowCount());
  EXPECT_EQ(1, table_view_->FirstSelectedRow());

  // Select option 2.
  table_view_->Select(2);
  EXPECT_EQ(1, table_view_->SelectedRowCount());
  EXPECT_EQ(2, table_view_->FirstSelectedRow());
}

TEST_F(ChooserContentViewTest, SelectAnOptionAndRemoveAnotherOption) {
  // Called one time from TableView::Select() and two times from
  // TableView::OnItemsRemoved().
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(3);

  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  // Select option 1.
  table_view_->Select(1);
  EXPECT_EQ(1, table_view_->SelectedRowCount());
  EXPECT_EQ(1, table_view_->FirstSelectedRow());

  // Remove option 0, the list becomes: b c.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("a"));
  EXPECT_EQ(2, table_view_->RowCount());
  EXPECT_EQ(1, table_view_->SelectedRowCount());
  // Since option 0 is removed, the original selected option 1 becomes
  // the first option in the list.
  EXPECT_EQ(0, table_view_->FirstSelectedRow());

  // Remove option 1.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("c"));
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(1, table_view_->SelectedRowCount());
  EXPECT_EQ(0, table_view_->FirstSelectedRow());
}

TEST_F(ChooserContentViewTest, SelectAnOptionAndRemoveTheSelectedOption) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(2);

  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));
  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("b"));
  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("c"));

  // Select option 1.
  table_view_->Select(1);
  EXPECT_EQ(1, table_view_->SelectedRowCount());
  EXPECT_EQ(1, table_view_->FirstSelectedRow());

  // Remove option 1.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("b"));
  EXPECT_EQ(2, table_view_->RowCount());
  // No option selected.
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(ChooserContentViewTest,
       AddAnOptionAndSelectItAndRemoveTheSelectedOption) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(2);

  mock_chooser_controller_->OptionAdded(base::ASCIIToUTF16("a"));

  // Select option 0.
  table_view_->Select(0);
  EXPECT_EQ(1, table_view_->SelectedRowCount());
  EXPECT_EQ(0, table_view_->FirstSelectedRow());

  // Remove option 0.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("a"));
  // There is no option shown now. But since "No devices found."
  // needs to be displayed on the |table_view_|, the number of rows is 1.
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT),
      table_model_->GetText(0, 0));
  // |table_view_| should be disabled since all options are removed.
  EXPECT_FALSE(table_view_->enabled());
  // No option selected.
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(ChooserContentViewTest, ClickStyledLabelLink) {
  EXPECT_CALL(*mock_styled_label_listener_,
              StyledLabelLinkClicked(styled_label_.get(), testing::_, 0))
      .Times(1);
  styled_label_->LinkClicked(nullptr, 0);
}
