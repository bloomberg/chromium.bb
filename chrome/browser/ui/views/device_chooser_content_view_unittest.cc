// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/device_chooser_content_view.h"

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chooser_controller/fake_bluetooth_chooser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/controls/throbber.h"

namespace {

class MockTableViewObserver : public views::TableViewObserver {
 public:
  // views::TableViewObserver:
  MOCK_METHOD0(OnSelectionChanged, void());
};

}  // namespace

class DeviceChooserContentViewTest : public ChromeViewsTestBase {
 public:
  DeviceChooserContentViewTest() {}

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    table_observer_ = std::make_unique<MockTableViewObserver>();
    auto controller = std::make_unique<FakeBluetoothChooserController>();
    controller_ = controller.get();
    content_view_ = std::make_unique<DeviceChooserContentView>(
        table_observer_.get(), std::move(controller));

    ASSERT_NE(nullptr, table_view());
    ASSERT_NE(nullptr, throbber());
    ASSERT_NE(nullptr, adapter_off_help_link());
    ASSERT_NE(nullptr, footnote_link());

    controller_->SetBluetoothStatus(
        FakeBluetoothChooserController::BluetoothStatus::IDLE);
  }

  FakeBluetoothChooserController* controller() { return controller_; }
  MockTableViewObserver& table_observer() { return *table_observer_; }
  DeviceChooserContentView& content_view() { return *content_view_; }

  views::TableView* table_view() { return content_view().table_view_; }
  ui::TableModel* table_model() { return table_view()->model(); }
  views::Throbber* throbber() { return content_view().throbber_; }
  views::StyledLabel* adapter_off_help_link() {
    return content_view().turn_adapter_off_help_;
  }
  views::StyledLabel* footnote_link() {
    return content_view().footnote_link_.get();
  }

  void ExpectFootnoteLinkHasScanningText() {
    EXPECT_EQ(content_view().help_and_scanning_text_, footnote_link()->text());
  }

  void ExpectFootnoteLinkHasRescanText() {
    EXPECT_EQ(content_view().help_and_re_scan_text_, footnote_link()->text());
  }

  void ExpectFootnoteLinkOnlyHasHelpText() {
    EXPECT_EQ(content_view().help_text_, footnote_link()->text());
  }

  void AddUnpairedDevice() {
    controller()->AddDevice(
        {"Unpaired Device", FakeBluetoothChooserController::NOT_CONNECTED,
         FakeBluetoothChooserController::NOT_PAIRED,
         FakeBluetoothChooserController::kSignalStrengthLevel1});
  }

  void AddPairedDevice() {
    controller()->AddDevice(
        {"Paired Device", FakeBluetoothChooserController::CONNECTED,
         FakeBluetoothChooserController::PAIRED,
         FakeBluetoothChooserController::kSignalStrengthUnknown});
  }

  base::string16 GetUnpairedDeviceTextAtRow(size_t row_index) {
    return controller()->GetOption(row_index);
  }

  base::string16 GetPairedDeviceTextAtRow(size_t row_index) {
    return l10n_util::GetStringFUTF16(
        IDS_DEVICE_CHOOSER_DEVICE_NAME_AND_PAIRED_STATUS_TEXT,
        GetUnpairedDeviceTextAtRow(row_index));
  }

  bool IsDeviceSelected() { return table_view()->selection_model().size() > 0; }

  void ExpectNoDevices() {
    // "No devices found." is displayed in the table, so there's exactly 1 row.
    EXPECT_EQ(1, table_view()->RowCount());
    EXPECT_EQ(l10n_util::GetStringUTF16(
                  IDS_BLUETOOTH_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT),
              table_model()->GetText(0, 0));
    // The table should be disabled since there are no (real) options.
    EXPECT_FALSE(table_view()->enabled());
    EXPECT_FALSE(IsDeviceSelected());
  }

 private:
  std::unique_ptr<MockTableViewObserver> table_observer_;
  FakeBluetoothChooserController* controller_ = nullptr;
  std::unique_ptr<DeviceChooserContentView> content_view_;

  DISALLOW_COPY_AND_ASSIGN(DeviceChooserContentViewTest);
};

TEST_F(DeviceChooserContentViewTest, InitialState) {
  EXPECT_CALL(table_observer(), OnSelectionChanged()).Times(0);

  EXPECT_TRUE(table_view()->visible());
  ExpectNoDevices();
  EXPECT_FALSE(throbber()->visible());
  EXPECT_FALSE(adapter_off_help_link()->visible());
  ExpectFootnoteLinkHasRescanText();
}

TEST_F(DeviceChooserContentViewTest, AddOption) {
  EXPECT_CALL(table_observer(), OnSelectionChanged()).Times(0);
  AddPairedDevice();

  EXPECT_EQ(1, table_view()->RowCount());
  EXPECT_EQ(GetPairedDeviceTextAtRow(0), table_model()->GetText(0, 0));
  // The table should be enabled now that there's an option.
  EXPECT_TRUE(table_view()->enabled());
  EXPECT_FALSE(IsDeviceSelected());

  AddUnpairedDevice();
  EXPECT_EQ(2, table_view()->RowCount());
  EXPECT_EQ(GetUnpairedDeviceTextAtRow(1), table_model()->GetText(1, 0));
  EXPECT_TRUE(table_view()->enabled());
  EXPECT_FALSE(IsDeviceSelected());
}

