// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu_icon.h"

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/test/base/testing_browser_process.h"
#include "grit/ash_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Compares each pixel in the 1x representation of two images for testing.
// TODO(pkotwicz): Compare pixels of all bitmaps contained within image.
bool CompareTwoImages(const gfx::ImageSkia& image_a,
                      const gfx::ImageSkia& image_b,
                      int log_level) {
  CHECK(!image_a.isNull());
  CHECK(!image_b.isNull());

  gfx::ImageSkiaRep image_rep_a =
      image_a.GetRepresentation(ui::SCALE_FACTOR_100P);
  CHECK_EQ(ui::SCALE_FACTOR_100P, image_rep_a.scale_factor());
  gfx::ImageSkiaRep image_rep_b =
      image_b.GetRepresentation(ui::SCALE_FACTOR_100P);
  CHECK_EQ(ui::SCALE_FACTOR_100P, image_rep_b.scale_factor());

  SkBitmap a = image_rep_a.sk_bitmap();
  SkBitmap b = image_rep_b.sk_bitmap();

  CHECK(!a.empty());
  CHECK(!b.empty());
  if (a.getSize() != b.getSize()) {
    VLOG(log_level) << "Mistmatched size: "
                    << a.getSize() << " != " << b.getSize();
    return false;
  }
  size_t bytes = a.getSize();
  SkAutoLockPixels locka(a);
  SkAutoLockPixels lockb(b);
  const char* pixa = static_cast<const char*>(a.getPixels());
  const char* pixb = static_cast<const char*>(b.getPixels());
  if (!pixa || !pixb) {
    if (!pixa)
      VLOG(log_level) << "getPixels() returned NULL for LHS";
    if (!pixb)
      VLOG(log_level) << "getPixels() returned NULL for RHS";
    return false;
  }
  size_t width = a.width();
  size_t height = a.height();
  size_t bpp = a.bytesPerPixel();
  if (width * height * bpp != bytes) {
    VLOG(log_level) << "Width: " << width << " x Height: " << height
                    << " x bpp: " << bpp << " != Size: " << bytes;
    return false;
  }
  for (int y=0; y<a.height(); ++y) {
    for (int x=0; x<a.width(); ++x) {
      for (size_t i = 0; i<bpp; ++i) {
        if (*pixa++ != *pixb++) {
          VLOG(log_level) << "Icon: " << width << "x" << height << "x" << bpp
                          << ", Mismatch at: " << x << "," << y << ":" << i;
          return false;
        }
      }
    }
  }
  return true;
}

} // namespace

namespace chromeos {

class NetworkMenuIconTest : public testing::Test {
 protected:
  NetworkMenuIconTest() : rb_(ResourceBundle::GetSharedInstance()) {}

