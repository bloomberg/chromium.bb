// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_paths.h"

class ExtensionsMenuViewBrowserTest : public DialogBrowserTest {
 protected:
  void LoadTestExtension(const std::string& extension) {
    extensions::ChromeTestExtensionLoader loader(browser()->profile());
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    extensions_.push_back(
        loader.LoadExtension(test_data_dir.AppendASCII(extension)));
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kExtensionsToolbarMenu);
    DialogBrowserTest::SetUp();
  }

  void ShowUi(const std::string& name) override {
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->extensions_button()
        ->OnMousePressed(click_event);
  }

  static std::vector<ExtensionsMenuButton*> GetExtensionMenuButtons() {
    std::vector<ExtensionsMenuButton*> buttons;
    for (auto* view : ExtensionsMenuView::GetExtensionsMenuViewForTesting()
                          ->GetChildrenInZOrder()) {
      if (view->GetClassName() == ExtensionsMenuButton::kClassName)
        buttons.push_back(static_cast<ExtensionsMenuButton*>(view));
    }
    return buttons;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<scoped_refptr<const extensions::Extension>> extensions_;
};

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, InvokeUi_default) {
  LoadTestExtension("extensions/uitest/long_name");
  LoadTestExtension("extensions/uitest/window_open");

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, InvokeUi_NoExtensions) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest,
                       CreatesOneButtonPerExtension) {
  LoadTestExtension("extensions/uitest/long_name");
  LoadTestExtension("extensions/uitest/window_open");
  ShowUi("");
  VerifyUi();
  EXPECT_EQ(2u, extensions_.size());
  EXPECT_EQ(extensions_.size(), GetExtensionMenuButtons().size());
  DismissUi();
}
