// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_icon.h"

#include <memory>
#include <set>

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/active_network_icon.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/network/tether_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"

// This tests both the helper functions in network_icon, and ActiveNetworkIcon
// which is a primary consumer of the helper functions.

namespace ash {

namespace network_icon {

class NetworkIconTest : public testing::Test {
 public:
  NetworkIconTest() = default;
  ~NetworkIconTest() override = default;

  void SetUp() override {
    SetUpDefaultNetworkState();
    active_network_icon_ = std::make_unique<ActiveNetworkIcon>();
    active_network_icon_->InitForTesting(helper().network_state_handler());
  }

  void TearDown() override {
    active_network_icon_.reset();
    PurgeNetworkIconCache(std::set<std::string>());
  }

  std::string ConfigureService(const std::string& shill_json_string) {
    return helper_.ConfigureService(shill_json_string);
  }

  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value) {
    helper_.SetServiceProperty(service_path, key, value);
  }

  void SetUpDefaultNetworkState() {
    // NetworkStateTestHelper default has a wifi device only and no services.

    helper_.device_test()->AddDevice("/device/stub_cellular_device",
                                     shill::kTypeCellular,
                                     "stub_cellular_device");
    base::RunLoop().RunUntilIdle();

    wifi1_path_ = ConfigureService(
        R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "idle"})");
    ASSERT_FALSE(wifi1_path_.empty());
    wifi2_path_ = ConfigureService(
        R"({"GUID": "wifi2_guid", "Type": "wifi", "State": "idle"})");
    ASSERT_FALSE(wifi2_path_.empty());
    cellular_path_ = ConfigureService(
        R"({"GUID": "cellular_guid", "Type": "cellular", "Technology": "LTE",
            "State": "idle"})");
    ASSERT_FALSE(cellular_path_.empty());
  }

  std::unique_ptr<chromeos::NetworkState> CreateStandaloneNetworkState(
      const std::string& id,
      const std::string& type,
      const std::string& connection_state,
      int signal_strength) {
    return helper_.CreateStandaloneNetworkState(id, type, connection_state,
                                                signal_strength);
  }

  std::unique_ptr<chromeos::NetworkState>
  CreateStandaloneWifiTetherNetworkState(const std::string& id,
                                         const std::string& tether_guid,
                                         const std::string& connection_state,
                                         int signal_strength) {
    std::unique_ptr<chromeos::NetworkState> network =
        CreateStandaloneNetworkState(id, shill::kTypeWifi, connection_state,
                                     signal_strength);
    network->set_tether_guid(tether_guid);
    return network;
  }

  gfx::Image ImageForNetwork(const chromeos::NetworkState* network) {
    gfx::ImageSkia image_skia = GetImageForNonVirtualNetwork(
        network, icon_type_, false /* show_vpn_badge */);
    return gfx::Image(image_skia);
  }

  void GetDefaultNetworkImageAndLabel(IconType icon_type,
                                      gfx::ImageSkia* image,
                                      base::string16* label,
                                      bool* animating) {
    *image = active_network_icon_->GetSingleImage(icon_type, animating);
    *label = active_network_icon_->GetDefaultLabel(icon_type);
  }

  // The icon for a Tether network should be the same as one for a cellular
  // network. The icon for a Tether network should be different from one for a
  // Wi-Fi network. The icon for a cellular network should be different from one
  // for a Wi-Fi network. The icon for a Tether network should be the same as
  // one for a Wi-Fi network with an associated Tether guid.
  void GetAndCompareImagesByNetworkType(
      const chromeos::NetworkState* wifi_network,
      const chromeos::NetworkState* cellular_network,
      const chromeos::NetworkState* tether_network,
      const chromeos::NetworkState* wifi_tether_network) {
    ASSERT_EQ(wifi_network->type(), shill::kTypeWifi);
    gfx::Image wifi_image = ImageForNetwork(wifi_network);

    ASSERT_EQ(cellular_network->type(), shill::kTypeCellular);
    gfx::Image cellular_image = ImageForNetwork(cellular_network);

    ASSERT_EQ(tether_network->type(), chromeos::kTypeTether);
    gfx::Image tether_image = ImageForNetwork(tether_network);

    ASSERT_EQ(wifi_tether_network->type(), shill::kTypeWifi);
    ASSERT_FALSE(wifi_tether_network->tether_guid().empty());
    gfx::Image wifi_tether_image = ImageForNetwork(wifi_tether_network);

    EXPECT_FALSE(gfx::test::AreImagesEqual(tether_image, wifi_image));
    EXPECT_FALSE(gfx::test::AreImagesEqual(cellular_image, wifi_image));
    EXPECT_TRUE(gfx::test::AreImagesEqual(tether_image, cellular_image));

    EXPECT_TRUE(gfx::test::AreImagesEqual(tether_image, wifi_tether_image));
  }

  void SetCellularUnavailable() {
    helper_.manager_test()->RemoveTechnology(shill::kTypeCellular);

    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(
        chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
        helper_.network_state_handler()->GetTechnologyState(
            chromeos::NetworkTypePattern::Cellular()));
  }

  chromeos::NetworkStateTestHelper& helper() { return helper_; }

  const std::string& wifi1_path() const { return wifi1_path_; }
  const std::string& wifi2_path() const { return wifi2_path_; }
  const std::string& cellular_path() const { return cellular_path_; }

  IconType icon_type_ = ICON_TYPE_TRAY_REGULAR;

 private:
  const base::MessageLoop message_loop_;

  chromeos::NetworkStateTestHelper helper_{
      false /* use_default_devices_and_services */};

  // Preconfigured service paths:
  std::string wifi1_path_;
  std::string wifi2_path_;
  std::string cellular_path_;

  std::unique_ptr<ActiveNetworkIcon> active_network_icon_;

  DISALLOW_COPY_AND_ASSIGN(NetworkIconTest);
};

