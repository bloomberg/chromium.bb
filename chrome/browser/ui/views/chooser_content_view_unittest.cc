// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chooser_content_view.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chooser_controller/mock_chooser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/test/views_test_base.h"

namespace {

class MockTableViewObserver : public views::TableViewObserver {
 public:
  // views::TableViewObserver:
  MOCK_METHOD0(OnSelectionChanged, void());
};

base::string16 GetPairedText(const std::string& device_name) {
  return l10n_util::GetStringFUTF16(
      IDS_DEVICE_CHOOSER_DEVICE_NAME_AND_PAIRED_STATUS_TEXT,
      base::ASCIIToUTF16(device_name));
}

}  // namespace

class ChooserContentViewTest : public views::ViewsTestBase {
 public:
  ChooserContentViewTest() {}

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
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
    throbber_ = chooser_content_view_->throbber_for_test();
    ASSERT_TRUE(throbber_);
    turn_adapter_off_help_ =
        chooser_content_view_->turn_adapter_off_help_for_test();
    ASSERT_TRUE(turn_adapter_off_help_);
    discovery_state_.reset(chooser_content_view_->CreateExtraView());
    ASSERT_TRUE(discovery_state_);
    help_link_.reset(chooser_content_view_->CreateFootnoteView());
    ASSERT_TRUE(help_link_);
  }

 protected:
  // |discovery_state_| needs to be valid when |chooser_content_view_| is
  // released, since ChooserContentView's destructor needs to access it.
  // So it is declared before |chooser_content_view_|.
  std::unique_ptr<views::Link> discovery_state_;
  std::unique_ptr<MockTableViewObserver> mock_table_view_observer_;
  std::unique_ptr<ChooserContentView> chooser_content_view_;
  MockChooserController* mock_chooser_controller_;
  views::TableView* table_view_;
  ui::TableModel* table_model_;
  views::Throbber* throbber_;
  views::StyledLabel* turn_adapter_off_help_;
  std::unique_ptr<views::StyledLabel> help_link_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChooserContentViewTest);
};

TEST_F(ChooserContentViewTest, InitialState) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(0);

  EXPECT_TRUE(table_view_->visible());
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
  EXPECT_FALSE(throbber_->visible());
  EXPECT_FALSE(turn_adapter_off_help_->visible());
  EXPECT_TRUE(discovery_state_->text().empty());
}

