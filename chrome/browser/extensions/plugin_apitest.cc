// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "net/base/filename_util.h"

using content::NavigationController;
using content::WebContents;
using extensions::Extension;

#if defined(OS_WIN)
// http://crbug.com/123851 : test flakily fails on win.
#define MAYBE_PluginLoadUnload DISABLED_PluginLoadUnload
#elif defined(OS_MACOSX) && defined(ADDRESS_SANITIZER)
// ExtensionBrowserTest.PluginLoadUnload started failing after the switch to
// dynamic ASan runtime library on Mac. See http://crbug.com/234591.
#define MAYBE_PluginLoadUnload DISABLED_PluginLoadUnload
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux http://crbug.com/238460
#define MAYBE_PluginLoadUnload DISABLED_PluginLoadUnload
#else
#define MAYBE_PluginLoadUnload PluginLoadUnload
#endif

// Tests that a renderer's plugin list is properly updated when we load and
// unload an extension that contains a plugin.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, MAYBE_PluginLoadUnload) {
  if (!content::PluginService::GetInstance()->NPAPIPluginsSupported())
    return;
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysAuthorize,
                                               true);

  base::FilePath extension_dir =
      test_data_dir_.AppendASCII("uitest").AppendASCII("plugins");

  ui_test_utils::NavigateToURL(browser(),
      net::FilePathToFileURL(extension_dir.AppendASCII("test.html")));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // With no extensions, the plugin should not be loaded.
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab, "testPluginWorks()", &result));
  EXPECT_FALSE(result);

  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  service->set_show_extensions_prompts(false);
  const size_t size_before = registry->enabled_extensions().size();
  const Extension* extension = LoadExtension(extension_dir);
  ASSERT_TRUE(extension);
  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());
  // Now the plugin should be in the cache.
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab, "testPluginWorks()", &result));
  // We don't allow extension plugins to run on ChromeOS.
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(result);
#else
  EXPECT_TRUE(result);
#endif

  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());
  UnloadExtension(extension->id());
  EXPECT_EQ(size_before, registry->enabled_extensions().size());

  // Now the plugin should be unloaded, and the page should be broken.

  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab, "testPluginWorks()", &result));
  EXPECT_FALSE(result);

  // If we reload the extension and page, it should work again.

  ASSERT_TRUE(LoadExtension(extension_dir));
  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(
            &browser()->tab_strip_model()->GetActiveWebContents()->
                GetController()));
    chrome::Reload(browser(), CURRENT_TAB);
    observer.Wait();
  }
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab, "testPluginWorks()", &result));
  // We don't allow extension plugins to run on ChromeOS.
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(result);
#else
  EXPECT_TRUE(result);
#endif
}

#if defined(OS_MACOSX) && defined(ADDRESS_SANITIZER)
// ExtensionBrowserTest.PluginPrivate started failing after the switch to
// dynamic ASan runtime library on Mac. See http://crbug.com/234591.
#define MAYBE_PluginPrivate DISABLED_PluginPrivate
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux http://crbug.com/238467
#define MAYBE_PluginPrivate DISABLED_PluginPrivate
#elif defined(OS_WIN) && defined(ARCH_CPU_X86_64)
// TODO(jschuh): Failing plugin tests. crbug.com/244653
#define MAYBE_PluginPrivate DISABLED_PluginPrivate
#else
#define MAYBE_PluginPrivate PluginPrivate
#endif
// Tests that private extension plugins are only visible to the extension.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, MAYBE_PluginPrivate) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  if (!content::PluginService::GetInstance()->NPAPIPluginsSupported())
    return;

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysAuthorize,
                                               true);

  base::FilePath extension_dir =
      test_data_dir_.AppendASCII("uitest").AppendASCII("plugins_private");

  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  service->set_show_extensions_prompts(false);
  const size_t size_before = registry->enabled_extensions().size();
  const Extension* extension = LoadExtension(extension_dir);
  ASSERT_TRUE(extension);
  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());

  // Load the test page through the extension URL, and the plugin should work.
  ui_test_utils::NavigateToURL(browser(),
      extension->GetResourceURL("test.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab, "testPluginWorks()", &result));
  // We don't allow extension plugins to run on ChromeOS.
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(result);
#else
  // TODO(bauerb): This might wrongly fail if the plugin takes too long
  // to load. Extension-private plugins don't appear in navigator.plugins, so
  // we can't check for the plugin in Javascript.
  EXPECT_TRUE(result);
#endif

  // Regression test for http://crbug.com/131811: The plugin should be
  // whitelisted for the extension (and only for the extension), so it should be
  // loaded even if content settings are set to block plugins.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                 CONTENT_SETTING_BLOCK);
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab, "testPluginWorks()", &result));
  // We don't allow extension plugins to run on ChromeOS.
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(result);
#else
  EXPECT_TRUE(result);
#endif

  // Now load it through a file URL. The plugin should not load.
  ui_test_utils::NavigateToURL(browser(),
      net::FilePathToFileURL(extension_dir.AppendASCII("test.html")));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab, "testPluginWorks()", &result));
  EXPECT_FALSE(result);
}