  // testing::Test implementation.
  virtual void SetUp() OVERRIDE {
    cros_ = CrosLibrary::Get()->GetNetworkLibrary();
    // Ethernet connected = WIRED icon, no badges.
    ethernet_connected_image_ = NetworkMenuIcon::GenerateImageFromComponents(
        *rb_.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_WIRED),
        NULL, NULL, NULL, NULL);
    // Wifi connected, strength = 100% = ARCS4 icon, no badges.
    wifi_connected_100_image_ = NetworkMenuIcon::GenerateImageFromComponents(
        NetworkMenuIcon::GetImage(
            NetworkMenuIcon::ARCS,
            NetworkMenuIcon::NumImages(NetworkMenuIcon::ARCS) - 1,
            NetworkMenuIcon::COLOR_DARK),
        NULL, NULL, NULL, NULL);
    // Wifi connected, strength = 50%, encrypted = ARCS2 icon + SECURE badge.
    wifi_encrypted_50_image_ = NetworkMenuIcon::GenerateImageFromComponents(
        NetworkMenuIcon::GetImage(NetworkMenuIcon::ARCS, 3,
                                  NetworkMenuIcon::COLOR_DARK),
        NULL, NULL, NULL,
        rb_.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_SECURE_DARK));
    // Wifi connecting = IDR_AURA_UBER_TRAY_NETWORK_ARCS1 (faded).
    wifi_connecting_image_ = NetworkMenuIcon::GenerateConnectingImage(
        NetworkMenuIcon::GetImage(NetworkMenuIcon::ARCS, 1,
                                  NetworkMenuIcon::COLOR_DARK));
    // 4G connected, strength = 50% = BARS4 icon + 4G badge.
    wimax_connected_50_image_ =
        NetworkMenuIcon::GenerateImageFromComponents(
            NetworkMenuIcon::GetImage(
                NetworkMenuIcon::BARS, 3,
                NetworkMenuIcon::COLOR_DARK),
            rb_.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_4G_DARK),
            NULL, NULL, NULL);
    // 3G connected, strength = 100% = BARS4 icon + 3G badge.
    cellular_connected_100_image_ =
        NetworkMenuIcon::GenerateImageFromComponents(
            NetworkMenuIcon::GetImage(
                NetworkMenuIcon::BARS,
                NetworkMenuIcon::NumImages(NetworkMenuIcon::BARS) - 1,
                NetworkMenuIcon::COLOR_DARK),
            rb_.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_3G_DARK),
            NULL, NULL, NULL);
    // 3G connected, strength = 50%, roaming = BARS2 icon + roaming & 3G badges.
    cellular_roaming_50_image_ = NetworkMenuIcon::GenerateImageFromComponents(
        NetworkMenuIcon::GetImage(NetworkMenuIcon::BARS, 3,
                                  NetworkMenuIcon::COLOR_DARK),
        rb_.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_3G_DARK), NULL, NULL,
        rb_.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_NETWORK_ROAMING_DARK));
    // 3G connecting = IDR_AURA_UBER_TRAY_NETWORK_BARS1 (faded).
    cellular_connecting_image_ = NetworkMenuIcon::GenerateConnectingImage(
        NetworkMenuIcon::GetImage(NetworkMenuIcon::BARS, 1,
                                  NetworkMenuIcon::COLOR_DARK));
    // Disconnected = ARCS0 icon.
    disconnected_image_ = NetworkMenuIcon::GenerateImageFromComponents(
        NetworkMenuIcon::GetImage(NetworkMenuIcon::ARCS, 0,
                                  NetworkMenuIcon::COLOR_DARK),
        NULL, NULL, NULL, NULL);
  }

  virtual void TearDown() OVERRIDE {
  }

 protected:
  void SetConnected(Network* network) {
    Network::TestApi test_network(network);
    test_network.SetConnected();
    test_network.SetUserConnectState(USER_CONNECT_CONNECTED);
  }

  void SetConnecting(Network* network, bool user_initiated) {
    Network::TestApi test_network(network);
    test_network.SetConnecting();
    test_network.SetUserConnectState(
        user_initiated ? USER_CONNECT_STARTED : USER_CONNECT_NONE);
  }

  void SetDisconnected(Network* network) {
    Network::TestApi test_network(network);
    test_network.SetDisconnected();
    test_network.SetUserConnectState(USER_CONNECT_NONE);
  }

  void SetActive(Network* network, bool active) {
    if (active) {
      cros_->SetActiveNetwork(network->type(), network->service_path());
    } else {
      cros_->SetActiveNetwork(network->type(), "");
    }
  }

  void SetStrength(WirelessNetwork* network, int strength) {
    WirelessNetwork::TestApi test_network(network);
    test_network.SetStrength(strength);
  }

  void SetEncryption(WifiNetwork* network, ConnectionSecurity encryption) {
    WifiNetwork::TestApi test_network(network);
    test_network.SetEncryption(encryption);
  }

  void SetRoamingState(CellularNetwork* network, NetworkRoamingState roaming) {
    CellularNetwork::TestApi test_network(network);
    test_network.SetRoamingState(roaming);
  }

  bool CompareImages(const gfx::ImageSkia& icon, const gfx::ImageSkia& base) {
    if (CompareTwoImages(icon, base, 1))
      return true;
    EXPECT_FALSE(CompareTwoImages(icon, ethernet_connected_image_, 2));
    EXPECT_FALSE(CompareTwoImages(icon, wifi_connected_100_image_, 2));
    EXPECT_FALSE(CompareTwoImages(icon, wifi_encrypted_50_image_, 2));
    EXPECT_FALSE(CompareTwoImages(icon, wifi_connecting_image_, 2));
    EXPECT_FALSE(CompareTwoImages(icon, cellular_connected_100_image_, 2));
    EXPECT_FALSE(CompareTwoImages(icon, cellular_roaming_50_image_, 2));
    EXPECT_FALSE(CompareTwoImages(icon, cellular_connecting_image_, 2));
    EXPECT_FALSE(CompareTwoImages(icon, disconnected_image_, 2));
    return false;
  }

 protected:
  ScopedStubCrosEnabler cros_stub_;
  NetworkLibrary* cros_;
  ResourceBundle& rb_;
  gfx::ImageSkia ethernet_connected_image_;
  gfx::ImageSkia wifi_connected_100_image_;
  gfx::ImageSkia wifi_encrypted_50_image_;
  gfx::ImageSkia wifi_connecting_image_;
  gfx::ImageSkia wimax_connected_50_image_;
  gfx::ImageSkia cellular_connected_100_image_;
  gfx::ImageSkia cellular_roaming_50_image_;
  gfx::ImageSkia cellular_connecting_image_;
  gfx::ImageSkia disconnected_image_;
};