TEST_F(ChooserContentViewTest, AddOption) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(0);

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(GetPairedText("a"), table_model_->GetText(0, 0));
  // |table_view_| should be enabled since there is an option.
  EXPECT_TRUE(table_view_->enabled());
  // No option selected.
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  EXPECT_EQ(2, table_view_->RowCount());
  EXPECT_EQ(base::ASCIIToUTF16("b"), table_model_->GetText(1, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  EXPECT_EQ(3, table_view_->RowCount());
  EXPECT_EQ(base::ASCIIToUTF16("c"), table_model_->GetText(2, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(ChooserContentViewTest, RemoveOption) {
  // Called from TableView::OnItemsRemoved().
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(3);

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);

  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("b"));
  EXPECT_EQ(2, table_view_->RowCount());
  EXPECT_EQ(GetPairedText("a"), table_model_->GetText(0, 0));
  EXPECT_EQ(base::ASCIIToUTF16("c"), table_model_->GetText(1, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  // Remove a non-existent option, the number of rows should not change.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("non-existent"));
  EXPECT_EQ(2, table_view_->RowCount());
  EXPECT_EQ(GetPairedText("a"), table_model_->GetText(0, 0));
  EXPECT_EQ(base::ASCIIToUTF16("c"), table_model_->GetText(1, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("c"));
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(GetPairedText("a"), table_model_->GetText(0, 0));
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

TEST_F(ChooserContentViewTest, UpdateOption) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(0);

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);

  mock_chooser_controller_->OptionUpdated(
      base::ASCIIToUTF16("b"), base::ASCIIToUTF16("d"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  EXPECT_EQ(3, table_view_->RowCount());
  EXPECT_EQ(GetPairedText("a"), table_model_->GetText(0, 0));
  EXPECT_EQ(GetPairedText("d"), table_model_->GetText(1, 0));
  EXPECT_EQ(base::ASCIIToUTF16("c"), table_model_->GetText(2, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(ChooserContentViewTest, AddAndRemoveOption) {
  // Called from TableView::OnItemsRemoved().
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(3);

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  EXPECT_EQ(1, table_view_->RowCount());
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  EXPECT_EQ(2, table_view_->RowCount());
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("b"));
  EXPECT_EQ(1, table_view_->RowCount());
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  EXPECT_EQ(2, table_view_->RowCount());
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("d"), MockChooserController::kSignalStrengthLevel2Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  EXPECT_EQ(3, table_view_->RowCount());
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("d"));
  EXPECT_EQ(2, table_view_->RowCount());
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("c"));
  EXPECT_EQ(1, table_view_->RowCount());
}

TEST_F(ChooserContentViewTest, UpdateAndRemoveTheUpdatedOption) {
  // Called from TableView::OnItemsRemoved().
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(1);

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);

  mock_chooser_controller_->OptionUpdated(
      base::ASCIIToUTF16("b"), base::ASCIIToUTF16("d"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);

  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("d"));

  EXPECT_EQ(2, table_view_->RowCount());
  EXPECT_EQ(GetPairedText("a"), table_model_->GetText(0, 0));
  EXPECT_EQ(base::ASCIIToUTF16("c"), table_model_->GetText(1, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(ChooserContentViewTest, SelectAndDeselectAnOption) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(4);

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);

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

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);

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

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);

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

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);

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

TEST_F(ChooserContentViewTest, SelectAnOptionAndUpdateTheSelectedOption) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(1);

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);

  // Select option 1.
  table_view_->Select(1);

  // Update option 1.
  mock_chooser_controller_->OptionUpdated(
      base::ASCIIToUTF16("b"), base::ASCIIToUTF16("d"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);

  EXPECT_EQ(1, table_view_->SelectedRowCount());
  EXPECT_EQ(1, table_view_->FirstSelectedRow());
  EXPECT_EQ(GetPairedText("a"), table_model_->GetText(0, 0));
  EXPECT_EQ(GetPairedText("d"), table_model_->GetText(1, 0));
  EXPECT_EQ(base::ASCIIToUTF16("c"), table_model_->GetText(2, 0));
}

TEST_F(ChooserContentViewTest,
       AddAnOptionAndSelectItAndRemoveTheSelectedOption) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(2);

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);

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

TEST_F(ChooserContentViewTest, AdapterOnAndOffAndOn) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(2);

  mock_chooser_controller_->OnAdapterPresenceChanged(
      content::BluetoothChooser::AdapterPresence::POWERED_ON);
  EXPECT_TRUE(table_view_->visible());
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
  EXPECT_FALSE(throbber_->visible());
  EXPECT_FALSE(turn_adapter_off_help_->visible());
  EXPECT_TRUE(discovery_state_->enabled());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN),
            discovery_state_->text());

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);

  table_view_->Select(1);

  mock_chooser_controller_->OnAdapterPresenceChanged(
      content::BluetoothChooser::AdapterPresence::POWERED_OFF);
  EXPECT_EQ(0u, mock_chooser_controller_->NumOptions());
  EXPECT_FALSE(table_view_->visible());
  EXPECT_FALSE(throbber_->visible());
  EXPECT_TRUE(turn_adapter_off_help_->visible());
  EXPECT_FALSE(discovery_state_->enabled());
  EXPECT_TRUE(discovery_state_->text().empty());

  mock_chooser_controller_->OnAdapterPresenceChanged(
      content::BluetoothChooser::AdapterPresence::POWERED_ON);
  EXPECT_EQ(0u, mock_chooser_controller_->NumOptions());
  EXPECT_TRUE(table_view_->visible());
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT),
      table_model_->GetText(0, 0));
  EXPECT_FALSE(table_view_->enabled());
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
  EXPECT_FALSE(throbber_->visible());
  EXPECT_FALSE(turn_adapter_off_help_->visible());
  EXPECT_TRUE(discovery_state_->enabled());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN),
            discovery_state_->text());
}

