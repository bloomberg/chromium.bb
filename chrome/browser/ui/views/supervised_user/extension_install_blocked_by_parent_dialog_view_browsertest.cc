// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

class ExtensionInstallBlockedByParentDialogViewTest : public DialogBrowserTest {
 public:
  ExtensionInstallBlockedByParentDialogViewTest() = default;
  ~ExtensionInstallBlockedByParentDialogViewTest() override = default;

  void ShowUi(const std::string& name) override {
    extensions::ExtensionBuilder::Type type =
        extensions::ExtensionBuilder::Type::EXTENSION;
    if (name == "extension") {
      // type EXTENSION is correct.
    } else if (name == "app") {
      type = extensions::ExtensionBuilder::Type::PLATFORM_APP;
    } else {
      NOTREACHED();
    }
    extension_ = extensions::ExtensionBuilder("test extension", type).Build();

    chrome::ShowExtensionInstallBlockedByParentDialog(
        chrome::ExtensionInstalledBlockedByParentDialogAction::kAdd,
        extension_.get(), browser()->tab_strip_model()->GetWebContentsAt(0),
        base::DoNothing());
  }

 private:
  scoped_refptr<const extensions::Extension> extension_;
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallBlockedByParentDialogViewTest,
                       InvokeUi_extension) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallBlockedByParentDialogViewTest,
                       InvokeUi_app) {
  ShowAndVerifyUi();
}
