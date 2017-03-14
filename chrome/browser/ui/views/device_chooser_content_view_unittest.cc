// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/device_chooser_content_view.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chooser_controller/mock_chooser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/list_selection_model.h"
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

class DeviceChooserContentViewTest : public views::ViewsTestBase {
 public:
  DeviceChooserContentViewTest() {}

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    auto mock_chooser_controller = base::MakeUnique<MockChooserController>();
    mock_chooser_controller_ = mock_chooser_controller.get();
    mock_table_view_observer_ = base::MakeUnique<MockTableViewObserver>();
    device_chooser_content_view_ = base::MakeUnique<DeviceChooserContentView>(
        mock_table_view_observer_.get(), std::move(mock_chooser_controller));
    table_view_ = device_chooser_content_view_->table_view_;
    ASSERT_TRUE(table_view_);
    table_model_ = table_view_->model();
    ASSERT_TRUE(table_model_);
    throbber_ = device_chooser_content_view_->throbber_;
    ASSERT_TRUE(throbber_);
    turn_adapter_off_help_ =
        device_chooser_content_view_->turn_adapter_off_help_;
    ASSERT_TRUE(turn_adapter_off_help_);
    footnote_link_ = device_chooser_content_view_->footnote_link();
    ASSERT_TRUE(footnote_link_);
  }

 protected:
  std::unique_ptr<MockTableViewObserver> mock_table_view_observer_;
  std::unique_ptr<DeviceChooserContentView> device_chooser_content_view_;
  MockChooserController* mock_chooser_controller_ = nullptr;
  views::TableView* table_view_ = nullptr;
  ui::TableModel* table_model_ = nullptr;
  views::Throbber* throbber_ = nullptr;
  views::StyledLabel* turn_adapter_off_help_ = nullptr;
  views::StyledLabel* footnote_link_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceChooserContentViewTest);
};

TEST_F(DeviceChooserContentViewTest, InitialState) {
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
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
  EXPECT_FALSE(throbber_->visible());
  EXPECT_FALSE(turn_adapter_off_help_->visible());
  EXPECT_EQ(device_chooser_content_view_->help_text_, footnote_link_->text());
}

