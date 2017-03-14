// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/chooser_dialog_view.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chooser_controller/mock_chooser_controller.h"
#include "chrome/browser/ui/views/device_chooser_content_view.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

class ChooserDialogViewTest : public views::ViewsTestBase {
 public:
  ChooserDialogViewTest() {}
  ~ChooserDialogViewTest() override {}

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    auto mock_chooser_controller = base::MakeUnique<MockChooserController>();
    mock_chooser_controller_ = mock_chooser_controller.get();
    std::unique_ptr<ChooserDialogView> chooser_dialog_view(
        new ChooserDialogView(std::move(mock_chooser_controller)));
    chooser_dialog_view_ = chooser_dialog_view.get();
    table_view_ = chooser_dialog_view_->device_chooser_content_view_for_test()
                      ->table_view_;
    ASSERT_TRUE(table_view_);

    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 600, 600);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    parent_widget_ = base::MakeUnique<views::Widget>();
    params.native_widget = views::test::CreatePlatformDesktopNativeWidgetImpl(
        params, parent_widget_.get(), nullptr);
    parent_widget_->Init(params);

    dialog_ = views::DialogDelegate::CreateDialogWidget(
        chooser_dialog_view.release(), GetContext(),
        parent_widget_->GetNativeView());
    ASSERT_TRUE(dialog_);
    ok_button_ = chooser_dialog_view_->GetDialogClientView()->ok_button();
    ASSERT_TRUE(ok_button_);
    cancel_button_ =
        chooser_dialog_view_->GetDialogClientView()->cancel_button();
    ASSERT_TRUE(cancel_button_);
  }

  // views::ViewsTestBase:
  void TearDown() override {
    dialog_->CloseNow();
    parent_widget_->CloseNow();
    views::ViewsTestBase::TearDown();
  }

 protected:
  MockChooserController* mock_chooser_controller_ = nullptr;
  ChooserDialogView* chooser_dialog_view_ = nullptr;
  std::unique_ptr<views::Widget> parent_widget_;
  views::TableView* table_view_ = nullptr;
  views::LabelButton* ok_button_ = nullptr;
  views::LabelButton* cancel_button_ = nullptr;
  views::Widget* dialog_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChooserDialogViewTest);
};

TEST_F(ChooserDialogViewTest, InitialState) {
  // OK button is disabled since there is no option.
  EXPECT_FALSE(ok_button_->enabled());
  // Cancel button is always enabled.
  EXPECT_TRUE(cancel_button_->enabled());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_CONNECT_BUTTON_TEXT),
      ok_button_->GetText());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_CANCEL_BUTTON_TEXT),
            cancel_button_->GetText());
}

TEST_F(ChooserDialogViewTest, SelectAndDeselectAnOption) {
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
  // OK button is disabled since no option is selected.
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Select option 0.
  table_view_->Select(0);
  // OK button is enabled since an option is selected.
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Unselect option 0.
  table_view_->Select(-1);
  // OK button is disabled since no option is selected.
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Select option 1.
  table_view_->Select(1);
  // OK button is enabled since an option is selected.
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Unselect option 1.
  table_view_->Select(-1);
  // OK button is disabled since no option is selected.
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());
}

TEST_F(ChooserDialogViewTest, SelectAnOptionAndThenSelectAnotherOption) {
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
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Select option 0.
  table_view_->Select(0);
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Select option 1.
  table_view_->Select(1);
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Select option 2.
  table_view_->Select(2);
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());
}

TEST_F(ChooserDialogViewTest, SelectAnOptionAndRemoveAnotherOption) {
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
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Select option 1.
  table_view_->Select(1);
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Remove option 0, the list becomes: b c.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("a"));
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Remove option 1.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("c"));
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());
}

TEST_F(ChooserDialogViewTest, SelectAnOptionAndRemoveTheSelectedOption) {
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
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Select option 1.
  table_view_->Select(1);
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Remove option 1.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("b"));
  // OK button is disabled since the selected option is removed.
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());
}

TEST_F(ChooserDialogViewTest, SelectAnOptionAndUpdateTheSelectedOption) {
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
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Select option 1.
  table_view_->Select(1);
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Update option 1.
  mock_chooser_controller_->OptionUpdated(
      base::ASCIIToUTF16("b"), base::ASCIIToUTF16("d"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Remove option 1.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("d"));
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());
}

TEST_F(ChooserDialogViewTest,
       AddAnOptionAndSelectItAndRemoveTheSelectedOption) {
  mock_chooser_controller_->OptionAdded(
      base::ASCIIToUTF16("a"),
      MockChooserController::kNoSignalStrengthLevelImage,
      MockChooserController::ConnectedPairedStatus::CONNECTED |
          MockChooserController::ConnectedPairedStatus::PAIRED);
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Select option 0.
  table_view_->Select(0);
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  // Remove option 0.
  mock_chooser_controller_->OptionRemoved(base::ASCIIToUTF16("a"));
  // There is no option shown now and the OK button is disabled.
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());
}

TEST_F(ChooserDialogViewTest, Accept) {
  EXPECT_CALL(*mock_chooser_controller_, Select(testing::_)).Times(1);
  chooser_dialog_view_->GetDialogClientView()->AcceptWindow();
}

TEST_F(ChooserDialogViewTest, Cancel) {
  EXPECT_CALL(*mock_chooser_controller_, Cancel()).Times(1);
  chooser_dialog_view_->GetDialogClientView()->CancelWindow();
}

TEST_F(ChooserDialogViewTest, Close) {
  EXPECT_CALL(*mock_chooser_controller_, Close()).Times(1);
  chooser_dialog_view_->GetDialogClientView()->GetWidget()->Close();
}

TEST_F(ChooserDialogViewTest, AdapterOnAndOffAndOn) {
  mock_chooser_controller_->OnAdapterPresenceChanged(
      content::BluetoothChooser::AdapterPresence::POWERED_ON);
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

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
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  table_view_->Select(1);
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  mock_chooser_controller_->OnAdapterPresenceChanged(
      content::BluetoothChooser::AdapterPresence::POWERED_OFF);
  // Since the adapter is turned off, the previously selected option
  // becomes invalid, the OK button is disabled.
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  mock_chooser_controller_->OnAdapterPresenceChanged(
      content::BluetoothChooser::AdapterPresence::POWERED_ON);
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());
}

TEST_F(ChooserDialogViewTest, DiscoveringAndNoOptionAddedAndIdle) {
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
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  mock_chooser_controller_->OnDiscoveryStateChanged(
      content::BluetoothChooser::DiscoveryState::DISCOVERING);
  // OK button is disabled since the chooser is refreshing options.
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  mock_chooser_controller_->OnDiscoveryStateChanged(
      content::BluetoothChooser::DiscoveryState::IDLE);
  // OK button is disabled since the chooser refreshed options.
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());
}

TEST_F(ChooserDialogViewTest, DiscoveringAndOneOptionAddedAndSelectedAndIdle) {
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
  // OK button is disabled since no option is selected.
  EXPECT_FALSE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());
  table_view_->Select(0);
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());

  mock_chooser_controller_->OnDiscoveryStateChanged(
      content::BluetoothChooser::DiscoveryState::IDLE);
  EXPECT_TRUE(ok_button_->enabled());
  EXPECT_TRUE(cancel_button_->enabled());
}
