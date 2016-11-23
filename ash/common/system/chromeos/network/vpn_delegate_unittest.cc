// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/vpn_delegate.h"

#include <algorithm>
#include <vector>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class TestVpnDelegate : public VPNDelegate {
 public:
  TestVpnDelegate() {}
  ~TestVpnDelegate() override {}

  // VPNDelegate:
  void ShowAddPage(const std::string& extension_id) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestVpnDelegate);
};

}  // namespace

using VpnDelegateTest = testing::Test;

TEST_F(VpnDelegateTest, BuiltInProvider) {
  TestVpnDelegate delegate;

  // The VPN list should only contain the built-in provider.
  ASSERT_EQ(1u, delegate.vpn_providers().size());
  VPNProvider provider = delegate.vpn_providers()[0];
  EXPECT_FALSE(provider.third_party);
  EXPECT_TRUE(provider.extension_id.empty());
}

TEST_F(VpnDelegateTest, ThirdPartyProviders) {
  TestVpnDelegate delegate;

  // The VPN list should only contain the built-in provider.
  EXPECT_EQ(1u, delegate.vpn_providers().size());

  // Add some third party (extension-backed) providers.
  VPNProvider provider1("extension_id1", "name1");
  VPNProvider provider2("extension_id2", "name2");
  std::vector<VPNProvider> third_party_providers = {provider1, provider2};
  delegate.SetThirdPartyVpnProviders(third_party_providers);

  // List contains the extension-backed providers. Order doesn't matter.
  std::vector<VPNProvider> providers = delegate.vpn_providers();
  EXPECT_EQ(3u, providers.size());
  EXPECT_EQ(1u, std::count(providers.begin(), providers.end(), provider1));
  EXPECT_EQ(1u, std::count(providers.begin(), providers.end(), provider2));
}

}  // namespace ash