TEST_F(DeviceChooserContentViewTest, AddOption) {
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
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("b"), MockChooserController::kSignalStrengthLevel0Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  EXPECT_EQ(2, table_view_->RowCount());
  EXPECT_EQ(base::ASCIIToUTF16("b"), table_model_->GetText(1, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("c"), MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::ConnectedPairedStatus::NONE);
  EXPECT_EQ(3, table_view_->RowCount());
  EXPECT_EQ(base::ASCIIToUTF16("c"), table_model_->GetText(2, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(DeviceChooserContentViewTest, RemoveOption) {
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
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  // Remove a non-existent option, the number of rows should not change.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("non-existent"));
  EXPECT_EQ(2, table_view_->RowCount());
  EXPECT_EQ(GetPairedText("a"), table_model_->GetText(0, 0));
  EXPECT_EQ(base::ASCIIToUTF16("c"), table_model_->GetText(1, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("c"));
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(GetPairedText("a"), table_model_->GetText(0, 0));
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(0UL, table_view_->selection_model().size());
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
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(DeviceChooserContentViewTest, UpdateOption) {
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
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(DeviceChooserContentViewTest, AddAndRemoveOption) {
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

TEST_F(DeviceChooserContentViewTest, UpdateAndRemoveTheUpdatedOption) {
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
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(DeviceChooserContentViewTest, SelectAndDeselectAnOption) {
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
  EXPECT_EQ(1UL, table_view_->selection_model().size());
  EXPECT_EQ(0, table_view_->FirstSelectedRow());

  // Unselect option 0.
  table_view_->Select(-1);
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());

  // Select option 1.
  table_view_->Select(1);
  EXPECT_EQ(1UL, table_view_->selection_model().size());
  EXPECT_EQ(1, table_view_->FirstSelectedRow());

  // Unselect option 1.
  table_view_->Select(-1);
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(DeviceChooserContentViewTest, SelectAnOptionAndThenSelectAnotherOption) {
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
  EXPECT_EQ(1UL, table_view_->selection_model().size());
  EXPECT_EQ(0, table_view_->FirstSelectedRow());

  // Select option 1.
  table_view_->Select(1);
  EXPECT_EQ(1UL, table_view_->selection_model().size());
  EXPECT_EQ(1, table_view_->FirstSelectedRow());

  // Select option 2.
  table_view_->Select(2);
  EXPECT_EQ(1UL, table_view_->selection_model().size());
  EXPECT_EQ(2, table_view_->FirstSelectedRow());
}

TEST_F(DeviceChooserContentViewTest, SelectAnOptionAndRemoveAnotherOption) {
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
  EXPECT_EQ(1UL, table_view_->selection_model().size());
  EXPECT_EQ(1, table_view_->FirstSelectedRow());

  // Remove option 0, the list becomes: b c.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("a"));
  EXPECT_EQ(2, table_view_->RowCount());
  EXPECT_EQ(1UL, table_view_->selection_model().size());
  // Since option 0 is removed, the original selected option 1 becomes
  // the first option in the list.
  EXPECT_EQ(0, table_view_->FirstSelectedRow());

  // Remove option 1.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("c"));
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(1UL, table_view_->selection_model().size());
  EXPECT_EQ(0, table_view_->FirstSelectedRow());
}

TEST_F(DeviceChooserContentViewTest, SelectAnOptionAndRemoveTheSelectedOption) {
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
  EXPECT_EQ(1UL, table_view_->selection_model().size());
  EXPECT_EQ(1, table_view_->FirstSelectedRow());

  // Remove option 1.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("b"));
  EXPECT_EQ(2, table_view_->RowCount());
  // No option selected.
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(DeviceChooserContentViewTest, SelectAnOptionAndUpdateTheSelectedOption) {
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

  EXPECT_EQ(1UL, table_view_->selection_model().size());
  EXPECT_EQ(1, table_view_->FirstSelectedRow());
  EXPECT_EQ(GetPairedText("a"), table_model_->GetText(0, 0));
  EXPECT_EQ(GetPairedText("d"), table_model_->GetText(1, 0));
  EXPECT_EQ(base::ASCIIToUTF16("c"), table_model_->GetText(2, 0));
}

TEST_F(DeviceChooserContentViewTest,
       AddAnOptionAndSelectItAndRemoveTheSelectedOption) {
  EXPECT_CALL(*mock_table_view_observer_, OnSelectionChanged()).Times(2);

  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);

  // Select option 0.
  table_view_->Select(0);
  EXPECT_EQ(1UL, table_view_->selection_model().size());
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
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
}

TEST_F(DeviceChooserContentViewTest, AdapterOnAndOffAndOn) {
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
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
  EXPECT_FALSE(throbber_->visible());
  EXPECT_FALSE(turn_adapter_off_help_->visible());
  EXPECT_EQ(device_chooser_content_view_->help_and_re_scan_text_,
            footnote_link_->text());

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
  EXPECT_EQ(device_chooser_content_view_->help_text_, footnote_link_->text());

  mock_chooser_controller_->OnAdapterPresenceChanged(
      content::BluetoothChooser::AdapterPresence::POWERED_ON);
  EXPECT_EQ(0u, mock_chooser_controller_->NumOptions());
  EXPECT_TRUE(table_view_->visible());
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT),
      table_model_->GetText(0, 0));
  EXPECT_FALSE(table_view_->enabled());
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
  EXPECT_FALSE(throbber_->visible());
  EXPECT_FALSE(turn_adapter_off_help_->visible());
  EXPECT_EQ(device_chooser_content_view_->help_and_re_scan_text_,
            footnote_link_->text());
}

TEST_F(DeviceChooserContentViewTest, DiscoveringAndNoOptionAddedAndIdle) {
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
  EXPECT_EQ(device_chooser_content_view_->help_and_scanning_text_,
            footnote_link_->text());

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
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
  EXPECT_FALSE(throbber_->visible());
  EXPECT_EQ(device_chooser_content_view_->help_and_re_scan_text_,
            footnote_link_->text());
}

TEST_F(DeviceChooserContentViewTest,
       DiscoveringAndOneOptionAddedAndSelectedAndIdle) {
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
  EXPECT_EQ(0UL, table_view_->selection_model().size());
  EXPECT_EQ(-1, table_view_->FirstSelectedRow());
  EXPECT_FALSE(throbber_->visible());
  EXPECT_EQ(device_chooser_content_view_->help_and_scanning_text_,
            footnote_link_->text());
  table_view_->Select(0);
  EXPECT_EQ(1UL, table_view_->selection_model().size());
  EXPECT_EQ(0, table_view_->FirstSelectedRow());

  mock_chooser_controller_->OnDiscoveryStateChanged(
      content::BluetoothChooser::DiscoveryState::IDLE);
  EXPECT_TRUE(table_view_->visible());
  EXPECT_EQ(1, table_view_->RowCount());
  EXPECT_EQ(base::ASCIIToUTF16("d"), table_model_->GetText(0, 0));
  // |table_view_| should be enabled since there is an option.
  EXPECT_TRUE(table_view_->enabled());
  EXPECT_EQ(1UL, table_view_->selection_model().size());
  EXPECT_EQ(0, table_view_->FirstSelectedRow());
  EXPECT_FALSE(throbber_->visible());
  EXPECT_EQ(device_chooser_content_view_->help_and_re_scan_text_,
            footnote_link_->text());
}

TEST_F(DeviceChooserContentViewTest, ClickAdapterOffHelpLink) {
  EXPECT_CALL(*mock_chooser_controller_, OpenAdapterOffHelpUrl()).Times(1);
  turn_adapter_off_help_->LinkClicked(nullptr, 0);
}

TEST_F(DeviceChooserContentViewTest, ClickRescanLink) {
  EXPECT_CALL(*mock_chooser_controller_, RefreshOptions()).Times(1);
  device_chooser_content_view_->StyledLabelLinkClicked(
      footnote_link_, device_chooser_content_view_->re_scan_text_range_, 0);
}

TEST_F(DeviceChooserContentViewTest, ClickGetHelpLink) {
  EXPECT_CALL(*mock_chooser_controller_, OpenHelpCenterUrl()).Times(1);
  device_chooser_content_view_->StyledLabelLinkClicked(
      footnote_link_, device_chooser_content_view_->help_text_range_, 0);
}
