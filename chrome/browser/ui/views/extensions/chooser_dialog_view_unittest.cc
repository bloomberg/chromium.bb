// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/chooser_dialog_view.h"

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chooser_controller/fake_bluetooth_chooser_controller.h"
#include "chrome/browser/ui/views/device_chooser_content_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/widget/widget.h"

class ChooserDialogViewTest : public ChromeViewsTestBase {
 public:
  ChooserDialogViewTest() {}

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    auto controller = std::make_unique<FakeBluetoothChooserController>();
    controller_ = controller.get();
    dialog_ = new ChooserDialogView(std::move(controller));
    controller_->SetBluetoothStatus(
        FakeBluetoothChooserController::BluetoothStatus::IDLE);

    widget_ = views::DialogDelegate::CreateDialogWidget(dialog_, GetContext(),
                                                        nullptr);
  }

  void TearDown() override {
    widget_->Close();
    ChromeViewsTestBase::TearDown();
  }

  void AddDevice() {
    controller_->AddDevice(
        {"Device", FakeBluetoothChooserController::NOT_CONNECTED,
         FakeBluetoothChooserController::NOT_PAIRED,
         FakeBluetoothChooserController::kSignalStrengthLevel1});
  }

  void SelectDevice(size_t index) {
    dialog_->device_chooser_content_view_for_test()->table_view_->Select(index);
  }

 protected:
  ChooserDialogView* dialog_ = nullptr;
  FakeBluetoothChooserController* controller_ = nullptr;

 private:
  views::Widget* widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChooserDialogViewTest);
};

TEST_F(ChooserDialogViewTest, ButtonState) {
  // Cancel button is always enabled.
  EXPECT_TRUE(dialog_->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  // Selecting a device enables the OK button.
  EXPECT_FALSE(dialog_->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  AddDevice();
  EXPECT_FALSE(dialog_->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  SelectDevice(0);
  EXPECT_TRUE(dialog_->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // Changing state disables the OK button.
  controller_->SetBluetoothStatus(
      FakeBluetoothChooserController::BluetoothStatus::UNAVAILABLE);
  EXPECT_FALSE(dialog_->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  controller_->SetBluetoothStatus(
      FakeBluetoothChooserController::BluetoothStatus::SCANNING);
  EXPECT_FALSE(dialog_->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  SelectDevice(0);
  EXPECT_TRUE(dialog_->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  controller_->SetBluetoothStatus(
      FakeBluetoothChooserController::BluetoothStatus::IDLE);
  EXPECT_FALSE(dialog_->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
}

TEST_F(ChooserDialogViewTest, Accept) {
  AddDevice();
  AddDevice();
  SelectDevice(1);
  std::vector<size_t> expected = {1u};
  EXPECT_CALL(*controller_, Select(testing::Eq(expected))).Times(1);
  dialog_->Accept();
}

TEST_F(ChooserDialogViewTest, Cancel) {
  EXPECT_CALL(*controller_, Cancel()).Times(1);
  dialog_->Cancel();
}

TEST_F(ChooserDialogViewTest, Close) {
  // Called from Widget::Close() in TearDown().
  EXPECT_CALL(*controller_, Close()).Times(1);
}
