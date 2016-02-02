// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::PluginService;
using content::WebContents;
using extensions::Extension;
using extensions::Manifest;

namespace {

const char kExtensionId[] = "bjjcibdiodkkeanflmiijlcfieiemced";

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
  enum PluginType {
    PLUGIN_TYPE_NONE = 0,
    PLUGIN_TYPE_EMBED = 1,
    PLUGIN_TYPE_CONTENT_HANDLER = 2,
    PLUGIN_TYPE_ALL = PLUGIN_TYPE_EMBED |
                      PLUGIN_TYPE_CONTENT_HANDLER,
  };


  const Extension* InstallExtension(const base::FilePath& file_path,
                                    InstallType install_type) {
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
          extension = service->GetExtensionById(last_loaded_extension_id(),
                                                false);
        }
        break;

      case INSTALL_TYPE_NON_WEBSTORE:
        // Install native_client.crx but not from the webstore.
        if (ExtensionBrowserTest::InstallExtension(file_path, 1)) {
          extension = service->GetExtensionById(last_loaded_extension_id(),
                                                false);
        }
        break;
    }
    return extension;
  }

  const Extension* InstallExtension(InstallType install_type) {
    base::FilePath file_path = test_data_dir_.AppendASCII("native_client");
    return InstallExtension(file_path, install_type);
  }

  const Extension* InstallHostedApp() {
    base::FilePath file_path = test_data_dir_.AppendASCII(
        "native_client_hosted_app");
    return InstallExtension(file_path, INSTALL_TYPE_FROM_WEBSTORE);
  }

  bool IsNaClPluginLoaded() {
    base::FilePath path;
    if (PathService::Get(chrome::FILE_NACL_PLUGIN, &path)) {
      content::WebPluginInfo info;
      return PluginService::GetInstance()->GetPluginInfoByPath(path, &info);
    }
    return false;
  }

  void CheckPluginsCreated(const GURL& url, PluginType expected_to_succeed) {
    ui_test_utils::NavigateToURL(browser(), url);
    // Don't run tests if the NaCl plugin isn't loaded.
    if (!IsNaClPluginLoaded())
      return;

    bool embedded_plugin_created = false;
    bool content_handler_plugin_created = false;
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        "window.domAutomationController.send(EmbeddedPluginCreated());",
        &embedded_plugin_created));
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        "window.domAutomationController.send(ContentHandlerPluginCreated());",
        &content_handler_plugin_created));

    EXPECT_EQ(embedded_plugin_created,
              (expected_to_succeed & PLUGIN_TYPE_EMBED) != 0);
    EXPECT_EQ(content_handler_plugin_created,
              (expected_to_succeed & PLUGIN_TYPE_CONTENT_HANDLER) != 0);
  }

  void CheckPluginsCreated(const Extension* extension,
                           PluginType expected_to_succeed) {
    CheckPluginsCreated(extension->GetResourceURL("test.html"),
                        expected_to_succeed);
  }
};

// Test that the NaCl plugin isn't blocked for Webstore extensions.
// Disabled: http://crbug.com/319892
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, DISABLED_WebStoreExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension = InstallExtension(INSTALL_TYPE_FROM_WEBSTORE);
  ASSERT_TRUE(extension);
  CheckPluginsCreated(extension, PLUGIN_TYPE_ALL);
}

// Test that the NaCl plugin is blocked for non-Webstore extensions.
// Disabled: http://crbug.com/319892
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, DISABLED_NonWebStoreExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension = InstallExtension(INSTALL_TYPE_NON_WEBSTORE);
  ASSERT_TRUE(extension);
  CheckPluginsCreated(extension, PLUGIN_TYPE_NONE);
}

// Test that the NaCl plugin isn't blocked for component extensions.
// Disabled: http://crbug.com/319892
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, DISABLED_ComponentExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension = InstallExtension(INSTALL_TYPE_COMPONENT);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->location(), Manifest::COMPONENT);
  CheckPluginsCreated(extension, PLUGIN_TYPE_ALL);
}

// Test that the NaCl plugin isn't blocked for unpacked extensions.
// Disabled: http://crbug.com/319892
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, DISABLED_UnpackedExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension = InstallExtension(INSTALL_TYPE_UNPACKED);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->location(), Manifest::UNPACKED);
  CheckPluginsCreated(extension, PLUGIN_TYPE_ALL);
}

// Test that the NaCl plugin is blocked for non chrome-extension urls, except
// if it's a content (MIME type) handler.
// Disabled: http://crbug.com/319892
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, DISABLED_NonExtensionScheme) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension = InstallExtension(INSTALL_TYPE_FROM_WEBSTORE);
  ASSERT_TRUE(extension);
  CheckPluginsCreated(
      embedded_test_server()->GetURL("/extensions/native_client/test.html"),
      PLUGIN_TYPE_CONTENT_HANDLER);
}

// Test that NaCl plugin isn't blocked for hosted app URLs.
IN_PROC_BROWSER_TEST_F(NaClExtensionTest, HostedApp) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url =
      embedded_test_server()->GetURL("/extensions/native_client/test.html");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  replace_host.ClearPort();
  url = url.ReplaceComponents(replace_host);

  const Extension* extension = InstallHostedApp();
  ASSERT_TRUE(extension);
  CheckPluginsCreated(url, PLUGIN_TYPE_ALL);
}

}  // namespace
