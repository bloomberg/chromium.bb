// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/channel_info.h"

#include <utility>

#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using installer::ChannelInfo;

namespace {

const base::string16 kChannelStable(installer::kChromeChannelStable);
const base::string16 kChannelBeta(installer::kChromeChannelBeta);
const base::string16 kChannelDev(installer::kChromeChannelDev);

}  // namespace

TEST(ChannelInfoTest, Channels) {
  ChannelInfo ci;
  base::string16 channel;

  ci.set_value(L"");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelStable, channel);
  ci.set_value(L"-full");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelStable, channel);

  ci.set_value(L"1.1-beta");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelBeta, channel);
  ci.set_value(L"1.1-beta-foo");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelBeta, channel);
  ci.set_value(L"1.1-bar");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelBeta, channel);
  ci.set_value(L"1n1-foobar");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelBeta, channel);
  ci.set_value(L"foo-1.1-beta");
  EXPECT_FALSE(ci.GetChannelName(&channel));
  ci.set_value(L"2.0-beta");
  EXPECT_FALSE(ci.GetChannelName(&channel));

  ci.set_value(L"2.0-dev");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelDev, channel);
  ci.set_value(L"2.0-DEV");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelDev, channel);
  ci.set_value(L"2.0-dev-eloper");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelDev, channel);
  ci.set_value(L"2.0-doom");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelDev, channel);
  ci.set_value(L"250-doom");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelDev, channel);
  ci.set_value(L"bar-2.0-dev");
  EXPECT_FALSE(ci.GetChannelName(&channel));
  ci.set_value(L"1.0-dev");
  EXPECT_FALSE(ci.GetChannelName(&channel));

  ci.set_value(L"x64-dev");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelDev, channel);
  ci.set_value(L"foo-x64-dev");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelDev, channel);
  ci.set_value(L"x64-Dev");
  EXPECT_FALSE(ci.GetChannelName(&channel));

  ci.set_value(L"x64-beta");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelBeta, channel);
  ci.set_value(L"bar-x64-beta");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelBeta, channel);
  ci.set_value(L"x64-Beta");
  EXPECT_FALSE(ci.GetChannelName(&channel));

  ci.set_value(L"x64-stable");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelStable, channel);
  ci.set_value(L"baz-x64-stable");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelStable, channel);
  ci.set_value(L"x64-Stable");
  EXPECT_FALSE(ci.GetChannelName(&channel));

  ci.set_value(L"fuzzy");
  EXPECT_FALSE(ci.GetChannelName(&channel));
  ci.set_value(L"foo");
  EXPECT_FALSE(ci.GetChannelName(&channel));
}

TEST(ChannelInfoTest, FullInstall) {
  ChannelInfo ci;

  ci.set_value(L"");
  EXPECT_TRUE(ci.SetFullSuffix(true));
  EXPECT_TRUE(ci.HasFullSuffix());
  EXPECT_EQ(L"-full", ci.value());
  EXPECT_FALSE(ci.SetFullSuffix(true));
  EXPECT_TRUE(ci.HasFullSuffix());
  EXPECT_EQ(L"-full", ci.value());
  EXPECT_TRUE(ci.SetFullSuffix(false));
  EXPECT_FALSE(ci.HasFullSuffix());
  EXPECT_EQ(L"", ci.value());
  EXPECT_FALSE(ci.SetFullSuffix(false));
  EXPECT_FALSE(ci.HasFullSuffix());
  EXPECT_EQ(L"", ci.value());

  ci.set_value(L"2.0-beta");
  EXPECT_TRUE(ci.SetFullSuffix(true));
  EXPECT_TRUE(ci.HasFullSuffix());
  EXPECT_EQ(L"2.0-beta-full", ci.value());
  EXPECT_FALSE(ci.SetFullSuffix(true));
  EXPECT_TRUE(ci.HasFullSuffix());
  EXPECT_EQ(L"2.0-beta-full", ci.value());
  EXPECT_TRUE(ci.SetFullSuffix(false));
  EXPECT_FALSE(ci.HasFullSuffix());
  EXPECT_EQ(L"2.0-beta", ci.value());
  EXPECT_FALSE(ci.SetFullSuffix(false));
  EXPECT_FALSE(ci.HasFullSuffix());
  EXPECT_EQ(L"2.0-beta", ci.value());
}