// This tests that the correct icons are being generated for the correct
// networks by pairwise comparison of three different network types, verifying
// that the Tether and cellular icon are the same, Tether and Wi-Fi icons are
// different, and cellular and Wi-Fi icons are different. Additionally, it
// verifies that the Tether network and Wi-Fi network with associated Tether
// guid are treated the same for purposes of icon display
TEST_F(NetworkIconTest, CompareImagesByNetworkType_NotVisible) {
  std::unique_ptr<chromeos::NetworkState> wifi_network =
      CreateStandaloneNetworkState("wifi", shill::kTypeWifi, shill::kStateIdle,
                                   50);

  std::unique_ptr<chromeos::NetworkState> cellular_network =
      CreateStandaloneNetworkState("cellular", shill::kTypeCellular,
                                   shill::kStateIdle, 50);

  std::unique_ptr<chromeos::NetworkState> wimax_network =
      CreateStandaloneNetworkState("wimax", shill::kTypeWimax,
                                   shill::kStateIdle, 50);

  EXPECT_TRUE(gfx::test::AreImagesEqual(ImageForNetwork(cellular_network.get()),
                                        ImageForNetwork(wimax_network.get())));

  std::unique_ptr<chromeos::NetworkState> tether_network =
      CreateStandaloneNetworkState("tether", chromeos::kTypeTether,
                                   shill::kStateIdle, 50);

  std::unique_ptr<chromeos::NetworkState> wifi_tether_network =
      CreateStandaloneWifiTetherNetworkState("wifi_tether", "tether",
                                             shill::kStateIdle, 50);

  GetAndCompareImagesByNetworkType(wifi_network.get(), cellular_network.get(),
                                   tether_network.get(),
                                   wifi_tether_network.get());
}

TEST_F(NetworkIconTest, CompareImagesByNetworkType_Connecting) {
  std::unique_ptr<chromeos::NetworkState> wifi_network =
      CreateStandaloneNetworkState("wifi", shill::kTypeWifi,
                                   shill::kStateAssociation, 50);

  std::unique_ptr<chromeos::NetworkState> cellular_network =
      CreateStandaloneNetworkState("cellular", shill::kTypeCellular,
                                   shill::kStateAssociation, 50);

  std::unique_ptr<chromeos::NetworkState> tether_network =
      CreateStandaloneNetworkState("tether", chromeos::kTypeTether,
                                   shill::kStateAssociation, 50);

  std::unique_ptr<chromeos::NetworkState> wifi_tether_network =
      CreateStandaloneWifiTetherNetworkState("wifi_tether", "tether",
                                             shill::kStateAssociation, 50);

  GetAndCompareImagesByNetworkType(wifi_network.get(), cellular_network.get(),
                                   tether_network.get(),
                                   wifi_tether_network.get());
}

