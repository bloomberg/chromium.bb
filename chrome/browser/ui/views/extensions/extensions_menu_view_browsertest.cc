// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_paths.h"

class ExtensionsMenuViewBrowserTest : public DialogBrowserTest {
 protected:
  void LoadTestExtension() {
    extensions::ChromeTestExtensionLoader loader(browser()->profile());
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    extension_ = loader.LoadExtension(
        test_data_dir.AppendASCII("extensions/uitest/long_name"));
  }

 private:
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

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<const extensions::Extension> extension_;
};

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, InvokeUi_default) {
  LoadTestExtension();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewBrowserTest, InvokeUi_NoExtensions) {
  ShowAndVerifyUi();
}
