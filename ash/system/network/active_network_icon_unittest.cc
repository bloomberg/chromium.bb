// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/active_network_icon.h"

#include <memory>
#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_icon.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace {

const char kShillManagerClientStubCellularDevice[] =
    "/device/stub_cellular_device";

}  // namespace

class ActiveNetworkIconTest : public testing::Test {
 public:
  ActiveNetworkIconTest() = default;
  ~ActiveNetworkIconTest() override = default;

  void SetUp() override {
    active_network_icon_ = std::make_unique<ActiveNetworkIcon>();
    active_network_icon_->InitForTesting(helper().network_state_handler());
  }

  void TearDown() override { active_network_icon_.reset(); }

  void SetupEthernet() {
    if (eth_path_.empty()) {
      helper().device_test()->AddDevice("/device/stub_eth_device",
                                        shill::kTypeWifi, "stub_eth_device");
      eth_path_ = ConfigureService(
          R"({"GUID": "eth_guid", "Type": "ethernet", "State": "online"})");
    }
    base::RunLoop().RunUntilIdle();
  }

  void SetupWiFi(const char* state) {
    if (wifi_path_.empty()) {
      // WiFi device already exists.
      wifi_path_ = ConfigureService(
          R"({"GUID": "wifi_guid", "Type": "wifi", "State": "idle"})");
    }
    SetServiceProperty(wifi_path_, shill::kStateProperty, base::Value(state));
    SetServiceProperty(wifi_path_, shill::kSignalStrengthProperty,
                       base::Value(100));
    base::RunLoop().RunUntilIdle();
  }

  void SetupCellular(const char* state) {
    if (cellular_path_.empty()) {
      helper_.manager_test()->AddTechnology(shill::kTypeCellular,
                                            true /* enabled */);
      helper().device_test()->AddDevice(kShillManagerClientStubCellularDevice,
                                        shill::kTypeCellular,
                                        "stub_cellular_device");
      cellular_path_ = ConfigureService(
          R"({"GUID": "cellular_guid", "Type": "cellular", "Technology": "LTE",
            "State": "idle"})");
    }
    SetServiceProperty(cellular_path_, shill::kStateProperty,
                       base::Value(state));
    SetServiceProperty(cellular_path_, shill::kSignalStrengthProperty,
                       base::Value(100));
    base::RunLoop().RunUntilIdle();
  }

  void SetCellularUninitialized(bool scanning) {
    const bool enabled = scanning;
    helper_.manager_test()->RemoveTechnology(shill::kTypeCellular);
    helper_.manager_test()->AddTechnology(shill::kTypeCellular, enabled);
    helper().device_test()->AddDevice(kShillManagerClientStubCellularDevice,
                                      shill::kTypeCellular,
                                      "stub_cellular_device");
    if (scanning) {
      helper().device_test()->SetDeviceProperty(
          kShillManagerClientStubCellularDevice, shill::kScanningProperty,
          base::Value(true), true /* notify_changed */);
    } else {
      helper_.manager_test()->SetTechnologyInitializing(shill::kTypeCellular,
                                                        true);
    }
    base::RunLoop().RunUntilIdle();
  }

  gfx::ImageSkia ImageForNetwork(const chromeos::NetworkState* network) {
    return network_icon::GetImageForNonVirtualNetwork(
        network, icon_type_, false /* show_vpn_badge */);
  }

  bool AreImagesEqual(const gfx::ImageSkia& image,
                      const gfx::ImageSkia& reference) {
    return gfx::test::AreBitmapsEqual(*image.bitmap(), *reference.bitmap());
  }

  std::string ConfigureService(const std::string& shill_json_string) {
    return helper_.ConfigureService(shill_json_string);
  }

  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value) {
    helper_.SetServiceProperty(service_path, key, value);
  }

  std::unique_ptr<chromeos::NetworkState> CreateStandaloneNetworkState(
      const std::string& type,
      const std::string& connection_state,
      int signal_strength = 100) {
    std::string id = base::StringPrintf("reference_%d", reference_count_++);
    return helper_.CreateStandaloneNetworkState(id, type, connection_state,
                                                signal_strength);
  }

  chromeos::NetworkStateTestHelper& helper() { return helper_; }

  ActiveNetworkIcon* active_network_icon() {
    return active_network_icon_.get();
  }

  const std::string& eth_path() const { return eth_path_; }
  const std::string& wifi_path() const { return wifi_path_; }
  const std::string& cellular_path() const { return cellular_path_; }

  network_icon::IconType icon_type() { return icon_type_; }

 private:
  const base::MessageLoop message_loop_;
  chromeos::NetworkStateTestHelper helper_{
      false /* use_default_devices_and_services */};
  std::unique_ptr<ActiveNetworkIcon> active_network_icon_;

  std::string eth_path_;
  std::string wifi_path_;
  std::string cellular_path_;

  network_icon::IconType icon_type_ = network_icon::ICON_TYPE_TRAY_REGULAR;
  // Counter to provide unique ids for reference networks.
  int reference_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ActiveNetworkIconTest);
};