TEST_F(NetworkIconTest, CompareImagesByNetworkType_Connected) {
  std::unique_ptr<chromeos::NetworkState> wifi_network =
      CreateStandaloneNetworkState("wifi", shill::kTypeWifi,
                                   shill::kStateOnline, 50);

  std::unique_ptr<chromeos::NetworkState> cellular_network =
      CreateStandaloneNetworkState("cellular", shill::kTypeCellular,
                                   shill::kStateOnline, 50);

  std::unique_ptr<chromeos::NetworkState> tether_network =
      CreateStandaloneNetworkState("tether", chromeos::kTypeTether,
                                   shill::kStateOnline, 50);

  std::unique_ptr<chromeos::NetworkState> wifi_tether_network =
      CreateStandaloneWifiTetherNetworkState("wifi_tether", "tether",
                                             shill::kStateOnline, 50);

  GetAndCompareImagesByNetworkType(wifi_network.get(), cellular_network.get(),
                                   tether_network.get(),
                                   wifi_tether_network.get());
}

TEST_F(NetworkIconTest, NetworkSignalStrength) {
  using ss = SignalStrength;

  std::unique_ptr<chromeos::NetworkState> ethernet_network =
      CreateStandaloneNetworkState("eth", shill::kTypeEthernet,
                                   shill::kStateOnline, 50);

  std::unique_ptr<chromeos::NetworkState> wifi_network =
      CreateStandaloneNetworkState("wifi", shill::kTypeWifi,
                                   shill::kStateOnline, 50);

  // Verify non-wireless network types return SignalStrength::NOT_WIRELESS, and
  // wireless network types return something other than
  // SignalStrength::NOT_WIRELESS.
  EXPECT_EQ(ss::NOT_WIRELESS,
            GetSignalStrengthForNetwork(ethernet_network.get()));
  EXPECT_NE(ss::NOT_WIRELESS, GetSignalStrengthForNetwork(wifi_network.get()));

  // Signal strength is divided into four categories: none, weak, medium and
  // strong. They are meant to match the number of sections in the wifi icon.
  // The wifi icon currently has four levels; signals [0, 100] are mapped to [1,
  // 4]. There are only three signal strengths so icons that were mapped to 2
  // are also considered weak.
  wifi_network->set_signal_strength(0);
  EXPECT_EQ(ss::NONE, GetSignalStrengthForNetwork(wifi_network.get()));
  wifi_network->set_signal_strength(50);
  EXPECT_EQ(ss::WEAK, GetSignalStrengthForNetwork(wifi_network.get()));
  wifi_network->set_signal_strength(51);
  EXPECT_EQ(ss::MEDIUM, GetSignalStrengthForNetwork(wifi_network.get()));
  wifi_network->set_signal_strength(75);
  EXPECT_EQ(ss::MEDIUM, GetSignalStrengthForNetwork(wifi_network.get()));
  wifi_network->set_signal_strength(76);
  EXPECT_EQ(ss::STRONG, GetSignalStrengthForNetwork(wifi_network.get()));
  wifi_network->set_signal_strength(100);
  EXPECT_EQ(ss::STRONG, GetSignalStrengthForNetwork(wifi_network.get()));
}

TEST_F(NetworkIconTest, DefaultImageAndLabelWifiConnected) {
  // Set the Wifi service as connected.
  SetServiceProperty(wifi1_path(), shill::kSignalStrengthProperty,
                     base::Value(45));
  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  gfx::ImageSkia default_image;
  base::string16 label;
  bool animating = false;
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState("reference", shill::kTypeWifi,
                                   shill::kStateOnline, 45);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network.get())));
}

