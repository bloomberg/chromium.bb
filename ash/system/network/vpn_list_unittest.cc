// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/vpn_list.h"

#include <algorithm>
#include <vector>

#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::network_config::mojom::VpnProvider;
using chromeos::network_config::mojom::VpnProviderPtr;
using chromeos::network_config::mojom::VpnType;

namespace ash {

namespace {

class TestVpnListObserver : public VpnList::Observer {
 public:
  TestVpnListObserver() = default;
  ~TestVpnListObserver() override = default;

  // VpnList::Observer:
  void OnVPNProvidersChanged() override { change_count_++; }

  int change_count_ = 0;
};

std::vector<VpnProviderPtr> CopyProviders(
    const std::vector<VpnProviderPtr>& providers) {
  std::vector<VpnProviderPtr> result;
  for (const VpnProviderPtr& provider : providers)
    result.push_back(provider.Clone());
  return result;
}

}  // namespace

using VpnListTest = AshTestBase;

TEST_F(VpnListTest, BuiltInProvider) {
  VpnList vpn_list;

  // The VPN list should only contain the built-in provider.
  ASSERT_EQ(1u, vpn_list.extension_vpn_providers().size());
  VPNProvider provider = vpn_list.extension_vpn_providers()[0];
  EXPECT_EQ(provider.provider_type, VPNProvider::BUILT_IN_VPN);
  EXPECT_TRUE(provider.app_id.empty());
}

TEST_F(VpnListTest, ExtensionProviders) {
  VpnList vpn_list;

  // The VPN list should only contain the built-in provider.
  EXPECT_EQ(1u, vpn_list.extension_vpn_providers().size());

  // Add some third party (extension-backed) providers.
  std::vector<VpnProviderPtr> third_party_providers;
  VpnProviderPtr third_party1 = VpnProvider::New();
  third_party1->type = VpnType::kExtension;
  third_party1->provider_name = "name1";
  third_party1->provider_id = "extension_id1";
  third_party_providers.push_back(std::move(third_party1));

  VpnProviderPtr third_party2 = VpnProvider::New();
  third_party2->type = VpnType::kExtension;
  third_party2->provider_name = "name2";
  third_party2->provider_id = "extension_id2";
  third_party_providers.push_back(std::move(third_party2));

  vpn_list.SetVpnProvidersForTest(std::move(third_party_providers));

  // Mojo types will be converted to internal ash types.
  VPNProvider extension_provider1 =
      VPNProvider::CreateExtensionVPNProvider("extension_id1", "name1");
  VPNProvider extension_provider2 =
      VPNProvider::CreateExtensionVPNProvider("extension_id2", "name2");

  // List contains the extension-backed providers. Order doesn't matter.
  std::vector<VPNProvider> extension_providers =
      vpn_list.extension_vpn_providers();
  EXPECT_EQ(3u, extension_providers.size());
  EXPECT_EQ(1u, std::count(extension_providers.begin(),
                           extension_providers.end(), extension_provider1));
  EXPECT_EQ(1u, std::count(extension_providers.begin(),
                           extension_providers.end(), extension_provider2));
}

TEST_F(VpnListTest, ArcProviders) {
  VpnList vpn_list;

  // Initial refresh.
  base::Time launchTime1 = base::Time::Now();
  std::vector<VpnProviderPtr> arc_vpn_providers;
  VpnProviderPtr arc_vpn_provider1 = VpnProvider::New();
  arc_vpn_provider1->type = VpnType::kArc;
  arc_vpn_provider1->provider_id = "package.name.foo1";
  arc_vpn_provider1->provider_name = "ArcVPNMonster1";
  arc_vpn_provider1->app_id = "arc_app_id1";
  arc_vpn_provider1->last_launch_time = launchTime1;
  arc_vpn_providers.push_back(std::move(arc_vpn_provider1));

  vpn_list.SetVpnProvidersForTest(CopyProviders(arc_vpn_providers));

  VPNProvider arc_provider1 = VPNProvider::CreateArcVPNProvider(
      "package.name.foo1", "ArcVPNMonster1", "arc_app_id1", launchTime1);

  std::vector<VPNProvider> arc_providers = vpn_list.arc_vpn_providers();
  EXPECT_EQ(1u, arc_providers.size());
  EXPECT_EQ(1u, std::count(arc_providers.begin(), arc_providers.end(),
                           arc_provider1));
  EXPECT_EQ(launchTime1, arc_providers[0].last_launch_time);

  // package.name.foo2 gets installed.
  VpnProviderPtr arc_vpn_provider2 = VpnProvider::New();
  arc_vpn_provider2->type = VpnType::kArc;
  arc_vpn_provider2->provider_id = "package.name.foo2";
  arc_vpn_provider2->provider_name = "ArcVPNMonster2";
  arc_vpn_provider2->app_id = "arc_app_id2";
  arc_vpn_provider2->last_launch_time = base::Time::Now();
  arc_vpn_providers.push_back(std::move(arc_vpn_provider2));

  vpn_list.SetVpnProvidersForTest(CopyProviders(arc_vpn_providers));

  VPNProvider arc_provider2 = VPNProvider::CreateArcVPNProvider(
      "package.name.foo2", "ArcVPNMonster2", "arc_app_id2", base::Time::Now());
  arc_providers = vpn_list.arc_vpn_providers();
  EXPECT_EQ(2u, arc_providers.size());
  EXPECT_EQ(1u, std::count(arc_providers.begin(), arc_providers.end(),
                           arc_provider1));
  EXPECT_EQ(1u, std::count(arc_providers.begin(), arc_providers.end(),
                           arc_provider2));

  // package.name.foo1 gets uninstalled.
  arc_vpn_providers.erase(arc_vpn_providers.begin());
  vpn_list.SetVpnProvidersForTest(CopyProviders(arc_vpn_providers));

  arc_providers = vpn_list.arc_vpn_providers();
  EXPECT_EQ(1u, arc_providers.size());
  EXPECT_EQ(1u, std::count(arc_providers.begin(), arc_providers.end(),
                           arc_provider2));

  // package.name.foo2 changes due to update or system language change.
  base::Time launchTime2 = base::Time::Now();
  VpnProviderPtr arc_vpn_provider2_rename = VpnProvider::New();
  arc_vpn_provider2_rename->type = VpnType::kArc;
  arc_vpn_provider2_rename->provider_id = "package.name.foo2";
  arc_vpn_provider2_rename->provider_name = "ArcVPNMonster2Rename";
  arc_vpn_provider2_rename->app_id = "arc_app_id2_rename";
  arc_vpn_provider2_rename->last_launch_time = launchTime2;
  arc_vpn_providers[0] = std::move(arc_vpn_provider2_rename);
  vpn_list.SetVpnProvidersForTest(CopyProviders(arc_vpn_providers));

  arc_provider2.provider_name = "ArcVPNMonster2Rename";
  arc_provider2.app_id = "arc_app_id2_rename";

  arc_providers = vpn_list.arc_vpn_providers();
  EXPECT_EQ(1u, arc_providers.size());
  EXPECT_EQ(1u, std::count(arc_providers.begin(), arc_providers.end(),
                           arc_provider2));
  EXPECT_EQ(launchTime2, arc_providers[0].last_launch_time);
}

TEST_F(VpnListTest, Observers) {
  VpnList vpn_list;

  // Observers are not notified when they are added.
  TestVpnListObserver observer;
  vpn_list.AddObserver(&observer);
  EXPECT_EQ(0, observer.change_count_);

  // Add a third party (extension-backed) provider.
  std::vector<VpnProviderPtr> third_party_providers;
  VpnProviderPtr third_party1 = VpnProvider::New();
  third_party1->type = VpnType::kExtension;
  third_party1->provider_name = "name1";
  third_party1->provider_id = "extension_id1";
  third_party_providers.push_back(std::move(third_party1));
  vpn_list.SetVpnProvidersForTest(std::move(third_party_providers));

  // Observer was notified.
  EXPECT_EQ(1, observer.change_count_);

  vpn_list.RemoveObserver(&observer);
}

}  // namespace ash
