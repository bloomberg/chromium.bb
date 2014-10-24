// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/public/extensions_delegate.h"
#include "base/macros.h"
#include "extensions/browser/install/extension_install_ui.h"
#include "extensions/common/extension_set.h"

namespace athena {
namespace {

class TestExtensionsDelegate : public ExtensionsDelegate {
 public:
  TestExtensionsDelegate() {}

  ~TestExtensionsDelegate() override {}

 private:
  // ExtensionsDelegate:
  content::BrowserContext* GetBrowserContext() const override {
    return nullptr;
  }
  const extensions::ExtensionSet& GetInstalledExtensions() override {
    return shell_extensions_;
  }
  bool LaunchApp(const std::string& app_id) override { return true; }
  bool UnloadApp(const std::string& app_id) override { return false; }

  scoped_ptr<extensions::ExtensionInstallUI> CreateExtensionInstallUI()
      override {
    return scoped_ptr<extensions::ExtensionInstallUI>();
  }

  extensions::ExtensionSet shell_extensions_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionsDelegate);
};

}  // namespace

// static
void ExtensionsDelegate::CreateExtensionsDelegateForTest() {
  new TestExtensionsDelegate();
}

}  // namespace athena