TEST_F(NetworkIconTest, DefaultImageAndLabelWifiConnecting) {
  // Set the Wifi service as connected.
  SetServiceProperty(wifi1_path(), shill::kSignalStrengthProperty,
                     base::Value(45));
  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateAssociation));

  gfx::ImageSkia default_image;
  base::string16 label;
  bool animating = false;
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_TRUE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState("reference", shill::kTypeWifi,
                                   shill::kStateAssociation, 45);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network.get())));
}

// Tests that the default network image is a cellular network icon when cellular
// network is the default network, even if a wifi network is connected.
// Generally, shill will prefer wifi over cellular networks when both are
// connected, but that is not always the case. For example, if the connected
// wifi service has no Internet connectivity, cellular service will be selected
// as default.
TEST_F(NetworkIconTest, DefaultImageAndLabelCellularDefaultWithWifiConnected) {
  // Set both wifi and cellular networks in a connected state, but with wifi not
  // online - this should prompt fake shill manager implementation to prefer
  // cellular network over wifi.
  SetServiceProperty(wifi1_path(), shill::kSignalStrengthProperty,
                     base::Value(45));
  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateReady));

  SetServiceProperty(cellular_path(), shill::kSignalStrengthProperty,
                     base::Value(65));
  SetServiceProperty(cellular_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  gfx::ImageSkia default_image;
  base::string16 label;
  bool animating = false;
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState("reference", shill::kTypeCellular,
                                   shill::kStateOnline, 65);

  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network.get())));
}

// Tests the use case where the default network starts reconnecting while
// another network is connected.
TEST_F(NetworkIconTest, DefaultImageReconnectingWifiWithCellularConnected) {
  // First connect both wifi and cellular network (with wifi as default).
  SetServiceProperty(wifi1_path(), shill::kSignalStrengthProperty,
                     base::Value(45));
  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  SetServiceProperty(cellular_path(), shill::kSignalStrengthProperty,
                     base::Value(65));
  SetServiceProperty(cellular_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  // Start reconnecting wifi network.
  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateAssociation));

  gfx::ImageSkia default_image;
  base::string16 label;
  bool animating = false;
  // Verify that the default network is connecting icon for the initial default
  // network (even though the default network as reported by shill actually
  // changed).
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_TRUE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_network_1 =
      CreateStandaloneNetworkState("reference1", shill::kTypeWifi,
                                   shill::kStateAssociation, 45);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network_1.get())));

  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateReady));

  // At this point the wifi network is not connecting, but is not yet default -
  // the default network icon should have image for the cellular network (which
  // is the default one at this point).
  // TODO(tbarzic): This creates a unoptimal behavior in the UI where cellular
  //     icon could flash between wifi connecting and connected icons - this
  //     should be fixed, for example by not showing connecting icon when
  //     reconnecting a network if another network is connected.
  std::unique_ptr<chromeos::NetworkState> reference_network_2 =
      CreateStandaloneNetworkState("reference2", shill::kTypeCellular,
                                   shill::kStateOnline, 65);
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);

  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network_2.get())));

  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  // The wifi network is online, and thus default - the default network icon
  // should display the wifi network's associated image again.
  std::unique_ptr<chromeos::NetworkState> reference_network_3 =
      CreateStandaloneNetworkState("reference3", shill::kTypeWifi,
                                   shill::kStateOnline, 45);
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);

  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network_3.get())));
}

TEST_F(NetworkIconTest, DefaultImageDisconnectWifiWithCellularConnected) {
  // First connect both wifi and cellular network (with wifi as default).
  SetServiceProperty(wifi1_path(), shill::kSignalStrengthProperty,
                     base::Value(45));
  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  SetServiceProperty(cellular_path(), shill::kSignalStrengthProperty,
                     base::Value(65));
  SetServiceProperty(cellular_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  // Disconnect wifi network, and verify the default icon changes to cellular.
  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateIdle));

  gfx::ImageSkia default_image;
  base::string16 label;
  bool animating = false;
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState("reference", shill::kTypeCellular,
                                   shill::kStateOnline, 65);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network.get())));
}

