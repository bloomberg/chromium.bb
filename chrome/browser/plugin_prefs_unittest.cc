// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_prefs.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/plugins/npapi/mock_plugin_list.cc"
#include "webkit/plugins/webplugininfo.h"

using content::BrowserThread;
using content::PluginService;

namespace {

void CanEnablePluginCallback(const base::Closure& quit_closure,
                             bool expected_can_change,
                             bool did_change) {
  EXPECT_EQ(expected_can_change, did_change);
  quit_closure.Run();
}

FilePath GetComponentUpdatedPepperFlashPath(
    const FilePath::StringType& version) {
  FilePath path;
  EXPECT_TRUE(PathService::Get(
      chrome::DIR_COMPONENT_UPDATED_PEPPER_FLASH_PLUGIN, &path));
  path = path.Append(version);
  path = path.Append(chrome::kPepperFlashPluginFilename);
  return path;
}

FilePath GetBundledPepperFlashPath() {
  FilePath path;
  EXPECT_TRUE(PathService::Get(chrome::FILE_PEPPER_FLASH_PLUGIN, &path));
  return path;
}

}  // namespace

class PluginPrefsTest : public ::testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    plugin_prefs_ = new PluginPrefs();
  }

  void SetPolicyEnforcedPluginPatterns(
      const std::set<string16>& disabled,
      const std::set<string16>& disabled_exceptions,
      const std::set<string16>& enabled) {
    plugin_prefs_->SetPolicyEnforcedPluginPatterns(
        disabled, disabled_exceptions, enabled);
  }

 protected:
  void EnablePluginSynchronously(bool enabled,
                                 const FilePath& path,
                                 bool expected_can_change) {
    base::RunLoop run_loop;
    plugin_prefs_->EnablePlugin(
        enabled, path,
        base::Bind(&CanEnablePluginCallback, run_loop.QuitClosure(),
                   expected_can_change));
    run_loop.Run();
  }

  scoped_refptr<PluginPrefs> plugin_prefs_;
};

TEST_F(PluginPrefsTest, DisabledByPolicy) {
  std::set<string16> disabled_plugins;
  disabled_plugins.insert(ASCIIToUTF16("Disable this!"));
  disabled_plugins.insert(ASCIIToUTF16("*Google*"));
  SetPolicyEnforcedPluginPatterns(disabled_plugins,
                                  std::set<string16>(),
                                  std::set<string16>());

  EXPECT_EQ(PluginPrefs::NO_POLICY,
            plugin_prefs_->PolicyStatusForPlugin(ASCIIToUTF16("42")));
  EXPECT_EQ(PluginPrefs::POLICY_DISABLED,
            plugin_prefs_->PolicyStatusForPlugin(
                ASCIIToUTF16("Disable this!")));
  EXPECT_EQ(PluginPrefs::POLICY_DISABLED,
            plugin_prefs_->PolicyStatusForPlugin(ASCIIToUTF16("Google Earth")));
}

TEST_F(PluginPrefsTest, EnabledByPolicy) {
  std::set<string16> enabled_plugins;
  enabled_plugins.insert(ASCIIToUTF16("Enable that!"));
  enabled_plugins.insert(ASCIIToUTF16("PDF*"));
  SetPolicyEnforcedPluginPatterns(std::set<string16>(),
                                  std::set<string16>(),
                                  enabled_plugins);

  EXPECT_EQ(PluginPrefs::NO_POLICY,
            plugin_prefs_->PolicyStatusForPlugin(ASCIIToUTF16("42")));
  EXPECT_EQ(PluginPrefs::POLICY_ENABLED,
            plugin_prefs_->PolicyStatusForPlugin(ASCIIToUTF16("Enable that!")));
  EXPECT_EQ(PluginPrefs::POLICY_ENABLED,
            plugin_prefs_->PolicyStatusForPlugin(ASCIIToUTF16("PDF Reader")));
}

TEST_F(PluginPrefsTest, EnabledAndDisabledByPolicy) {
  const string16 k42(ASCIIToUTF16("42"));
  const string16 kEnabled(ASCIIToUTF16("Enabled"));
  const string16 kEnabled2(ASCIIToUTF16("Enabled 2"));
  const string16 kEnabled3(ASCIIToUTF16("Enabled 3"));
  const string16 kException(ASCIIToUTF16("Exception"));
  const string16 kException2(ASCIIToUTF16("Exception 2"));
  const string16 kGoogleMars(ASCIIToUTF16("Google Mars"));
  const string16 kGoogleEarth(ASCIIToUTF16("Google Earth"));

  std::set<string16> disabled_plugins;
  std::set<string16> disabled_plugins_exceptions;
  std::set<string16> enabled_plugins;

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

TEST_F(PluginPrefsTest, UnifiedPepperFlashState) {
  base::ShadowingAtExitManager at_exit_manager_;  // Destroys the PluginService.

  MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  webkit::npapi::MockPluginList plugin_list(NULL, 0);
  PluginService::GetInstance()->SetPluginListForTesting(&plugin_list);
  PluginService::GetInstance()->Init();
  plugin_prefs_->SetPluginListForTesting(&plugin_list);

  string16 component_updated_plugin_name(
      ASCIIToUTF16("Component-updated Pepper Flash"));
  webkit::WebPluginInfo component_updated_plugin_1(
      component_updated_plugin_name,
      GetComponentUpdatedPepperFlashPath(FILE_PATH_LITERAL("11.3.31.227")),
      ASCIIToUTF16("11.3.31.227"),
      ASCIIToUTF16(""));
  webkit::WebPluginInfo component_updated_plugin_2(
      component_updated_plugin_name,
      GetComponentUpdatedPepperFlashPath(FILE_PATH_LITERAL("11.3.31.228")),
      ASCIIToUTF16("11.3.31.228"),
      ASCIIToUTF16(""));
  webkit::WebPluginInfo bundled_plugin(ASCIIToUTF16("Pepper Flash"),
                                       GetBundledPepperFlashPath(),
                                       ASCIIToUTF16("11.3.31.229"),
                                       ASCIIToUTF16(""));

  plugin_list.AddPluginToLoad(component_updated_plugin_1);
  plugin_list.AddPluginToLoad(component_updated_plugin_2);
  plugin_list.AddPluginToLoad(bundled_plugin);

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

  std::set<string16> disabled_plugins;
  disabled_plugins.insert(component_updated_plugin_name);
  SetPolicyEnforcedPluginPatterns(disabled_plugins,
                                  std::set<string16>(),
                                  std::set<string16>());

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

  plugin_prefs_->SetPluginListForTesting(NULL);
  PluginService::GetInstance()->SetPluginListForTesting(NULL);
}
