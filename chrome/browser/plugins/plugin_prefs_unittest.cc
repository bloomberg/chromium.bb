// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_prefs.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using content::BrowserThread;
using content::PluginService;

namespace {

void CanEnablePluginCallback(const base::Closure& quit_closure,
                             bool expected_can_change,
                             bool did_change) {
  EXPECT_EQ(expected_can_change, did_change);
  quit_closure.Run();
}

#if !(defined(OS_LINUX) && defined(USE_AURA))
base::FilePath GetComponentUpdatedPepperFlashPath(
    const base::FilePath::StringType& version) {
  base::FilePath path;
  EXPECT_TRUE(PathService::Get(
      chrome::DIR_COMPONENT_UPDATED_PEPPER_FLASH_PLUGIN, &path));
  path = path.Append(version);
  path = path.Append(chrome::kPepperFlashPluginFilename);
  return path;
}

base::FilePath GetBundledPepperFlashPath() {
  base::FilePath path;
  EXPECT_TRUE(PathService::Get(chrome::FILE_PEPPER_FLASH_PLUGIN, &path));
  return path;
}
#endif  // !(defined(OS_LINUX) && defined(USE_AURA))

void GotPlugins(const base::Closure& quit_closure,
                const std::vector<content::WebPluginInfo>& plugins) {
  quit_closure.Run();
}

}  // namespace

class PluginPrefsTest : public ::testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    plugin_prefs_ = new PluginPrefs();
  }

  void SetPolicyEnforcedPluginPatterns(
      const std::set<base::string16>& disabled,
      const std::set<base::string16>& disabled_exceptions,
      const std::set<base::string16>& enabled) {
    plugin_prefs_->SetPolicyEnforcedPluginPatterns(
        disabled, disabled_exceptions, enabled);
  }

 protected:
  void EnablePluginSynchronously(bool enabled,
                                 const base::FilePath& path,
                                 bool expected_can_change) {
    base::RunLoop run_loop;
    plugin_prefs_->EnablePlugin(
        enabled, path,
        base::Bind(&CanEnablePluginCallback, run_loop.QuitClosure(),
                   expected_can_change));
    run_loop.Run();
  }

  void RefreshPluginsSynchronously() {
    PluginService::GetInstance()->RefreshPlugins();
#if !defined(OS_WIN)
    // Can't go out of process in unit tests.
    content::RenderProcessHost::SetRunRendererInProcess(true);
#endif
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    PluginService::GetInstance()->GetPlugins(
        base::Bind(&GotPlugins, runner->QuitClosure()));
    runner->Run();
#if !defined(OS_WIN)
    content::RenderProcessHost::SetRunRendererInProcess(false);
#endif
  }

  scoped_refptr<PluginPrefs> plugin_prefs_;
};

TEST_F(PluginPrefsTest, DisabledByPolicy) {
  std::set<base::string16> disabled_plugins;
  disabled_plugins.insert(ASCIIToUTF16("Disable this!"));
  disabled_plugins.insert(ASCIIToUTF16("*Google*"));
  SetPolicyEnforcedPluginPatterns(disabled_plugins,
                                  std::set<base::string16>(),
                                  std::set<base::string16>());

  EXPECT_EQ(PluginPrefs::NO_POLICY,
            plugin_prefs_->PolicyStatusForPlugin(ASCIIToUTF16("42")));
  EXPECT_EQ(PluginPrefs::POLICY_DISABLED,
            plugin_prefs_->PolicyStatusForPlugin(
                ASCIIToUTF16("Disable this!")));
  EXPECT_EQ(PluginPrefs::POLICY_DISABLED,
            plugin_prefs_->PolicyStatusForPlugin(ASCIIToUTF16("Google Earth")));
}

TEST_F(PluginPrefsTest, EnabledByPolicy) {
  std::set<base::string16> enabled_plugins;
  enabled_plugins.insert(ASCIIToUTF16("Enable that!"));
  enabled_plugins.insert(ASCIIToUTF16("PDF*"));
  SetPolicyEnforcedPluginPatterns(std::set<base::string16>(),
                                  std::set<base::string16>(),
                                  enabled_plugins);

  EXPECT_EQ(PluginPrefs::NO_POLICY,
            plugin_prefs_->PolicyStatusForPlugin(ASCIIToUTF16("42")));
  EXPECT_EQ(PluginPrefs::POLICY_ENABLED,
            plugin_prefs_->PolicyStatusForPlugin(ASCIIToUTF16("Enable that!")));
  EXPECT_EQ(PluginPrefs::POLICY_ENABLED,
            plugin_prefs_->PolicyStatusForPlugin(ASCIIToUTF16("PDF Reader")));
}

