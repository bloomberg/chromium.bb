// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/ui/browser.h"

class ExtensionManagementApiTest : public ExtensionApiTest {
 public:
  virtual void InstallExtensions() {
    FilePath basedir = test_data_dir_.AppendASCII("management");

    // Load 4 enabled items.
    ASSERT_TRUE(LoadExtension(basedir.AppendASCII("enabled_extension")));
    ASSERT_TRUE(LoadExtension(basedir.AppendASCII("enabled_app")));
    ASSERT_TRUE(LoadExtension(basedir.AppendASCII("description")));
    ASSERT_TRUE(LoadExtension(basedir.AppendASCII("permissions")));

    // Load 2 disabled items.
    ExtensionsService* service = browser()->profile()->GetExtensionsService();
    ASSERT_TRUE(LoadExtension(basedir.AppendASCII("disabled_extension")));
    service->DisableExtension(last_loaded_extension_id_);
    ASSERT_TRUE(LoadExtension(basedir.AppendASCII("disabled_app")));
    service->DisableExtension(last_loaded_extension_id_);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, Basics) {
  InstallExtensions();
  ASSERT_TRUE(RunExtensionSubtest("management/test", "basics.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, Uninstall) {
  InstallExtensions();
  ASSERT_TRUE(RunExtensionSubtest("management/test", "uninstall.html"));
}
