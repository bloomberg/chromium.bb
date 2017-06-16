// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/chooser_dialog_view.h"

#include <string>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chooser_controller/mock_chooser_controller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"

// Invokes the device chooser dialog with mock content. See
// test_browser_dialog.h.
class ChooserDialogViewBrowserTest : public DialogBrowserTest {
 public:
  ChooserDialogViewBrowserTest() : controller_(new MockChooserController()) {}

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    web_modal::WebContentsModalDialogManager* manager =
        web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
    if (manager) {
      constrained_window::ShowWebModalDialogViews(
          new ChooserDialogView(std::move(controller_)), web_contents);
    }
  }

  MockChooserController& controller() { return *controller_; }

 private:
  std::unique_ptr<MockChooserController> controller_;

  DISALLOW_COPY_AND_ASSIGN(ChooserDialogViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ChooserDialogViewBrowserTest, InvokeDialog_noDevices) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ChooserDialogViewBrowserTest, InvokeDialog_withDevices) {
  controller().OptionAdded(base::ASCIIToUTF16("Device 1"),
                           MockChooserController::kNoSignalStrengthLevelImage,
                           MockChooserController::NONE);
  controller().OptionAdded(base::ASCIIToUTF16("Device 2"),
                           MockChooserController::kSignalStrengthLevel2Bar,
                           MockChooserController::PAIRED);
  controller().OptionAdded(base::ASCIIToUTF16("Device 3"),
                           MockChooserController::kSignalStrengthLevel4Bar,
                           MockChooserController::CONNECTED);
  controller().OptionAdded(
      base::ASCIIToUTF16("Device 4"),
      MockChooserController::kSignalStrengthLevel1Bar,
      MockChooserController::PAIRED | MockChooserController::CONNECTED);
  RunDialog();
}
