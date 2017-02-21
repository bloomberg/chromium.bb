// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_modes.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Gt;
using ::testing::Ne;
using ::testing::StrEq;
using ::testing::StrNe;

namespace install_static {

TEST(InstallModes, VerifyModes) {
  ASSERT_THAT(NUM_INSTALL_MODES, Gt(0));
  for (int i = 0; i < NUM_INSTALL_MODES; ++i) {
    const InstallConstants& mode = kInstallModes[i];

    // The modes must be listed in order.
    ASSERT_THAT(mode.index, Eq(i));

    // The first mode must have no install switch; the rest must have one.
    if (i == 0)
      ASSERT_THAT(mode.install_switch, StrEq(""));
    else
      ASSERT_THAT(mode.install_switch, StrNe(""));

    // The first mode must have no suffix; the rest must have one.
    if (i == 0)
      ASSERT_THAT(mode.install_suffix, StrEq(L""));
    else
      ASSERT_THAT(mode.install_suffix, StrNe(L""));

    // The first mode must have no logo suffix; the rest must have one.
    if (i == 0)
      ASSERT_THAT(mode.logo_suffix, StrEq(L""));
    else
      ASSERT_THAT(mode.logo_suffix, StrNe(L""));

    // The modes must have an appguid if Google Update integration is supported.
    if (kUseGoogleUpdateIntegration)
      ASSERT_THAT(mode.app_guid, StrNe(L""));
    else
      ASSERT_THAT(mode.app_guid, StrEq(L""));

    // UNSUPPORTED and kUseGoogleUpdateIntegration are mutually exclusive.
    if (kUseGoogleUpdateIntegration)
      ASSERT_THAT(mode.channel_strategy, Ne(ChannelStrategy::UNSUPPORTED));
    else
      ASSERT_THAT(mode.channel_strategy, Eq(ChannelStrategy::UNSUPPORTED));
  }
}

TEST(InstallModes, VerifyBrand) {
  if (kUseGoogleUpdateIntegration) {
    // Binaries were registered via an app guid with Google Update integration.
    ASSERT_THAT(kBinariesAppGuid, StrNe(L""));
    ASSERT_THAT(kBinariesPathName, StrEq(L""));
  } else {
    // Binaries were registered via a different path name without.
    ASSERT_THAT(kBinariesAppGuid, StrEq(L""));
    ASSERT_THAT(kBinariesPathName, StrNe(L""));
  }
}

TEST(InstallModes, GetClientsKeyPath) {
  constexpr wchar_t kAppGuid[] = L"test";

  if (kUseGoogleUpdateIntegration) {
    ASSERT_THAT(GetClientsKeyPath(kAppGuid),
                StrEq(L"Software\\Google\\Update\\Clients\\test"));
  } else {
    ASSERT_THAT(GetClientsKeyPath(kAppGuid),
                StrEq(std::wstring(L"Software\\").append(kProductPathName)));
  }
}

TEST(InstallModes, GetClientStateKeyPath) {
  constexpr wchar_t kAppGuid[] = L"test";

  if (kUseGoogleUpdateIntegration) {
    ASSERT_THAT(GetClientStateKeyPath(kAppGuid),
                StrEq(L"Software\\Google\\Update\\ClientState\\test"));
  } else {
    ASSERT_THAT(GetClientStateKeyPath(kAppGuid),
                StrEq(std::wstring(L"Software\\").append(kProductPathName)));
  }
}

TEST(InstallModes, GetBinariesClientsKeyPath) {
  if (kUseGoogleUpdateIntegration) {
    ASSERT_THAT(GetBinariesClientsKeyPath(),
                StrEq(std::wstring(L"Software\\Google\\Update\\Clients\\")
                          .append(kBinariesAppGuid)));
  } else {
    ASSERT_THAT(GetBinariesClientsKeyPath(),
                StrEq(std::wstring(L"Software\\").append(kBinariesPathName)));
  }
}

TEST(InstallModes, GetBinariesClientStateKeyPath) {
  if (kUseGoogleUpdateIntegration) {
    ASSERT_THAT(GetBinariesClientStateKeyPath(),
                StrEq(std::wstring(L"Software\\Google\\Update\\ClientState\\")
                          .append(kBinariesAppGuid)));
  } else {
    ASSERT_THAT(GetBinariesClientStateKeyPath(),
                StrEq(std::wstring(L"Software\\").append(kBinariesPathName)));
  }
}

TEST(InstallModes, GetClientStateMediumKeyPath) {
  constexpr wchar_t kAppGuid[] = L"test";

  if (kUseGoogleUpdateIntegration) {
    ASSERT_THAT(GetClientStateMediumKeyPath(kAppGuid),
                StrEq(L"Software\\Google\\Update\\ClientStateMedium\\test"));
  } else {
    ASSERT_THAT(GetClientStateMediumKeyPath(kAppGuid),
                StrEq(std::wstring(L"Software\\").append(kProductPathName)));
  }
}

TEST(InstallModes, GetBinariesClientStateMediumKeyPath) {
  if (kUseGoogleUpdateIntegration) {
    ASSERT_THAT(
        GetBinariesClientStateMediumKeyPath(),
        StrEq(std::wstring(L"Software\\Google\\Update\\ClientStateMedium\\")
                  .append(kBinariesAppGuid)));
  } else {
    ASSERT_THAT(GetBinariesClientStateMediumKeyPath(),
                StrEq(std::wstring(L"Software\\").append(kBinariesPathName)));
  }
}

}  // namespace install_static
