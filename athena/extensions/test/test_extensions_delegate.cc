// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/public/extensions_delegate.h"
#include "base/macros.h"
#include "extensions/common/extension_set.h"

namespace athena {
namespace {

class TestExtensionsDelegate : public ExtensionsDelegate {
 public:
  TestExtensionsDelegate() {}

  virtual ~TestExtensionsDelegate() {}

 private:
  // ExtensionsDelegate:
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE {
    return NULL;
  }
  virtual const extensions::ExtensionSet& GetInstalledExtensions() OVERRIDE {
    return shell_extensions_;
  }
  virtual bool LaunchApp(const std::string& app_id) OVERRIDE { return true; }
  virtual bool UnloadApp(const std::string& app_id) OVERRIDE { return false; }

  extensions::ExtensionSet shell_extensions_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionsDelegate);
};

}  // namespace

// static
void ExtensionsDelegate::CreateExtensionsDelegateForTest() {
  new TestExtensionsDelegate();
}

}  // namespace athena