TEST_F(ChooserContentViewTest, DiscoveringAndNoOptionAddedAndIdle) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(2);

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  table_view_->Select(1);

  mock_chooser_controller_->OnDiscoveryStateChanged(
      content::BluetoothChooser::DiscoveryState::DISCOVERING);
  EXPECT_FALSE(table_view_->visible());
  EXPECT_TRUE(throbber_->visible());
  // |discovery_state_| is disabled and shows a label instead of a link.
  EXPECT_FALSE(discovery_state_->enabled());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING),
            discovery_state_->text());

  mock_chooser_controller_->OnDiscoveryStateChanged(
      content::BluetoothChooser::DiscoveryState::IDLE);
  EXPECT_TRUE(table_view_->visible());
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
  EXPECT_FALSE(throbber_->visible());
  // |discovery_state_| is enabled and shows a link.
  EXPECT_TRUE(discovery_state_->enabled());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN),
            discovery_state_->text());
}

TEST_F(ChooserContentViewTest, DiscoveringAndOneOptionAddedAndSelectedAndIdle) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(3);

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  table_view_->Select(1);

  mock_chooser_controller_->OnDiscoveryStateChanged(
      content::BluetoothChooser::DiscoveryState::DISCOVERING);
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("d"), MockChooserController::kSignalStrengthLevel2Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  EXPECT_TRUE(table_view_->visible());
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(base::ASCIIToUTF16("d"), table_model_->GetText(0, 0));
  // |table_view_| should be enabled since there is an option.
  EXPECT_TRUE(table_view_->enabled());
  // No option selected.
  EXPECT_EQ(0, table_view_->SelectedRowCount());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
  EXPECT_FALSE(throbber_->visible());
  // |discovery_state_| is disabled and shows a label instead of a link.
  EXPECT_FALSE(discovery_state_->enabled());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING),
            discovery_state_->text());
  table_view_->Select(0);
  EXPECT_EQ(1, table_view_->SelectedRowCount());
  EXPECT_EQ(0, table_view_->FirstSelectedRow());

  mock_chooser_controller_->OnDiscoveryStateChanged(
      content::BluetoothChooser::DiscoveryState::IDLE);
  EXPECT_TRUE(table_view_->visible());
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(base::ASCIIToUTF16("d"), table_model_->GetText(0, 0));
  // |table_view_| should be enabled since there is an option.
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(1, table_view_->SelectedRowCount());
  EXPECT_EQ(0, table_view_->FirstSelectedRow());
  EXPECT_FALSE(throbber_->visible());
  // |discovery_state_| is enabled and shows a link.
  EXPECT_TRUE(discovery_state_->enabled());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN),
            discovery_state_->text());
}

TEST_F(ChooserContentViewTest, ClickAdapterOffHelpLink) {
  EXPECT_CALL(*mock_chooser_controller_, OpenAdapterOffHelpUrl()).Times(1);
  turn_adapter_off_help_->LinkClicked(nullptr, 0);
}

TEST_F(ChooserContentViewTest, ClickRescanLink) {
  EXPECT_CALL(*mock_chooser_controller_, RefreshOptions()).Times(1);
  chooser_content_view_->LinkClicked(discovery_state_.get(), 0);
}

TEST_F(ChooserContentViewTest, ClickStyledLabelLink) {
  EXPECT_CALL(*mock_chooser_controller_, OpenHelpCenterUrl()).Times(1);
  help_link_->LinkClicked(nullptr, 0);
}