TEST_F(ActiveNetworkIconTest, GetDefaultLabel) {
  SetupCellular(shill::kStateOnline);
  base::string16 label = active_network_icon()->GetDefaultLabel(icon_type());
  // Note: The guid is used for the name in ConfigureService.
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED,
                                       base::UTF8ToUTF16("cellular_guid")),
            label);

  SetupWiFi(shill::kStateIdle);
  helper().network_state_handler()->SetNetworkConnectRequested(wifi_path(),
                                                               true);
  base::RunLoop().RunUntilIdle();
  label = active_network_icon()->GetDefaultLabel(icon_type());
  // Note: The guid is used for the name in ConfigureService.
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING,
                                       base::UTF8ToUTF16("wifi_guid")),
            label);
}

TEST_F(ActiveNetworkIconTest, GetSingleImage) {
  // Cellular only = Cellular icon
  SetupCellular(shill::kStateOnline);
  bool animating;
  gfx::ImageSkia image =
      active_network_icon()->GetSingleImage(icon_type(), &animating);
  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState(shill::kTypeCellular, shill::kStateOnline);
  EXPECT_TRUE(AreImagesEqual(image, ImageForNetwork(reference_network.get())));
  EXPECT_FALSE(animating);

  // Cellular + WiFi connected = WiFi connected icon
  SetupWiFi(shill::kStateOnline);
  image = active_network_icon()->GetSingleImage(icon_type(), &animating);
  reference_network =
      CreateStandaloneNetworkState(shill::kTypeWifi, shill::kStateOnline);
  EXPECT_TRUE(AreImagesEqual(image, ImageForNetwork(reference_network.get())));
  EXPECT_FALSE(animating);

  // Cellular + WiFi connecting = WiFi connecting icon
  SetupWiFi(shill::kStateAssociation);
  helper().network_state_handler()->SetNetworkConnectRequested(wifi_path(),
                                                               true);
  SetServiceProperty(wifi_path(), shill::kSignalStrengthProperty,
                     base::Value(50));
  base::RunLoop().RunUntilIdle();
  image = active_network_icon()->GetSingleImage(icon_type(), &animating);
  reference_network = CreateStandaloneNetworkState(
      shill::kTypeWifi, shill::kStateAssociation, 50);
  EXPECT_TRUE(AreImagesEqual(image, ImageForNetwork(reference_network.get())));
  EXPECT_TRUE(animating);

  // Cellular + WiFi connecting + Ethernet = WiFi connecting icon
  SetupEthernet();
  image = active_network_icon()->GetSingleImage(icon_type(), &animating);
  EXPECT_TRUE(AreImagesEqual(image, ImageForNetwork(reference_network.get())));
  EXPECT_TRUE(animating);

  // Cellular + WiFi connected + Ethernet = No icon
  SetupWiFi(shill::kStateOnline);
  helper().network_state_handler()->SetNetworkConnectRequested(wifi_path(),
                                                               false);
  image = active_network_icon()->GetSingleImage(icon_type(), &animating);
  EXPECT_TRUE(image.isNull());
  EXPECT_FALSE(animating);
}

TEST_F(ActiveNetworkIconTest, CellularUninitialized) {
  SetCellularUninitialized(false /* scanning */);

  base::string16 label = active_network_icon()->GetDefaultLabel(icon_type());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR),
      label);

  bool animating;
  gfx::ImageSkia image =
      active_network_icon()->GetSingleImage(icon_type(), &animating);
  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState(shill::kTypeCellular,
                                   shill::kStateAssociation);
  EXPECT_TRUE(AreImagesEqual(image, ImageForNetwork(reference_network.get())));
  EXPECT_TRUE(animating);
}

TEST_F(ActiveNetworkIconTest, CellularScanning) {
  SetCellularUninitialized(true /* scanning */);

  ASSERT_TRUE(helper().network_state_handler()->GetScanningByType(
      chromeos::NetworkTypePattern::Cellular()));

  base::string16 label = active_network_icon()->GetDefaultLabel(icon_type());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_MOBILE_SCANNING),
            label);

  bool animating;
  gfx::ImageSkia image =
      active_network_icon()->GetSingleImage(icon_type(), &animating);
  std::unique_ptr<chromeos::NetworkState> reference_network =
      CreateStandaloneNetworkState(shill::kTypeCellular,
                                   shill::kStateAssociation);
  EXPECT_TRUE(AreImagesEqual(image, ImageForNetwork(reference_network.get())));
  EXPECT_TRUE(animating);
}

// TODO(stevenjb): Test GetDualImagePrimary, GetDualImageCellular.

}  // namespace ash