// Tests that the default network image remains the same if non-default network
// reconnects.
TEST_F(NetworkIconTest, DefaultImageWhileNonDefaultNetworkReconnecting) {
  // First connect both wifi and cellular network (with wifi as default).
  SetServiceProperty(wifi1_path(), shill::kSignalStrengthProperty,
                     base::Value(45));
  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  SetServiceProperty(cellular_path(), shill::kSignalStrengthProperty,
                     base::Value(65));
  SetServiceProperty(cellular_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  // Start reconnecting the non-default, cellular network.
  SetServiceProperty(cellular_path(), shill::kStateProperty,
                     base::Value(shill::kStateAssociation));

  gfx::ImageSkia default_image;
  base::string16 label;
  bool animating = false;
  // Currently, a connecting icon is used as default network icon even if
  // another network connected and used as default.
  // TODO(tbarzic): Consider changing network icon logic to use a connected
  //     network icon if a network is connected while a network is reconnecting.
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_TRUE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_network_1 =
      CreateStandaloneNetworkState("reference1", shill::kTypeCellular,
                                   shill::kStateAssociation, 65);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network_1.get())));

  // Move the cellular network to connected, but not yet online state - the
  // default network image changes back to the default network.
  SetServiceProperty(cellular_path(), shill::kStateProperty,
                     base::Value(shill::kStateReady));

  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);
  std::unique_ptr<chromeos::NetworkState> reference_network_2 =
      CreateStandaloneNetworkState("reference2", shill::kTypeWifi,
                                   shill::kStateOnline, 45);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network_2.get())));

  // Move the cellular network to online state - the default network image
  // should remain the same.
  SetServiceProperty(cellular_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network_2.get())));
}

// Tests that the default network image shows a cellular network icon if
// cellular network is connected while wifi is connecting.
TEST_F(NetworkIconTest, DefaultImageConnectingToWifiWhileCellularConnected) {
  // Connect cellular network, and set the wifi as connecting.
  SetServiceProperty(wifi1_path(), shill::kSignalStrengthProperty,
                     base::Value(45));
  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateAssociation));

  SetServiceProperty(cellular_path(), shill::kSignalStrengthProperty,
                     base::Value(65));
  SetServiceProperty(cellular_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  gfx::ImageSkia default_image;
  base::string16 label;
  bool animating = false;
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState("reference", shill::kTypeCellular,
                                   shill::kStateOnline, 65);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network.get())));
}

// Test that a connecting cellular icon is displayed when activating a cellular
// network (if other networks are not connected).
TEST_F(NetworkIconTest, DefaultNetworkImageActivatingCellularNetwork) {
  SetServiceProperty(cellular_path(), shill::kSignalStrengthProperty,
                     base::Value(65));
  SetServiceProperty(cellular_path(), shill::kActivationStateProperty,
                     base::Value(shill::kActivationStateActivating));

  gfx::ImageSkia default_image;
  base::string16 label;
  bool animating = false;
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState("reference", shill::kTypeCellular,
                                   shill::kStateIdle, 65);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network.get())));
}

// Tests that default network image is a wifi network image if wifi network is
// connected during cellular network activation.
TEST_F(NetworkIconTest,
       DefaultNetworkImageActivatingCellularNetworkWithConnectedWifi) {
  SetServiceProperty(wifi1_path(), shill::kSignalStrengthProperty,
                     base::Value(45));
  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));

  SetServiceProperty(cellular_path(), shill::kSignalStrengthProperty,
                     base::Value(65));
  SetServiceProperty(cellular_path(), shill::kActivationStateProperty,
                     base::Value(shill::kActivationStateActivating));

  gfx::ImageSkia default_image;
  base::string16 label;
  bool animating = false;
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState("reference", shill::kTypeWifi,
                                   shill::kStateIdle, 45);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network.get())));
}