// Compare icon cache results against expected results fron SetUp().
TEST_F(NetworkMenuIconTest, EthernetIcon) {
  Network* network = cros_->FindNetworkByPath("eth1");
  ASSERT_NE(static_cast<const Network*>(NULL), network);
  SetConnected(network);
  gfx::ImageSkia icon = NetworkMenuIcon::GetImage(network,
                                                  NetworkMenuIcon::COLOR_DARK);
  EXPECT_TRUE(CompareImages(icon, ethernet_connected_image_));
}

TEST_F(NetworkMenuIconTest, WifiIcon) {
  WifiNetwork* network = cros_->FindWifiNetworkByPath("wifi1");
  ASSERT_NE(static_cast<const Network*>(NULL), network);
  gfx::ImageSkia icon = NetworkMenuIcon::GetImage(network,
                                                  NetworkMenuIcon::COLOR_DARK);
  EXPECT_TRUE(CompareImages(icon, wifi_connected_100_image_));

  SetStrength(network, 50);
  SetEncryption(network, SECURITY_RSN);
  icon = NetworkMenuIcon::GetImage(network,
                                   NetworkMenuIcon::COLOR_DARK);
  EXPECT_TRUE(CompareImages(icon, wifi_encrypted_50_image_));
}

TEST_F(NetworkMenuIconTest, CellularIcon) {
  CellularNetwork* network = cros_->FindCellularNetworkByPath("cellular1");
  ASSERT_NE(static_cast<const Network*>(NULL), network);
  SetConnected(network);
  SetStrength(network, 100);
  SetRoamingState(network, ROAMING_STATE_HOME);
  gfx::ImageSkia icon = NetworkMenuIcon::GetImage(network,
                                                  NetworkMenuIcon::COLOR_DARK);
  EXPECT_TRUE(CompareImages(icon, cellular_connected_100_image_));

  SetStrength(network, 50);
  SetRoamingState(network, ROAMING_STATE_ROAMING);
  icon = NetworkMenuIcon::GetImage(network,
                                   NetworkMenuIcon::COLOR_DARK);
  EXPECT_TRUE(CompareImages(icon, cellular_roaming_50_image_));
}

namespace {

class TestNetworkMenuIcon : public NetworkMenuIcon {
 public:
  explicit TestNetworkMenuIcon(Mode mode)
      : NetworkMenuIcon(&delegate_, mode),
        animation_(0.0) {
  }
  virtual ~TestNetworkMenuIcon() {}

