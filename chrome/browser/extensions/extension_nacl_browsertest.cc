// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "webkit/plugins/webplugininfo.h"

using content::PluginService;
using content::WebContents;
using extensions::Extension;

namespace {

const char* kExtensionId = "bjjcibdiodkkeanflmiijlcfieiemced";

// This class tests that the Native Client plugin is blocked unless the
// .nexe is part of an extension from the Chrome Webstore.
class NaClExtensionTest : public ExtensionBrowserTest {
 public:
  NaClExtensionTest() {}

 protected:
  enum InstallType {
    INSTALL_TYPE_COMPONENT,
    INSTALL_TYPE_UNPACKED,
    INSTALL_TYPE_FROM_WEBSTORE,
    INSTALL_TYPE_NON_WEBSTORE,
  };

  const Extension* InstallExtension(InstallType install_type) {
    FilePath file_path = test_data_dir_.AppendASCII("native_client");
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();
    const Extension* extension = NULL;
    switch (install_type) {
      case INSTALL_TYPE_COMPONENT:
        if (LoadExtensionAsComponent(file_path)) {
          extension = service->GetExtensionById(kExtensionId, false);
        }
        break;

      case INSTALL_TYPE_UNPACKED:
        // Install the extension from a folder so it's unpacked.
        if (LoadExtension(file_path)) {
          extension = service->GetExtensionById(kExtensionId, false);
        }
        break;

      case INSTALL_TYPE_FROM_WEBSTORE:
        // Install native_client.crx from the webstore.
        if (InstallExtensionFromWebstore(file_path, 1)) {
          extension = service->GetExtensionById(last_loaded_extension_id_,
                                                false);
        }
        break;

      case INSTALL_TYPE_NON_WEBSTORE:
        // Install native_client.crx but not from the webstore.
        if (ExtensionBrowserTest::InstallExtension(file_path, 1)) {
          extension = service->GetExtensionById(last_loaded_extension_id_,
                                                false);
        }
        break;
    }
    return extension;
  }

  bool IsNaClPluginLoaded() {
    FilePath path;
    if (PathService::Get(chrome::FILE_NACL_PLUGIN, &path)) {
      webkit::WebPluginInfo info;
      return PluginService::GetInstance()->GetPluginInfoByPath(path, &info);
    }
    return false;
  }

  void CheckPluginsCreated(const Extension* extension, bool should_create) {
    ui_test_utils::NavigateToURL(browser(),
                                 extension->GetResourceURL("test.html"));
    // Don't run tests if the NaCl plugin isn't loaded.
    if (!IsNaClPluginLoaded())
      return;

    bool embedded_plugin_created = false;
    bool content_handler_plugin_created = false;
    WebContents* web_contents = chrome::GetActiveWebContents(browser());
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
        web_contents->GetRenderViewHost(),
        "",
        "window.domAutomationController.send(EmbeddedPluginCreated());",
        &embedded_plugin_created));
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
        web_contents->GetRenderViewHost(),
        "",
        "window.domAutomationController.send(ContentHandlerPluginCreated());",
        &content_handler_plugin_created));

    EXPECT_EQ(should_create, embedded_plugin_created);
    EXPECT_EQ(should_create, content_handler_plugin_created);
  }
};

// Test that the NaCl plugin isn't blocked for Webstore extensions.
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, WebStoreExtension) {
  ASSERT_TRUE(test_server()->Start());

  const Extension* extension = InstallExtension(INSTALL_TYPE_FROM_WEBSTORE);
  ASSERT_TRUE(extension);
  CheckPluginsCreated(extension, true);
}

// Test that the NaCl plugin is blocked for non-Webstore extensions.
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, NonWebStoreExtension) {
  ASSERT_TRUE(test_server()->Start());

  const Extension* extension = InstallExtension(INSTALL_TYPE_NON_WEBSTORE);
  ASSERT_TRUE(extension);
  CheckPluginsCreated(extension, false);
}

// Test that the NaCl plugin isn't blocked for component extensions.
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, ComponentExtension) {
  ASSERT_TRUE(test_server()->Start());

  const Extension* extension = InstallExtension(INSTALL_TYPE_COMPONENT);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->location(), Extension::COMPONENT);
  CheckPluginsCreated(extension, true);
}

// Test that the NaCl plugin isn't blocked for unpacked extensions.
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, UnpackedExtension) {
  ASSERT_TRUE(test_server()->Start());

  const Extension* extension = InstallExtension(INSTALL_TYPE_UNPACKED);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->location(), Extension::LOAD);
  CheckPluginsCreated(extension, true);
}

}  // namespace
