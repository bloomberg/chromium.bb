// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_prefs.h"

#include "base/utf_string_conversions.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/plugins/webplugininfo.h"

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

TEST_F(PluginPrefsTest, DisableGlobally) {
  MessageLoop message_loop;
  BrowserThread ui_thread(BrowserThread::UI, &message_loop);
  BrowserThread file_thread(BrowserThread::FILE, &message_loop);

  TestingBrowserProcess* browser_process =
      static_cast<TestingBrowserProcess*>(g_browser_process);
  TestingProfileManager profile_manager(browser_process);
  ASSERT_TRUE(profile_manager.SetUp());

  TestingProfile* profile_1 =
      profile_manager.CreateTestingProfile("Profile 1");
  PluginPrefs* plugin_prefs = PluginPrefs::GetForTestingProfile(profile_1);
  ASSERT_TRUE(plugin_prefs);

  webkit::WebPluginInfo plugin(ASCIIToUTF16("Foo"),
                               FilePath(FILE_PATH_LITERAL("/path/too/foo")),
                               ASCIIToUTF16("1.0.0"),
                               ASCIIToUTF16("Foo plug-in"));
  PluginPrefs::EnablePluginGlobally(false, plugin.path);

  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(plugin));

  TestingProfile* profile_2 =
      profile_manager.CreateTestingProfile("Profile 2");
  PluginPrefs* plugin_prefs_2 = PluginPrefs::GetForTestingProfile(profile_2);
  ASSERT_TRUE(plugin_prefs);
  EXPECT_FALSE(plugin_prefs_2->IsPluginEnabled(plugin));
}