// Tests VPN badging for the default network.
TEST_F(NetworkIconTest, DefaultNetworkVpnBadge) {
  gfx::ImageSkia default_image;
  base::string16 label;
  bool animating = false;

  // Set up initial state with Ethernet and WiFi connected.
  std::string ethernet_path = ConfigureService(
      R"({"GUID": "ethernet_guid", "Type": "ethernet", "State": "online"})");
  ASSERT_FALSE(ethernet_path.empty());
  // Wifi1 is set up by default but not connected. Also set its strength.
  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateOnline));
  SetServiceProperty(wifi1_path(), shill::kSignalStrengthProperty,
                     base::Value(45));

  // With Ethernet and WiFi connected, the default icon should be empty.
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_TRUE(default_image.isNull());
  EXPECT_FALSE(animating);

  // Add a connected VPN.
  std::string vpn_path = ConfigureService(
      R"({"GUID": "vpn_guid", "Type": "vpn", "State": "online"})");
  ASSERT_FALSE(vpn_path.empty());

  // When a VPN is connected, the default icon should be Ethernet with a badge.
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_eth =
      CreateStandaloneNetworkState("reference_eth", shill::kTypeEthernet,
                                   shill::kStateOnline, 0);
  gfx::Image reference_eth_unbadged = gfx::Image(GetImageForNonVirtualNetwork(
      reference_eth.get(), icon_type_, false /* show_vpn_badge */));
  gfx::Image reference_eth_badged = gfx::Image(GetImageForNonVirtualNetwork(
      reference_eth.get(), icon_type_, true /* show_vpn_badge */));

  EXPECT_FALSE(gfx::test::AreImagesEqual(gfx::Image(default_image),
                                         reference_eth_unbadged));
  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(default_image),
                                        reference_eth_badged));

  // Disconnect Ethernet. The default icon should become WiFi with a badge.
  SetServiceProperty(ethernet_path, shill::kStateProperty,
                     base::Value(shill::kStateIdle));
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_wifi =
      CreateStandaloneNetworkState("reference_wifi", shill::kTypeWifi,
                                   shill::kStateOnline, 45);
  gfx::Image reference_wifi_badged = gfx::Image(GetImageForNonVirtualNetwork(
      reference_wifi.get(), icon_type_, true /* show_vpn_badge */));
  EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(default_image),
                                        reference_wifi_badged));

  // Set the VPN to connecting; the default icon should be animating.
  SetServiceProperty(vpn_path, shill::kStateProperty,
                     base::Value(shill::kStateAssociation));
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_TRUE(animating);
}

// Tests that wifi image is shown when connecting to wifi network with vpn.
TEST_F(NetworkIconTest, DefaultNetworkImageVpnAndWifi) {
  SetServiceProperty(wifi1_path(), shill::kSignalStrengthProperty,
                     base::Value(65));
  SetServiceProperty(wifi1_path(), shill::kStateProperty,
                     base::Value(shill::kStateAssociation));

  std::string vpn_path = ConfigureService(
      R"({"GUID": "vpn_guid", "Type": "vpn", "State": "online"})");
  ASSERT_FALSE(vpn_path.empty());

  gfx::ImageSkia default_image;
  base::string16 label;
  bool animating = false;
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_TRUE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState("reference", shill::kTypeWifi,
                                   shill::kStateAssociation, 65);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network.get())));
}

// Tests that cellular image is shown when connecting to cellular network with
// VPN.
TEST_F(NetworkIconTest, DefaultNetworkImageVpnAndCellular) {
  SetServiceProperty(cellular_path(), shill::kSignalStrengthProperty,
                     base::Value(65));
  SetServiceProperty(cellular_path(), shill::kStateProperty,
                     base::Value(shill::kStateAssociation));

  std::string vpn_path = ConfigureService(
      R"({"GUID": "vpn_guid", "Type": "vpn", "State": "online"})");
  ASSERT_FALSE(vpn_path.empty());

  gfx::ImageSkia default_image;
  base::string16 label;
  bool animating = false;
  GetDefaultNetworkImageAndLabel(icon_type_, &default_image, &label,
                                 &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_TRUE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState("reference", shill::kTypeCellular,
                                   shill::kStateAssociation, 65);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network.get())));
}

}  // namespace network_icon

}  // namespace ash
