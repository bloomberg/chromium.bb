// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_icon.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test.h"
#include "chromeos/network/tether_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace network_icon {

namespace {

const char kShillManagerClientStubWifiDevice[] = "/device/stub_wifi_device1";
const char kShillManagerClientStubCellularDevice[] =
    "/device/stub_cellular_device1";

}  // namespace

class NetworkIconTest : public chromeos::NetworkStateTest {
 public:
  NetworkIconTest() = default;
  ~NetworkIconTest() override = default;

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();
    SetUpDefaultNetworkState();

    chromeos::NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    PurgeNetworkIconCache();
    chromeos::NetworkHandler::Shutdown();

    ShutdownNetworkState();
    chromeos::NetworkStateTest::TearDown();

    chromeos::DBusThreadManager::Shutdown();
  }

  void SetUpDefaultNetworkState() {
    base::RunLoop().RunUntilIdle();  // Process any pending updates
    device_test_ = chromeos::DBusThreadManager::Get()
                       ->GetShillDeviceClient()
                       ->GetTestInterface();
    ASSERT_TRUE(device_test_);
    device_test_->ClearDevices();
    device_test_->AddDevice(kShillManagerClientStubWifiDevice, shill::kTypeWifi,
                            "stub_wifi_device1");
    device_test_->AddDevice(kShillManagerClientStubCellularDevice,
                            shill::kTypeCellular, "stub_cellular_device1");

    ClearDefaultServices();

    wifi1_path_ = ConfigureService(
        R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "idle"})");
    wifi2_path_ = ConfigureService(
        R"({"GUID": "wifi2_guid", "Type": "wifi", "State": "idle"})");
    cellular_path_ = ConfigureService(
        R"({"GUID": "cellular_guid", "Type": "cellular", "Technology": "LTE",
            "State": "idle"})");
  }

  std::unique_ptr<chromeos::NetworkState> CreateStandaloneNetworkState(
      const std::string& id,
      const std::string& type,
      const std::string& connection_state,
      int signal_strength) {
    auto network = std::make_unique<chromeos::NetworkState>(id);
    network->set_type(type);
    network->set_visible(true);
    network->set_connection_state(connection_state);
    network->set_signal_strength(signal_strength);
    return network;
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
    gfx::ImageSkia image_skia = GetImageForNetwork(network, icon_type_);
    return gfx::Image(image_skia);
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
    test_manager_client()->RemoveTechnology(shill::kTypeCellular);

    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(
        chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
        network_state_handler()->GetTechnologyState(
            chromeos::NetworkTypePattern::Cellular()));
  }

  void SetCellularUninitialized() {
    test_manager_client()->RemoveTechnology(shill::kTypeCellular);
    test_manager_client()->AddTechnology(shill::kTypeCellular, false);
    test_manager_client()->SetTechnologyInitializing(shill::kTypeCellular,
                                                     true);

    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(chromeos::NetworkStateHandler::TechnologyState::
                  TECHNOLOGY_UNINITIALIZED,
              network_state_handler()->GetTechnologyState(
                  chromeos::NetworkTypePattern::Cellular()));
  }

  const std::string& wifi1_path() const { return wifi1_path_; }
  const std::string& wifi2_path() const { return wifi2_path_; }
  const std::string& cellular_path() const { return cellular_path_; }

  IconType icon_type_ = ICON_TYPE_TRAY_REGULAR;

  chromeos::ShillDeviceClient::TestInterface* device_test_;

 private:
  const base::MessageLoop message_loop_;

  // Preconfigured service paths:
  std::string wifi1_path_;
  std::string wifi2_path_;
  std::string cellular_path_;

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

TEST_F(NetworkIconTest,
       GetCellularUninitializedMsg_NoUninitializedMessageExpected) {
  EXPECT_EQ(0, GetCellularUninitializedMsg());
}

TEST_F(NetworkIconTest, GetCellularUninitializedMsg_CellularUninitialized) {
  SetCellularUninitialized();

  EXPECT_EQ(IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR,
            GetCellularUninitializedMsg());
}

TEST_F(NetworkIconTest, GetCellularUninitializedMsg_CellularScanning) {
  SetCellularUninitialized();

  test_manager_client()->AddTechnology(shill::kTypeCellular, true);

  device_test_->SetDeviceProperty(kShillManagerClientStubCellularDevice,
                                  shill::kScanningProperty, base::Value(true),
                                  /*notify_changed=*/true);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(network_state_handler()->GetScanningByType(
      chromeos::NetworkTypePattern::Cellular()));

  EXPECT_EQ(IDS_ASH_STATUS_TRAY_MOBILE_SCANNING, GetCellularUninitializedMsg());
}

TEST_F(NetworkIconTest, NetworkSignalStrength) {
  using ss = SignalStrength;

  std::unique_ptr<chromeos::NetworkState> ethernet_network =
      CreateStandaloneNetworkState("eth", shill::kTypeEthernet,
                                   shill::kStateOnline, 50);

  std::unique_ptr<chromeos::NetworkState> wifi_network =
      CreateStandaloneNetworkState("wifi", shill::kTypeWifi,
                                   shill::kStateOnline, 50);

  // Verify non-wirless network types return SignalStrength::NOT_WIRELESS, and
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
  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
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
  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
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
  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
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
  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
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
  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
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
  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
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
  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
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
  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
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

  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
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

  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network_2.get())));
}

// Tests that the default network image shows a cellular network icon if
// cellular network is connected while wifi is connecting.
TEST_F(NetworkIconTest, DefaultImageConnectingToWifiWileCellularConnected) {
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
  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState("reference", shill::kTypeCellular,
                                   shill::kStateOnline, 65);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network.get())));
}

// Test that a cellular icon is displayed when activating cellular
// network (if other networks are not connected).
TEST_F(NetworkIconTest, DefaultNetworkImageActivatingCellularNetwork) {
  SetServiceProperty(cellular_path(), shill::kSignalStrengthProperty,
                     base::Value(65));
  SetServiceProperty(cellular_path(), shill::kActivationStateProperty,
                     base::Value(shill::kActivationStateActivating));

  gfx::ImageSkia default_image;
  base::string16 label;
  bool animating = false;
  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
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
  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
  ASSERT_FALSE(default_image.isNull());
  EXPECT_FALSE(animating);

  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState("reference", shill::kTypeWifi,
                                   shill::kStateIdle, 45);
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(default_image), ImageForNetwork(reference_network.get())));
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
  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
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
  ash::network_icon::GetDefaultNetworkImageAndLabel(icon_type_, &default_image,
                                                    &label, &animating);
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
