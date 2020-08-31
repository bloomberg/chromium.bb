// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/secure_dns_util.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "components/embedder_support/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/dns/dns_config_overrides.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace chrome_browser_net {

namespace {

const char kAlternateErrorPagesBackup[] = "alternate_error_pages.backup";

}  // namespace

namespace secure_dns {

class DNSUtilTest : public testing::Test {
 public:
  void SetUp() override { DisableRedesign(); }

  void EnableRedesign() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPrivacySettingsRedesign, base::FieldTrialParams());
  }

  void DisableRedesign() { scoped_feature_list_.Reset(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DNSUtilTest, MigrateProbesPref) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterBooleanPref(
      embedder_support::kAlternateErrorPagesEnabled, true);
  prefs.registry()->RegisterBooleanPref(kAlternateErrorPagesBackup, true);

  const PrefService::Preference* current_pref =
      prefs.FindPreference(embedder_support::kAlternateErrorPagesEnabled);
  const PrefService::Preference* backup_pref =
      prefs.FindPreference(kAlternateErrorPagesBackup);

  // No migration happens if the privacy settings redesign is not enabled.
  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(backup_pref->HasUserSetting());

  // The hardcoded default value of TRUE gets correctly migrated.
  EnableRedesign();
  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_TRUE(backup_pref->HasUserSetting());
  EXPECT_TRUE(prefs.GetBoolean(kAlternateErrorPagesBackup));

  // And correctly restored.
  DisableRedesign();
  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_TRUE(current_pref->HasUserSetting());
  EXPECT_TRUE(prefs.GetBoolean(embedder_support::kAlternateErrorPagesEnabled));
  EXPECT_FALSE(backup_pref->HasUserSetting());

  // An explicit user value of TRUE will be correctly migrated.
  EnableRedesign();
  prefs.SetBoolean(embedder_support::kAlternateErrorPagesEnabled, true);
  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_TRUE(backup_pref->HasUserSetting());
  EXPECT_TRUE(prefs.GetBoolean(kAlternateErrorPagesBackup));

  // And correctly restored.
  DisableRedesign();
  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_TRUE(current_pref->HasUserSetting());
  EXPECT_TRUE(prefs.GetBoolean(embedder_support::kAlternateErrorPagesEnabled));
  EXPECT_FALSE(backup_pref->HasUserSetting());

  // An explicit user value of FALSE will also be correctly migrated.
  EnableRedesign();
  prefs.SetBoolean(embedder_support::kAlternateErrorPagesEnabled, false);
  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_TRUE(backup_pref->HasUserSetting());
  EXPECT_FALSE(prefs.GetBoolean(kAlternateErrorPagesBackup));

  // And correctly restored.
  DisableRedesign();
  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_TRUE(current_pref->HasUserSetting());
  EXPECT_FALSE(prefs.GetBoolean(embedder_support::kAlternateErrorPagesEnabled));
  EXPECT_FALSE(backup_pref->HasUserSetting());

  // A policy-sourced value of TRUE takes precedence over the user-sourced value
  // of FALSE when the preference is evaluated. However, it will still be the
  // user-sourced value of FALSE that will be migrated.
  prefs.SetManagedPref(embedder_support::kAlternateErrorPagesEnabled,
                       std::make_unique<base::Value>(true));
  EnableRedesign();
  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_TRUE(backup_pref->HasUserSetting());
  EXPECT_FALSE(prefs.GetBoolean(kAlternateErrorPagesBackup));

  // And correctly restored.
  DisableRedesign();
  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_TRUE(current_pref->HasUserSetting());
  {
    const base::Value* user_pref =
        prefs.GetUserPref(embedder_support::kAlternateErrorPagesEnabled);
    ASSERT_TRUE(user_pref->is_bool());
    EXPECT_FALSE(user_pref->GetBool());
  }
  EXPECT_FALSE(backup_pref->HasUserSetting());

  // After clearing the user-sourced value, the hardcoded value of TRUE should
  // be the value which is migrated, even if it is overridden by
  // a policy-sourced value of FALSE.
  prefs.ClearPref(embedder_support::kAlternateErrorPagesEnabled);
  prefs.SetManagedPref(embedder_support::kAlternateErrorPagesEnabled,
                       std::make_unique<base::Value>(false));
  EnableRedesign();
  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_TRUE(backup_pref->HasUserSetting());
  EXPECT_TRUE(prefs.GetBoolean(kAlternateErrorPagesBackup));

  // And correctly restored.
  DisableRedesign();
  MigrateProbesSettingToOrFromBackup(&prefs);
  EXPECT_TRUE(current_pref->HasUserSetting());
  {
    const base::Value* user_pref =
        prefs.GetUserPref(embedder_support::kAlternateErrorPagesEnabled);
    ASSERT_TRUE(user_pref->is_bool());
    EXPECT_TRUE(user_pref->GetBool());
  }
  EXPECT_FALSE(backup_pref->HasUserSetting());
}

TEST(DNSUtil, SplitGroup) {
  EXPECT_THAT(SplitGroup("a"), ElementsAre("a"));
  EXPECT_THAT(SplitGroup("a b"), ElementsAre("a", "b"));
  EXPECT_THAT(SplitGroup("a \tb\nc"), ElementsAre("a", "b\nc"));
  EXPECT_THAT(SplitGroup(" \ta b\n"), ElementsAre("a", "b"));
}

TEST(DNSUtil, IsValidGroup) {
  EXPECT_TRUE(IsValidGroup(""));
  EXPECT_TRUE(IsValidGroup("https://valid"));
  EXPECT_TRUE(IsValidGroup("https://valid https://valid2"));

  EXPECT_FALSE(IsValidGroup("https://valid invalid"));
  EXPECT_FALSE(IsValidGroup("invalid https://valid"));
  EXPECT_FALSE(IsValidGroup("invalid"));
  EXPECT_FALSE(IsValidGroup("invalid invalid2"));
}

TEST(DNSUtil, ApplyDohTemplatePost) {
  std::string post_template("https://valid");
  net::DnsConfigOverrides overrides;
  ApplyTemplate(&overrides, post_template);

  EXPECT_THAT(overrides.dns_over_https_servers,
              testing::Optional(ElementsAre(net::DnsOverHttpsServerConfig(
                  {post_template, true /* use_post */}))));
}

TEST(DNSUtil, ApplyDohTemplateGet) {
  std::string get_template("https://valid/{?dns}");
  net::DnsConfigOverrides overrides;
  ApplyTemplate(&overrides, get_template);

  EXPECT_THAT(overrides.dns_over_https_servers,
              testing::Optional(ElementsAre(net::DnsOverHttpsServerConfig(
                  {get_template, false /* use_post */}))));
}

}  // namespace secure_dns

}  // namespace chrome_browser_net
