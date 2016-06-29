// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/chooser_dialog_view.h"

#include "base/macros.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/chooser_controller/mock_chooser_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

class ChooserDialogViewTest : public ExtensionBrowserTest {
 public:
  ChooserDialogViewTest() {}
  ~ChooserDialogViewTest() override {}

  void SetUpOnMainThread() override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    std::unique_ptr<MockChooserController> mock_chooser_controller(
        new MockChooserController(nullptr));
    mock_chooser_controller_ = mock_chooser_controller.get();
    std::unique_ptr<ChooserDialogView> chooser_dialog_view(
        new ChooserDialogView(web_contents,
                              std::move(mock_chooser_controller)));
    chooser_dialog_view_ = chooser_dialog_view.get();
    table_view_ = chooser_dialog_view_->table_view_for_test();
    ASSERT_TRUE(table_view_);
    table_model_ = table_view_->model();
    ASSERT_TRUE(table_model_);
    views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
        chooser_dialog_view.release(), nullptr,
        platform_util::GetViewForWindow(
            browser()->window()->GetNativeWindow()));
    modal_dialog->Show();
  }

 protected:
  MockChooserController* mock_chooser_controller_;
  ChooserDialogView* chooser_dialog_view_;
  views::TableView* table_view_;
  ui::TableModel* table_model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChooserDialogViewTest);
};

IN_PROC_BROWSER_TEST_F(ChooserDialogViewTest, InitialState) {
  // Since "No devices found." needs to be displayed on the |table_view_|,
  // the number of rows is 1.
  EXPECT_EQ(table_view_->RowCount(), 1);
  EXPECT_EQ(
      table_model_->GetText(0, 0),
      l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT));
  // |table_view_| should be disabled since there is no option shown.
  EXPECT_FALSE(table_view_->enabled());
  // No option selected.
  EXPECT_EQ(table_view_->SelectedRowCount(), 0);
  EXPECT_EQ(table_view_->FirstSelectedRow(), -1);
}

IN_PROC_BROWSER_TEST_F(ChooserDialogViewTest, Accept) {
  EXPECT_CALL(*mock_chooser_controller_, Select(testing::_)).Times(1);
  chooser_dialog_view_->Accept();
}

IN_PROC_BROWSER_TEST_F(ChooserDialogViewTest, Cancel) {
  EXPECT_CALL(*mock_chooser_controller_, Cancel()).Times(1);
  chooser_dialog_view_->Cancel();
}

IN_PROC_BROWSER_TEST_F(ChooserDialogViewTest, Close) {
  EXPECT_CALL(*mock_chooser_controller_, Close()).Times(1);
  chooser_dialog_view_->Close();
}

IN_PROC_BROWSER_TEST_F(ChooserDialogViewTest, ClickStyledLabelLink) {
  EXPECT_CALL(*mock_chooser_controller_, OpenHelpCenterUrl()).Times(1);
  chooser_dialog_view_->StyledLabelLinkClicked(nullptr, gfx::Range(), 0);
}