  // NetworkMenuIcon override.
  virtual double GetAnimation() OVERRIDE { return animation_; }

  void set_animation(double animation) { animation_ = animation; }

 private:
  class Delegate : public NetworkMenuIcon::Delegate {
   public:
    Delegate() : changed_(0) {}
    virtual void NetworkMenuIconChanged() OVERRIDE {
      ++changed_;
    }
    int changed() const { return changed_; }
   private:
    int changed_;
  };
  Delegate delegate_;
  double animation_;
};

}  // namespace

// Test Network Menu status icon logic.

// Default relevent stub state:
//  eth1: connected (active ethernet)
//  wifi1: connected (active wifi)
//  cellular1: connected  (active cellular)
//  wimax2: connected  (active wimax)
// See network_library_unit_test.cc for more info.

TEST_F(NetworkMenuIconTest, StatusIconMenuMode) {
  TestNetworkMenuIcon menu_icon(NetworkMenuIcon::MENU_MODE);
  gfx::ImageSkia icon;

  // Set cellular1 to connecting.
  CellularNetwork* cellular1 = cros_->FindCellularNetworkByPath("cellular1");
  ASSERT_NE(static_cast<const Network*>(NULL), cellular1);
  SetRoamingState(cellular1, ROAMING_STATE_HOME);  // Clear romaing state
  SetConnecting(cellular1, true);

  // For user initiated connect always display the connecting icon (cellular1).
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareImages(icon, cellular_connecting_image_));

  // Set cellular1 to connected; ethernet icon should be shown.
  SetConnected(cellular1);
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareImages(icon, ethernet_connected_image_));

  // Set ethernet to inactive/disconnected; wifi icon should be shown.
  Network* eth1 = cros_->FindNetworkByPath("eth1");
  ASSERT_NE(static_cast<const Network*>(NULL), eth1);
  SetActive(eth1, false);
  SetDisconnected(eth1);
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareImages(icon, wifi_connected_100_image_));

  // Set all networks to disconnected; disconnected icon should be shown.
  SetActive(cellular1, false);
  SetDisconnected(cellular1);
  WifiNetwork* wifi1 = cros_->FindWifiNetworkByPath("wifi1");
  SetActive(wifi1, false);
  SetDisconnected(wifi1);
  WimaxNetwork* wimax2 = cros_->FindWimaxNetworkByPath("wimax2");
  SetActive(wimax2, false);
  SetDisconnected(wimax2);
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareImages(icon, disconnected_image_));
}

TEST_F(NetworkMenuIconTest, StatusIconDropdownMode) {
  TestNetworkMenuIcon menu_icon(NetworkMenuIcon::DROPDOWN_MODE);
  gfx::ImageSkia icon;

  // Set wifi1 to connecting.
  WifiNetwork* wifi1 = cros_->FindWifiNetworkByPath("wifi1");
  ASSERT_NE(static_cast<const Network*>(NULL), wifi1);
  SetConnecting(wifi1, false);

  // For non user-initiated connect, show the connected network (ethernet).
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareImages(icon, ethernet_connected_image_));

  // Set ethernet to inactive/disconnected.
  Network* ethernet = cros_->FindNetworkByPath("eth1");
  ASSERT_NE(static_cast<const Network*>(NULL), ethernet);
  SetActive(ethernet, false);
  SetDisconnected(ethernet);

  // Icon should now be cellular connected icon.
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareImages(icon, cellular_connected_100_image_));

  // Set cellular1 to disconnected; Icon should now be wimax icon.
  CellularNetwork* cellular1 = cros_->FindCellularNetworkByPath("cellular1");
  ASSERT_NE(static_cast<const Network*>(NULL), cellular1);
  SetDisconnected(cellular1);
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareImages(icon, wimax_connected_50_image_));

  // Set wifi1 to connected. Icon should now be wifi connected icon.
  SetConnected(wifi1);
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareImages(icon, wifi_connected_100_image_));
}

}  // namespace chromeos
