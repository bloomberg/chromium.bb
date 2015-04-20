// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/plugins_field_trial.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/test/mock_entropy_provider.h"
#include "components/content_settings/core/browser/content_settings_default_provider.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/plugins/common/plugins_switches.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {

namespace {

const auto& kFieldTrialName = PluginsFieldTrial::kFieldTrialName;

void ForceFieldTrialGroup(const std::string& group_name) {
  using base::FieldTrialList;
  ASSERT_TRUE(FieldTrialList::CreateFieldTrial(kFieldTrialName, group_name));
}

}  // namespace

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
  ASSERT_FALSE(base::FieldTrialList::TrialExists(kFieldTrialName));
  EXPECT_FALSE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, FieldTrialEnabled) {
  ForceFieldTrialGroup("Enabled");
  EXPECT_TRUE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, FieldTrialDisabled) {
  ForceFieldTrialGroup("Disabled");
  EXPECT_FALSE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, SwitchEnabled) {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(plugins::switches::kEnablePluginPowerSaver);
  EXPECT_TRUE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, SwitchDisabled) {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(plugins::switches::kDisablePluginPowerSaver);
  EXPECT_FALSE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, SwitchOverridesFieldTrial1) {
  ForceFieldTrialGroup("Disabled");
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(plugins::switches::kEnablePluginPowerSaver);
  EXPECT_TRUE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, SwitchOverridesFieldTrial2) {
  ForceFieldTrialGroup("Enabled");
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(plugins::switches::kDisablePluginPowerSaver);
  EXPECT_FALSE(PluginsFieldTrial::IsPluginPowerSaverEnabled());
}

TEST_F(PluginsFieldTrialTest, NoPrefLeftBehind) {
  ForceFieldTrialGroup("Enabled");
  user_prefs::TestingPrefServiceSyncable prefs;
  {
    DefaultProvider::RegisterProfilePrefs(prefs.registry());
    DefaultProvider default_provider(&prefs, false);
  }
  EXPECT_EQ(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
            prefs.GetInteger(prefs::kDefaultPluginsSetting));
  EXPECT_FALSE(prefs.HasPrefPath(prefs::kDefaultPluginsSetting));
}

}  // namespace content_settings
