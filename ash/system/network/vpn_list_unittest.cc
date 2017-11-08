// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_list.h"

#include <algorithm>
#include <vector>

#include "ash/public/interfaces/vpn_list.mojom.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::mojom::ThirdPartyVpnProvider;
using ash::mojom::ThirdPartyVpnProviderPtr;

namespace ash {

namespace {

class TestVpnListObserver : public VpnList::Observer {
 public:
  TestVpnListObserver() {}
  ~TestVpnListObserver() override {}

  // VpnList::Observer:
  void OnVPNProvidersChanged() override { change_count_++; }

  int change_count_ = 0;
};

}  // namespace

using VpnListTest = testing::Test;

TEST_F(VpnListTest, BuiltInProvider) {
  VpnList vpn_list;

  // The VPN list should only contain the built-in provider.
  ASSERT_EQ(1u, vpn_list.extension_vpn_providers().size());
  VPNProvider provider = vpn_list.extension_vpn_providers()[0];
  EXPECT_EQ(provider.provider_type, VPNProvider::BUILT_IN_VPN);
  EXPECT_TRUE(provider.app_id.empty());
}

TEST_F(VpnListTest, ThirdPartyProviders) {
  VpnList vpn_list;

  // The VPN list should only contain the built-in provider.
  EXPECT_EQ(1u, vpn_list.extension_vpn_providers().size());

  // Add some third party (extension-backed) providers.
  std::vector<ThirdPartyVpnProviderPtr> third_party_providers;
  ThirdPartyVpnProviderPtr third_party1 = ThirdPartyVpnProvider::New();
  third_party1->name = "name1";
  third_party1->extension_id = "extension_id1";
  third_party_providers.push_back(std::move(third_party1));

  ThirdPartyVpnProviderPtr third_party2 = ThirdPartyVpnProvider::New();
  third_party2->name = "name2";
  third_party2->extension_id = "extension_id2";
  third_party_providers.push_back(std::move(third_party2));

  vpn_list.SetThirdPartyVpnProviders(std::move(third_party_providers));

  // Mojo types will be converted to internal ash types.
  VPNProvider extension_provider1 =
      VPNProvider::CreateThirdPartyVPNProvider("extension_id1", "name1");
  VPNProvider extension_provider2 =
      VPNProvider::CreateThirdPartyVPNProvider("extension_id2", "name2");

  // List contains the extension-backed providers. Order doesn't matter.
  std::vector<VPNProvider> extension_providers =
      vpn_list.extension_vpn_providers();
  EXPECT_EQ(3u, extension_providers.size());
  EXPECT_EQ(1u, std::count(extension_providers.begin(),
                           extension_providers.end(), extension_provider1));
  EXPECT_EQ(1u, std::count(extension_providers.begin(),
                           extension_providers.end(), extension_provider2));
}

TEST_F(VpnListTest, Observers) {
  VpnList vpn_list;

  // Observers are not notified when they are added.
  TestVpnListObserver observer;
  vpn_list.AddObserver(&observer);
  EXPECT_EQ(0, observer.change_count_);

  // Add a third party (extension-backed) provider.
  std::vector<ThirdPartyVpnProviderPtr> third_party_providers;
  ThirdPartyVpnProviderPtr third_party1 = ThirdPartyVpnProvider::New();
  third_party1->name = "name1";
  third_party1->extension_id = "extension_id1";
  third_party_providers.push_back(std::move(third_party1));
  vpn_list.SetThirdPartyVpnProviders(std::move(third_party_providers));

  // Observer was notified.
  EXPECT_EQ(1, observer.change_count_);

  vpn_list.RemoveObserver(&observer);
}

}  // namespace ash