TEST_F(PluginPrefsTest, EnabledAndDisabledByPolicy) {
  const base::string16 k42(ASCIIToUTF16("42"));
  const base::string16 kEnabled(ASCIIToUTF16("Enabled"));
  const base::string16 kEnabled2(ASCIIToUTF16("Enabled 2"));
  const base::string16 kEnabled3(ASCIIToUTF16("Enabled 3"));
  const base::string16 kException(ASCIIToUTF16("Exception"));
  const base::string16 kException2(ASCIIToUTF16("Exception 2"));
  const base::string16 kGoogleMars(ASCIIToUTF16("Google Mars"));
  const base::string16 kGoogleEarth(ASCIIToUTF16("Google Earth"));

  std::set<base::string16> disabled_plugins;
  std::set<base::string16> disabled_plugins_exceptions;
  std::set<base::string16> enabled_plugins;

  disabled_plugins.insert(kEnabled);
  disabled_plugins_exceptions.insert(kEnabled);
  enabled_plugins.insert(kEnabled);

  disabled_plugins_exceptions.insert(kException);

  disabled_plugins.insert(kEnabled2);
  enabled_plugins.insert(kEnabled2);

  disabled_plugins.insert(kException2);
  disabled_plugins_exceptions.insert(kException2);

  disabled_plugins_exceptions.insert(kEnabled3);
  enabled_plugins.insert(kEnabled3);

  SetPolicyEnforcedPluginPatterns(disabled_plugins,
                                  disabled_plugins_exceptions,
                                  enabled_plugins);

  EXPECT_EQ(PluginPrefs::NO_POLICY, plugin_prefs_->PolicyStatusForPlugin(k42));

  EXPECT_EQ(PluginPrefs::POLICY_ENABLED,
            plugin_prefs_->PolicyStatusForPlugin(kEnabled));
  EXPECT_EQ(PluginPrefs::POLICY_ENABLED,
            plugin_prefs_->PolicyStatusForPlugin(kEnabled2));
  EXPECT_EQ(PluginPrefs::POLICY_ENABLED,
            plugin_prefs_->PolicyStatusForPlugin(kEnabled3));

  EXPECT_EQ(PluginPrefs::NO_POLICY,
            plugin_prefs_->PolicyStatusForPlugin(kException));
  EXPECT_EQ(PluginPrefs::NO_POLICY,
            plugin_prefs_->PolicyStatusForPlugin(kException2));

  disabled_plugins.clear();
  disabled_plugins_exceptions.clear();
  enabled_plugins.clear();

  disabled_plugins.insert(ASCIIToUTF16("*"));
  disabled_plugins_exceptions.insert(ASCIIToUTF16("*Google*"));
  enabled_plugins.insert(kGoogleEarth);

  SetPolicyEnforcedPluginPatterns(disabled_plugins,
                                  disabled_plugins_exceptions,
                                  enabled_plugins);

  EXPECT_EQ(PluginPrefs::POLICY_ENABLED,
            plugin_prefs_->PolicyStatusForPlugin(kGoogleEarth));
  EXPECT_EQ(PluginPrefs::NO_POLICY,
            plugin_prefs_->PolicyStatusForPlugin(kGoogleMars));
  EXPECT_EQ(PluginPrefs::POLICY_DISABLED,
            plugin_prefs_->PolicyStatusForPlugin(k42));
}

// Linux Aura doesn't support NPAPI.
#if !(defined(OS_LINUX) && defined(USE_AURA))

