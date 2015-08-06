// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/plugins_field_trial.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/test/mock_entropy_provider.h"
#include "components/content_settings/core/browser/content_settings_default_provider.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/plugins/common/plugins_switches.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::FieldTrialList;

namespace content_settings {

const char* kEnableFieldTrial = PluginsFieldTrial::kEnableFieldTrial;
const char* kForceFieldTrial = PluginsFieldTrial::kForceFieldTrial;

class PluginsFieldTrialTest : public testing::Test {
 public:
  PluginsFieldTrialTest() : field_trial_list_(new base::MockEntropyProvider) {}

 private:
  base::FieldTrialList field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(PluginsFieldTrialTest);
};

TEST_F(PluginsFieldTrialTest, DisabledByDefault) {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  ASSERT_FALSE(cl->HasSwitch(plugins::switches::kDisablePluginPowerSaver));
  ASSERT_FALSE(cl->HasSwitch(plugins::switches::kEnablePluginPowerSaver));
  ASSERT_FALSE(base::FieldTrialList::TrialExists(kEnableFieldTrial));
  ASSERT_FALSE(base::FieldTrialList::TrialExists(kForceFieldTrial));
  EXPECT_FALSE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, EnabledByFieldTrial) {
  ASSERT_TRUE(FieldTrialList::CreateFieldTrial(kForceFieldTrial, "Dogfood"));
  EXPECT_TRUE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, DisabledByFieldTrial) {
  ASSERT_TRUE(FieldTrialList::CreateFieldTrial(kEnableFieldTrial, "Disabled"));
  EXPECT_FALSE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, EnabledBySwitch) {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(plugins::switches::kEnablePluginPowerSaver);
  EXPECT_TRUE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, DisabledBySwitch) {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(plugins::switches::kDisablePluginPowerSaver);
  EXPECT_FALSE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, SwitchOverridesFieldTrial1) {
  ASSERT_TRUE(FieldTrialList::CreateFieldTrial(kForceFieldTrial, "Disabled"));
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(plugins::switches::kEnablePluginPowerSaver);
  EXPECT_TRUE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, SwitchOverridesFieldTrial2) {
  ASSERT_TRUE(FieldTrialList::CreateFieldTrial(kEnableFieldTrial, "Enabled"));
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(plugins::switches::kDisablePluginPowerSaver);
  EXPECT_FALSE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, NoPrefLeftBehind) {
  ASSERT_TRUE(FieldTrialList::CreateFieldTrial(kEnableFieldTrial, "Enabled"));
  user_prefs::TestingPrefServiceSyncable prefs;
  {
    DefaultProvider::RegisterProfilePrefs(prefs.registry());
    DefaultProvider default_provider(&prefs, false);
  }
  const std::string& default_plugin_setting_pref_name =
      WebsiteSettingsRegistry::GetInstance()
          ->Get(CONTENT_SETTINGS_TYPE_PLUGINS)
          ->default_value_pref_name();
  EXPECT_EQ(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
            prefs.GetInteger(default_plugin_setting_pref_name));
  EXPECT_FALSE(prefs.HasPrefPath(default_plugin_setting_pref_name));
}

}  // namespace content_settings