TEST_F(DeviceChooserContentViewTest, RemoveOption) {
  // Called from TableView::OnItemsRemoved().
  EXPECT_CALL(table_observer(), OnSelectionChanged()).Times(3);
  AddPairedDevice();
  AddUnpairedDevice();
  AddUnpairedDevice();

  // Remove the paired device.
  controller()->RemoveDevice(0);
  EXPECT_EQ(2, table_view()->RowCount());
  EXPECT_EQ(GetUnpairedDeviceTextAtRow(0), table_model()->GetText(0, 0));
  EXPECT_EQ(GetUnpairedDeviceTextAtRow(1), table_model()->GetText(1, 0));
  EXPECT_TRUE(table_view()->enabled());
  EXPECT_FALSE(IsDeviceSelected());

  // Remove everything.
  controller()->RemoveDevice(1);
  controller()->RemoveDevice(0);
  // Should be back to the initial state.
  ExpectNoDevices();
}

TEST_F(DeviceChooserContentViewTest, UpdateOption) {
  EXPECT_CALL(table_observer(), OnSelectionChanged()).Times(0);
  AddPairedDevice();
  AddUnpairedDevice();
  AddUnpairedDevice();

  EXPECT_EQ(GetUnpairedDeviceTextAtRow(1), table_model()->GetText(1, 0));
  controller()->UpdateDevice(
      1, {"Nice Device", FakeBluetoothChooserController::CONNECTED,
          FakeBluetoothChooserController::PAIRED,
          FakeBluetoothChooserController::kSignalStrengthUnknown});
  EXPECT_EQ(3, table_view()->RowCount());
  EXPECT_EQ(GetPairedDeviceTextAtRow(1), table_model()->GetText(1, 0));
  EXPECT_FALSE(IsDeviceSelected());
}

TEST_F(DeviceChooserContentViewTest, SelectAndDeselectAnOption) {
  EXPECT_CALL(table_observer(), OnSelectionChanged()).Times(2);
  AddPairedDevice();
  AddUnpairedDevice();

  table_view()->Select(0);
  EXPECT_TRUE(IsDeviceSelected());
  EXPECT_EQ(0, table_view()->FirstSelectedRow());

  table_view()->Select(-1);
  EXPECT_FALSE(IsDeviceSelected());
  EXPECT_EQ(-1, table_view()->FirstSelectedRow());
}

TEST_F(DeviceChooserContentViewTest, TurnBluetoothOffAndOn) {
  AddUnpairedDevice();
  controller()->SetBluetoothStatus(
      FakeBluetoothChooserController::BluetoothStatus::UNAVAILABLE);

  EXPECT_FALSE(table_view()->visible());
  EXPECT_TRUE(adapter_off_help_link()->visible());
  EXPECT_FALSE(throbber()->visible());
  ExpectFootnoteLinkOnlyHasHelpText();

  controller()->RemoveDevice(0);
  controller()->SetBluetoothStatus(
      FakeBluetoothChooserController::BluetoothStatus::IDLE);
  ExpectNoDevices();
  EXPECT_FALSE(adapter_off_help_link()->visible());
  EXPECT_FALSE(throbber()->visible());
}

TEST_F(DeviceChooserContentViewTest, ScanForDevices) {
  controller()->SetBluetoothStatus(
      FakeBluetoothChooserController::BluetoothStatus::SCANNING);
  EXPECT_FALSE(table_view()->visible());
  EXPECT_FALSE(adapter_off_help_link()->visible());
  EXPECT_TRUE(throbber()->visible());
  ExpectFootnoteLinkHasScanningText();

  AddUnpairedDevice();
  EXPECT_TRUE(table_view()->visible());
  EXPECT_TRUE(table_view()->enabled());
  EXPECT_FALSE(adapter_off_help_link()->visible());
  EXPECT_FALSE(throbber()->visible());
  EXPECT_EQ(1, table_view()->RowCount());
  EXPECT_FALSE(IsDeviceSelected());
  ExpectFootnoteLinkHasScanningText();
}

TEST_F(DeviceChooserContentViewTest, ClickAdapterOffHelpLink) {
  EXPECT_CALL(*controller(), OpenAdapterOffHelpUrl()).Times(1);
  adapter_off_help_link()->LinkClicked(nullptr, 0);
}

TEST_F(DeviceChooserContentViewTest, ClickRescanLink) {
  EXPECT_CALL(*controller(), RefreshOptions()).Times(1);
  content_view().StyledLabelLinkClicked(footnote_link(),
                                        content_view().re_scan_text_range_, 0);
}

TEST_F(DeviceChooserContentViewTest, ClickGetHelpLink) {
  EXPECT_CALL(*controller(), OpenHelpCenterUrl()).Times(1);
  content_view().StyledLabelLinkClicked(footnote_link(),
                                        content_view().help_text_range_, 0);
}