TEST_F(PluginPrefsTest, UnifiedPepperFlashState) {
  content::TestBrowserThreadBundle browser_threads;
  base::ShadowingAtExitManager at_exit_manager_;  // Destroys the PluginService.

  PluginService::GetInstance()->Init();
  PluginService::GetInstance()->DisablePluginsDiscoveryForTesting();

  base::string16 component_updated_plugin_name(
      ASCIIToUTF16("Component-updated Pepper Flash"));
  content::WebPluginInfo component_updated_plugin_1(
      component_updated_plugin_name,
      GetComponentUpdatedPepperFlashPath(FILE_PATH_LITERAL("11.3.31.227")),
      ASCIIToUTF16("11.3.31.227"),
      ASCIIToUTF16(""));
  content::WebPluginInfo component_updated_plugin_2(
      component_updated_plugin_name,
      GetComponentUpdatedPepperFlashPath(FILE_PATH_LITERAL("11.3.31.228")),
      ASCIIToUTF16("11.3.31.228"),
      ASCIIToUTF16(""));
  content::WebPluginInfo bundled_plugin(ASCIIToUTF16("Pepper Flash"),
                                        GetBundledPepperFlashPath(),
                                        ASCIIToUTF16("11.3.31.229"),
                                        ASCIIToUTF16(""));

  PluginService::GetInstance()->RegisterInternalPlugin(
      component_updated_plugin_1, false);
  PluginService::GetInstance()->RegisterInternalPlugin(
      component_updated_plugin_2, false);
  PluginService::GetInstance()->RegisterInternalPlugin(bundled_plugin, false);

  RefreshPluginsSynchronously();

  // Set the state of any of the three plugins will affect the others.
  EnablePluginSynchronously(true, component_updated_plugin_1.path, true);
  EXPECT_TRUE(plugin_prefs_->IsPluginEnabled(component_updated_plugin_1));
  EXPECT_TRUE(plugin_prefs_->IsPluginEnabled(component_updated_plugin_2));
  EXPECT_TRUE(plugin_prefs_->IsPluginEnabled(bundled_plugin));

  EnablePluginSynchronously(false, bundled_plugin.path, true);
  EXPECT_FALSE(plugin_prefs_->IsPluginEnabled(component_updated_plugin_1));
  EXPECT_FALSE(plugin_prefs_->IsPluginEnabled(component_updated_plugin_2));
  EXPECT_FALSE(plugin_prefs_->IsPluginEnabled(bundled_plugin));

  EnablePluginSynchronously(true, component_updated_plugin_2.path, true);
  EXPECT_TRUE(plugin_prefs_->IsPluginEnabled(component_updated_plugin_1));
  EXPECT_TRUE(plugin_prefs_->IsPluginEnabled(component_updated_plugin_2));
  EXPECT_TRUE(plugin_prefs_->IsPluginEnabled(bundled_plugin));

  std::set<base::string16> disabled_plugins;
  disabled_plugins.insert(component_updated_plugin_name);
  SetPolicyEnforcedPluginPatterns(disabled_plugins,
                                  std::set<base::string16>(),
                                  std::set<base::string16>());

  // Policy settings should be respected.
  EXPECT_FALSE(plugin_prefs_->IsPluginEnabled(component_updated_plugin_1));
  EXPECT_FALSE(plugin_prefs_->IsPluginEnabled(component_updated_plugin_2));
  EXPECT_TRUE(plugin_prefs_->IsPluginEnabled(bundled_plugin));

  EnablePluginSynchronously(false, bundled_plugin.path, true);
  EXPECT_FALSE(plugin_prefs_->IsPluginEnabled(bundled_plugin));

  // Trying to change the state of a policy-enforced plugin should not take
  // effect. And it shouldn't change the state of other plugins either, even if
  // they are not restricted by any policy.
  EnablePluginSynchronously(true, component_updated_plugin_1.path, false);
  EXPECT_FALSE(plugin_prefs_->IsPluginEnabled(component_updated_plugin_1));
  EXPECT_FALSE(plugin_prefs_->IsPluginEnabled(component_updated_plugin_2));
  EXPECT_FALSE(plugin_prefs_->IsPluginEnabled(bundled_plugin));

  EnablePluginSynchronously(true, bundled_plugin.path, true);
  EXPECT_FALSE(plugin_prefs_->IsPluginEnabled(component_updated_plugin_1));
  EXPECT_FALSE(plugin_prefs_->IsPluginEnabled(component_updated_plugin_2));
  EXPECT_TRUE(plugin_prefs_->IsPluginEnabled(bundled_plugin));
}

#endif