TEST(ChannelInfoTest, MultiInstall) {
  ChannelInfo ci;

  ci.set_value(L"");
  EXPECT_TRUE(ci.SetMultiInstall(true));
  EXPECT_TRUE(ci.IsMultiInstall());
  EXPECT_EQ(L"-multi", ci.value());
  EXPECT_FALSE(ci.SetMultiInstall(true));
  EXPECT_TRUE(ci.IsMultiInstall());
  EXPECT_EQ(L"-multi", ci.value());
  EXPECT_TRUE(ci.SetMultiInstall(false));
  EXPECT_FALSE(ci.IsMultiInstall());
  EXPECT_EQ(L"", ci.value());
  EXPECT_FALSE(ci.SetMultiInstall(false));
  EXPECT_FALSE(ci.IsMultiInstall());
  EXPECT_EQ(L"", ci.value());

  ci.set_value(L"2.0-beta");
  EXPECT_TRUE(ci.SetMultiInstall(true));
  EXPECT_TRUE(ci.IsMultiInstall());
  EXPECT_EQ(L"2.0-beta-multi", ci.value());
  EXPECT_FALSE(ci.SetMultiInstall(true));
  EXPECT_TRUE(ci.IsMultiInstall());
  EXPECT_EQ(L"2.0-beta-multi", ci.value());
  EXPECT_TRUE(ci.SetMultiInstall(false));
  EXPECT_FALSE(ci.IsMultiInstall());
  EXPECT_EQ(L"2.0-beta", ci.value());
  EXPECT_FALSE(ci.SetMultiInstall(false));
  EXPECT_FALSE(ci.IsMultiInstall());
  EXPECT_EQ(L"2.0-beta", ci.value());
}

TEST(ChannelInfoTest, Migration) {
  ChannelInfo ci;

  ci.set_value(L"");
  EXPECT_TRUE(ci.SetMigratingSuffix(true));
  EXPECT_TRUE(ci.HasMigratingSuffix());
  EXPECT_EQ(L"-migrating", ci.value());
  EXPECT_FALSE(ci.SetMigratingSuffix(true));
  EXPECT_TRUE(ci.HasMigratingSuffix());
  EXPECT_EQ(L"-migrating", ci.value());
  EXPECT_TRUE(ci.SetMigratingSuffix(false));
  EXPECT_FALSE(ci.HasMigratingSuffix());
  EXPECT_EQ(L"", ci.value());
  EXPECT_FALSE(ci.SetMigratingSuffix(false));
  EXPECT_FALSE(ci.HasMigratingSuffix());
  EXPECT_EQ(L"", ci.value());

  ci.set_value(L"2.0-beta");
  EXPECT_TRUE(ci.SetMigratingSuffix(true));
  EXPECT_TRUE(ci.HasMigratingSuffix());
  EXPECT_EQ(L"2.0-beta-migrating", ci.value());
  EXPECT_FALSE(ci.SetMigratingSuffix(true));
  EXPECT_TRUE(ci.HasMigratingSuffix());
  EXPECT_EQ(L"2.0-beta-migrating", ci.value());
  EXPECT_TRUE(ci.SetMigratingSuffix(false));
  EXPECT_FALSE(ci.HasMigratingSuffix());
  EXPECT_EQ(L"2.0-beta", ci.value());
  EXPECT_FALSE(ci.SetMigratingSuffix(false));
  EXPECT_FALSE(ci.HasMigratingSuffix());
  EXPECT_EQ(L"2.0-beta", ci.value());
}

