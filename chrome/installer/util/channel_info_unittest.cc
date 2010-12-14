// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/channel_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::wstring kChannelStable;
const std::wstring kChannelBeta(L"beta");
const std::wstring kChannelDev(L"dev");

}  // namespace

TEST(ChannelInfoTest, Channels) {
  installer::ChannelInfo ci;
  std::wstring channel;

  ci.set_value(L"");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelStable, channel);
  ci.set_value(L"-CEEE");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelStable, channel);
  ci.set_value(L"-CEEE-multi");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelStable, channel);
  ci.set_value(L"-full");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelStable, channel);

  ci.set_value(L"2.0-beta");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelBeta, channel);
  ci.set_value(L"2.0-beta-spam");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelBeta, channel);
  ci.set_value(L"2.0-spam-beta");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelBeta, channel);

  ci.set_value(L"2.0-dev");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelDev, channel);
  ci.set_value(L"2.0-kinda-dev");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelDev, channel);
  ci.set_value(L"2.0-dev-eloper");
  EXPECT_TRUE(ci.GetChannelName(&channel));
  EXPECT_EQ(kChannelDev, channel);

  ci.set_value(L"fuzzy");
  EXPECT_FALSE(ci.GetChannelName(&channel));
}

TEST(ChannelInfoTest, CEEE) {
  installer::ChannelInfo ci;

  ci.set_value(L"");
  EXPECT_TRUE(ci.SetCeee(true));
  EXPECT_TRUE(ci.IsCeee());
  EXPECT_EQ(L"-CEEE", ci.value());
  EXPECT_FALSE(ci.SetCeee(true));
  EXPECT_TRUE(ci.IsCeee());
  EXPECT_EQ(L"-CEEE", ci.value());
  EXPECT_TRUE(ci.SetCeee(false));
  EXPECT_FALSE(ci.IsCeee());
  EXPECT_EQ(L"", ci.value());
  EXPECT_FALSE(ci.SetCeee(false));
  EXPECT_FALSE(ci.IsCeee());
  EXPECT_EQ(L"", ci.value());

  ci.set_value(L"2.0-beta");
  EXPECT_TRUE(ci.SetCeee(true));
  EXPECT_TRUE(ci.IsCeee());
  EXPECT_EQ(L"2.0-beta-CEEE", ci.value());
  EXPECT_FALSE(ci.SetCeee(true));
  EXPECT_TRUE(ci.IsCeee());
  EXPECT_EQ(L"2.0-beta-CEEE", ci.value());
  EXPECT_TRUE(ci.SetCeee(false));
  EXPECT_FALSE(ci.IsCeee());
  EXPECT_EQ(L"2.0-beta", ci.value());
  EXPECT_FALSE(ci.SetCeee(false));
  EXPECT_FALSE(ci.IsCeee());
  EXPECT_EQ(L"2.0-beta", ci.value());
}

TEST(ChannelInfoTest, FullInstall) {
  installer::ChannelInfo ci;

  ci.set_value(L"");
  EXPECT_TRUE(ci.SetFullInstall(true));
  EXPECT_TRUE(ci.IsFullInstall());
  EXPECT_EQ(L"-full", ci.value());
  EXPECT_FALSE(ci.SetFullInstall(true));
  EXPECT_TRUE(ci.IsFullInstall());
  EXPECT_EQ(L"-full", ci.value());
  EXPECT_TRUE(ci.SetFullInstall(false));
  EXPECT_FALSE(ci.IsFullInstall());
  EXPECT_EQ(L"", ci.value());
  EXPECT_FALSE(ci.SetFullInstall(false));
  EXPECT_FALSE(ci.IsFullInstall());
  EXPECT_EQ(L"", ci.value());

  ci.set_value(L"2.0-beta");
  EXPECT_TRUE(ci.SetFullInstall(true));
  EXPECT_TRUE(ci.IsFullInstall());
  EXPECT_EQ(L"2.0-beta-full", ci.value());
  EXPECT_FALSE(ci.SetFullInstall(true));
  EXPECT_TRUE(ci.IsFullInstall());
  EXPECT_EQ(L"2.0-beta-full", ci.value());
  EXPECT_TRUE(ci.SetFullInstall(false));
  EXPECT_FALSE(ci.IsFullInstall());
  EXPECT_EQ(L"2.0-beta", ci.value());
  EXPECT_FALSE(ci.SetFullInstall(false));
  EXPECT_FALSE(ci.IsFullInstall());
  EXPECT_EQ(L"2.0-beta", ci.value());
}

TEST(ChannelInfoTest, MultiInstall) {
  installer::ChannelInfo ci;

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
