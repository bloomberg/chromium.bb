// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_icon.h"

#include "ash/test/ash_test_base.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/tether_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace network_icon {

class NetworkIconTest : public AshTestBase {
 public:
  NetworkIconTest() {}
  ~NetworkIconTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    chromeos::NetworkHandler::Initialize();

    tether_network =
        base::MakeUnique<chromeos::NetworkState>("tetherNetworkPath");
    tether_network->set_type(chromeos::kTypeTether);

    wifi_network = base::MakeUnique<chromeos::NetworkState>("wifiServicePath");
    wifi_network->set_type(shill::kTypeWifi);

    cellular_network =
        base::MakeUnique<chromeos::NetworkState>("cellularServicePath");
    cellular_network->set_type(shill::kTypeCellular);

    wifi_tether_network =
        base::MakeUnique<chromeos::NetworkState>("wifiTetherServicePath");
    wifi_tether_network->set_type(shill::kTypeWifi);
    wifi_tether_network.get()->set_tether_guid("tetherNetworkGuid");
  }

  void TearDown() override {
    PurgeNetworkIconCache();
    chromeos::NetworkHandler::Shutdown();
    AshTestBase::TearDown();
  }

  gfx::Image ImageForNetwork(chromeos::NetworkState* network) {
    gfx::ImageSkia image_skia = GetImageForNetwork(network, icon_type);
    return gfx::Image(image_skia);
  }

  // The icon for a Tether network should be the same as one for a cellular
  // network. The icon for a Tether network should be different from one for a
  // Wi-Fi network. The icon for a cellular network should be different from one
  // for a Wi-Fi network. The icon for a Tether network should be the same as
  // one for a Wi-Fi network with an associated Tether guid.
  void GetAndCompareImagesByNetworkType() {
    gfx::Image tether_image = ImageForNetwork(tether_network.get());
    gfx::Image wifi_image = ImageForNetwork(wifi_network.get());
    gfx::Image cellular_image = ImageForNetwork(cellular_network.get());
    gfx::Image wifi_tether_image = ImageForNetwork(wifi_tether_network.get());

    EXPECT_FALSE(gfx::test::AreImagesEqual(tether_image, wifi_image));
    EXPECT_FALSE(gfx::test::AreImagesEqual(cellular_image, wifi_image));
    EXPECT_TRUE(gfx::test::AreImagesEqual(tether_image, cellular_image));

    EXPECT_TRUE(gfx::test::AreImagesEqual(tether_image, wifi_tether_image));
  }

  IconType icon_type = ICON_TYPE_TRAY;

  std::unique_ptr<chromeos::NetworkState> tether_network;
  std::unique_ptr<chromeos::NetworkState> wifi_network;
  std::unique_ptr<chromeos::NetworkState> cellular_network;
  // A network whose type is shill::kTypeWifi, but which is associated with
  // a Tether network via its Tether network ID.
  std::unique_ptr<chromeos::NetworkState> wifi_tether_network;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkIconTest);
};

// This tests that the correct icons are being generated for the correct
// networks by pairwise comparison of three different network types, verifying
// that the Tether and cellular icon are the same, Tether and Wi-Fi icons are
// different, and cellular and Wi-Fi icons are different. Additionaly, it
// verifies that the Tether network and Wi-Fi network with associated Tether
// guid are treated the same for purposes of icon display
TEST_F(NetworkIconTest, CompareImagesByNetworkType_NotVisible) {
  GetAndCompareImagesByNetworkType();
}

TEST_F(NetworkIconTest, CompareImagesByNetworkType_Connecting) {
  tether_network->set_visible(true);
  tether_network->set_connection_state(shill::kStateAssociation);

  wifi_network->set_visible(true);
  wifi_network->set_connection_state(shill::kStateAssociation);

  cellular_network->set_visible(true);
  cellular_network->set_connection_state(shill::kStateAssociation);

  wifi_tether_network->set_visible(true);
  wifi_tether_network->set_connection_state(shill::kStateAssociation);

  GetAndCompareImagesByNetworkType();
}

TEST_F(NetworkIconTest, CompareImagesByNetworkType_Connected) {
  tether_network->set_visible(true);
  tether_network->set_connection_state(shill::kStateOnline);

  wifi_network->set_visible(true);
  wifi_network->set_connection_state(shill::kStateOnline);

  cellular_network->set_visible(true);
  cellular_network->set_connection_state(shill::kStateOnline);

  wifi_tether_network->set_visible(true);
  wifi_tether_network->set_connection_state(shill::kStateOnline);

  GetAndCompareImagesByNetworkType();
}

}  // namespace network_icon

}  // namespace ash