TEST(ChannelInfoTest, Combinations) {
  ChannelInfo ci;

  ci.set_value(L"2.0-beta-chromeframe");
  EXPECT_FALSE(ci.IsChrome());
  ci.set_value(L"2.0-beta-chromeframe-chrome");
  EXPECT_TRUE(ci.IsChrome());
}

TEST(ChannelInfoTest, ClearStage) {
  ChannelInfo ci;

  ci.set_value(L"");
  EXPECT_FALSE(ci.ClearStage());
  EXPECT_EQ(L"", ci.value());
  ci.set_value(L"-stage:spammy");
  EXPECT_TRUE(ci.ClearStage());
  EXPECT_EQ(L"", ci.value());

  ci.set_value(L"-multi");
  EXPECT_FALSE(ci.ClearStage());
  EXPECT_EQ(L"-multi", ci.value());
  ci.set_value(L"-stage:spammy-multi");
  EXPECT_TRUE(ci.ClearStage());
  EXPECT_EQ(L"-multi", ci.value());

  ci.set_value(L"2.0-beta-multi");
  EXPECT_FALSE(ci.ClearStage());
  EXPECT_EQ(L"2.0-beta-multi", ci.value());
  ci.set_value(L"2.0-beta-stage:spammy-multi");
  EXPECT_TRUE(ci.ClearStage());
  EXPECT_EQ(L"2.0-beta-multi", ci.value());

  ci.set_value(L"2.0-beta-stage:-multi");
  EXPECT_TRUE(ci.ClearStage());
  EXPECT_EQ(L"2.0-beta-multi", ci.value());
}

TEST(ChannelInfoTest, GetStatsDefault) {
  struct {
    const base::string16 base_value;
    const base::string16& expected_channel;
  } test_cases[] = {
    {L"", kChannelStable},
    {L"x64-stable", kChannelStable},
    {L"1.1-beta", kChannelBeta},
    {L"x64-beta", kChannelBeta},
    {L"2.0-dev", kChannelDev},
    {L"x64-dev", kChannelDev},
  };
  const base::string16 suffixes[] = {L"", L"-multi", L"-multi-chrome"};

  for (const auto& test_case : test_cases) {
    const base::string16& base_value = test_case.base_value;
    const base::string16& expected_channel = test_case.expected_channel;

    for (const auto& suffix : suffixes) {
      ChannelInfo ci;
      base::string16 channel;

      ci.set_value(base_value + suffix);
      EXPECT_EQ(L"", ci.GetStatsDefault());
      ci.set_value(base_value + L"-statsdef" + suffix);
      EXPECT_EQ(L"", ci.GetStatsDefault());
      ci.set_value(base_value + L"-statsdef_" + suffix);
      EXPECT_EQ(L"", ci.GetStatsDefault());
      ci.set_value(base_value + L"-statsdef_0" + suffix);
      EXPECT_EQ(L"0", ci.GetStatsDefault());
      EXPECT_TRUE(ci.GetChannelName(&channel));
      EXPECT_EQ(expected_channel, channel);
      ci.set_value(base_value + L"-statsdef_1" + suffix);
      EXPECT_EQ(L"1", ci.GetStatsDefault());
      EXPECT_TRUE(ci.GetChannelName(&channel));
      EXPECT_EQ(expected_channel, channel);
    }
  }
}

TEST(ChannelInfoTest, RemoveAllModifiersAndSuffixes) {
  ChannelInfo ci;

  ci.set_value(L"");
  EXPECT_FALSE(ci.RemoveAllModifiersAndSuffixes());
  EXPECT_EQ(L"", ci.value());

  ci.set_value(L"2.0-dev-multi-chrome-chromeframe-migrating");
  EXPECT_TRUE(ci.RemoveAllModifiersAndSuffixes());
  EXPECT_EQ(L"2.0-dev", ci.value());
}
