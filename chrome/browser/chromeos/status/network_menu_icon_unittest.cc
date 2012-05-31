// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu_icon.h"

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/test/base/testing_browser_process.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Compares each pixel in two bitmaps for testing.
bool CompareTwoBitmaps(const SkBitmap& a, const SkBitmap& b, int log_level) {
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
    ethernet_connected_bitmap_ = NetworkMenuIcon::GenerateBitmapFromComponents(
        *rb_.GetBitmapNamed(IDR_STATUSBAR_WIRED),
        NULL, NULL, NULL, NULL);
    // Ethernet disonnected = WIRED icon + DISCONNECTED badge.
    ethernet_disconnected_bitmap_ =
        NetworkMenuIcon::GenerateBitmapFromComponents(
            *rb_.GetBitmapNamed(IDR_STATUSBAR_WIRED),
            NULL, NULL, NULL,
            rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED));
    // Wifi connected, strength = 100% = ARCS4 icon, no badges.
    wifi_connected_100_bitmap_ = NetworkMenuIcon::GenerateBitmapFromComponents(
        NetworkMenuIcon::GetBitmap(
            NetworkMenuIcon::ARCS,
            NetworkMenuIcon::NumBitmaps(NetworkMenuIcon::ARCS) - 1,
            NetworkMenuIcon::COLOR_DARK),
        NULL, NULL, NULL, NULL);
    // Wifi connected, strength = 50%, encrypted = ARCS2 icon + SECURE badge.
    wifi_encrypted_50_bitmap_ = NetworkMenuIcon::GenerateBitmapFromComponents(
        NetworkMenuIcon::GetBitmap(NetworkMenuIcon::ARCS, 3,
                                   NetworkMenuIcon::COLOR_DARK),
        NULL, NULL, NULL, rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE));
    // Wifi disconnected (strength = 0%) = ARCS0 icon.
    wifi_disconnected_bitmap_ = NetworkMenuIcon::GenerateBitmapFromComponents(
        NetworkMenuIcon::GetBitmap(NetworkMenuIcon::ARCS, 0,
                                   NetworkMenuIcon::COLOR_DARK),
        NULL, NULL, NULL, NULL);
    // Wifi connecting = IDR_STATUSBAR_NETWORK_ARCS1 (faded).
    wifi_connecting_bitmap_ = NetworkMenuIcon::GenerateConnectingBitmap(
        NetworkMenuIcon::GetBitmap(NetworkMenuIcon::ARCS, 1,
                                   NetworkMenuIcon::COLOR_DARK));
    // 4G connected, strength = 50% = BARS4 icon + 4G badge.
    wimax_connected_50_bitmap_ =
        NetworkMenuIcon::GenerateBitmapFromComponents(
            NetworkMenuIcon::GetBitmap(
                NetworkMenuIcon::BARS, 3,
                NetworkMenuIcon::COLOR_DARK),
        NULL, NULL, NULL, rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_4G));
    // 3G connected, strength = 100% = BARS4 icon + 3G badge.
    cellular_connected_100_bitmap_ =
        NetworkMenuIcon::GenerateBitmapFromComponents(
            NetworkMenuIcon::GetBitmap(
                NetworkMenuIcon::BARS,
                NetworkMenuIcon::NumBitmaps(NetworkMenuIcon::BARS) - 1,
                NetworkMenuIcon::COLOR_DARK),
        NULL, NULL, NULL, rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_3G));
    // 3G connected, strength = 50%, roaming = BARS2 icon + roaming & 3G badges.
    cellular_roaming_50_bitmap_ = NetworkMenuIcon::GenerateBitmapFromComponents(
        NetworkMenuIcon::GetBitmap(NetworkMenuIcon::BARS, 3,
                                   NetworkMenuIcon::COLOR_DARK),
        rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_ROAMING), NULL,
        NULL, rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_3G));
    // 3G disconnected (strength = 0%) = BARS0 icon + 3G badge.
    cellular_disconnected_bitmap_ =
        NetworkMenuIcon::GenerateBitmapFromComponents(
            NetworkMenuIcon::GetBitmap(NetworkMenuIcon::BARS, 0,
                                       NetworkMenuIcon::COLOR_DARK),
            NULL, NULL, NULL, rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_3G));
    // 3G connecting = IDR_STATUSBAR_NETWORK_BARS1 (faded).
    cellular_connecting_bitmap_ = NetworkMenuIcon::GenerateConnectingBitmap(
        NetworkMenuIcon::GetBitmap(NetworkMenuIcon::BARS, 1,
                                   NetworkMenuIcon::COLOR_DARK));
  }

  virtual void TearDown() OVERRIDE {
  }

 protected:
  void SetConnected(Network* network, bool connected) {
    Network::TestApi test_network(network);
    test_network.SetConnected(connected);
  }

  void SetConnecting(Network* network, bool connecting) {
    Network::TestApi test_network(network);
    test_network.SetConnecting(connecting);
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

  bool CompareBitmaps(const SkBitmap& icon, const SkBitmap& base) {
    if (CompareTwoBitmaps(icon, base, 1))
      return true;
    EXPECT_FALSE(CompareTwoBitmaps(icon, ethernet_connected_bitmap_, 2));
    EXPECT_FALSE(CompareTwoBitmaps(icon, ethernet_disconnected_bitmap_, 2));
    EXPECT_FALSE(CompareTwoBitmaps(icon, wifi_connected_100_bitmap_, 2));
    EXPECT_FALSE(CompareTwoBitmaps(icon, wifi_encrypted_50_bitmap_, 2));
    EXPECT_FALSE(CompareTwoBitmaps(icon, wifi_disconnected_bitmap_, 2));
    EXPECT_FALSE(CompareTwoBitmaps(icon, wifi_connecting_bitmap_, 2));
    EXPECT_FALSE(CompareTwoBitmaps(icon, cellular_connected_100_bitmap_, 2));
    EXPECT_FALSE(CompareTwoBitmaps(icon, cellular_roaming_50_bitmap_, 2));
    EXPECT_FALSE(CompareTwoBitmaps(icon, cellular_disconnected_bitmap_, 2));
    EXPECT_FALSE(CompareTwoBitmaps(icon, cellular_connecting_bitmap_, 2));
    return false;
  }

 protected:
  ScopedStubCrosEnabler cros_stub_;
  NetworkLibrary* cros_;
  ResourceBundle& rb_;
  SkBitmap ethernet_connected_bitmap_;
  SkBitmap ethernet_disconnected_bitmap_;
  SkBitmap wifi_connected_100_bitmap_;
  SkBitmap wifi_encrypted_50_bitmap_;
  SkBitmap wifi_disconnected_bitmap_;
  SkBitmap wifi_connecting_bitmap_;
  SkBitmap wimax_connected_50_bitmap_;
  SkBitmap cellular_connected_100_bitmap_;
  SkBitmap cellular_roaming_50_bitmap_;
  SkBitmap cellular_disconnected_bitmap_;
  SkBitmap cellular_connecting_bitmap_;
};

// Compare icon cache results against expected results fron SetUp().
TEST_F(NetworkMenuIconTest, EthernetIcon) {
  Network* network = cros_->FindNetworkByPath("eth1");
  ASSERT_NE(static_cast<const Network*>(NULL), network);
  SetConnected(network, true);
  SkBitmap icon = NetworkMenuIcon::GetBitmap(network,
                                             NetworkMenuIcon::COLOR_DARK);
  EXPECT_TRUE(CompareBitmaps(icon, ethernet_connected_bitmap_));

  SetConnected(network, false);
  icon = NetworkMenuIcon::GetBitmap(network,
                                    NetworkMenuIcon::COLOR_DARK);
  EXPECT_TRUE(CompareBitmaps(icon, ethernet_disconnected_bitmap_));
}

TEST_F(NetworkMenuIconTest, WifiIcon) {
  WifiNetwork* network = cros_->FindWifiNetworkByPath("wifi1");
  ASSERT_NE(static_cast<const Network*>(NULL), network);
  SkBitmap icon = NetworkMenuIcon::GetBitmap(network,
                                             NetworkMenuIcon::COLOR_DARK);
  EXPECT_TRUE(CompareBitmaps(icon, wifi_connected_100_bitmap_));

  SetStrength(network, 50);
  SetEncryption(network, SECURITY_RSN);
  icon = NetworkMenuIcon::GetBitmap(network,
                                    NetworkMenuIcon::COLOR_DARK);
  EXPECT_TRUE(CompareBitmaps(icon, wifi_encrypted_50_bitmap_));

  SetConnected(network, false);
  SetStrength(network, 0);
  SetEncryption(network, SECURITY_NONE);
  icon = NetworkMenuIcon::GetBitmap(network,
                                    NetworkMenuIcon::COLOR_DARK);
  EXPECT_TRUE(CompareBitmaps(icon, wifi_disconnected_bitmap_));
}

TEST_F(NetworkMenuIconTest, CellularIcon) {
  CellularNetwork* network = cros_->FindCellularNetworkByPath("cellular1");
  ASSERT_NE(static_cast<const Network*>(NULL), network);
  SetConnected(network, true);
  SetStrength(network, 100);
  SetRoamingState(network, ROAMING_STATE_HOME);
  SkBitmap icon = NetworkMenuIcon::GetBitmap(network,
                                             NetworkMenuIcon::COLOR_DARK);
  EXPECT_TRUE(CompareBitmaps(icon, cellular_connected_100_bitmap_));

  SetStrength(network, 50);
  SetRoamingState(network, ROAMING_STATE_ROAMING);
  icon = NetworkMenuIcon::GetBitmap(network,
                                    NetworkMenuIcon::COLOR_DARK);
  EXPECT_TRUE(CompareBitmaps(icon, cellular_roaming_50_bitmap_));

  SetConnected(network, false);
  SetStrength(network, 0);
  SetRoamingState(network, ROAMING_STATE_HOME);
  icon = NetworkMenuIcon::GetBitmap(network,
                                    NetworkMenuIcon::COLOR_DARK);
  EXPECT_TRUE(CompareBitmaps(icon, cellular_disconnected_bitmap_));
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
// See network_library_unit_test.cc for more info.

TEST_F(NetworkMenuIconTest, StatusIconMenuMode) {
  TestNetworkMenuIcon menu_icon(NetworkMenuIcon::MENU_MODE);
  SkBitmap icon;

  // Set cellular1 to connecting.
  CellularNetwork* cellular1 = cros_->FindCellularNetworkByPath("cellular1");
  ASSERT_NE(static_cast<const Network*>(NULL), cellular1);
  SetRoamingState(cellular1, ROAMING_STATE_HOME);  // Clear romaing state
  SetConnecting(cellular1, true);

  // For MENU_MODE, we always display the connecting icon (cellular1).
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareBitmaps(icon, cellular_connecting_bitmap_));

  // Set cellular1 to connected; ethernet icon should be shown.
  SetConnected(cellular1, true);
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareBitmaps(icon, ethernet_connected_bitmap_));

  // Set ethernet to inactive/disconnected; wifi icon should be shown.
  Network* eth1 = cros_->FindNetworkByPath("eth1");
  ASSERT_NE(static_cast<const Network*>(NULL), eth1);
  SetActive(eth1, false);
  SetConnected(eth1, false);
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareBitmaps(icon, wifi_connected_100_bitmap_));
}

TEST_F(NetworkMenuIconTest, StatusIconDropdownMode) {
  TestNetworkMenuIcon menu_icon(NetworkMenuIcon::DROPDOWN_MODE);
  SkBitmap icon;

  // Set wifi1 to connecting.
  WifiNetwork* wifi1 = cros_->FindWifiNetworkByPath("wifi1");
  ASSERT_NE(static_cast<const Network*>(NULL), wifi1);
  SetConnecting(wifi1, true);

  // For DROPDOWN_MODE, we prioritize the connected network (ethernet).
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareBitmaps(icon, ethernet_connected_bitmap_));

  // Set ethernet to inactive/disconnected.
  Network* ethernet = cros_->FindNetworkByPath("eth1");
  ASSERT_NE(static_cast<const Network*>(NULL), ethernet);
  SetActive(ethernet, false);
  SetConnected(ethernet, false);

  // Icon should now be cellular connected icon.
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareBitmaps(icon, cellular_connected_100_bitmap_));

  // Set cellular1 to disconnected; Icon should now be wimax icon.
  CellularNetwork* cellular1 = cros_->FindCellularNetworkByPath("cellular1");
  ASSERT_NE(static_cast<const Network*>(NULL), cellular1);
  SetConnected(cellular1, false);
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareBitmaps(icon, wimax_connected_50_bitmap_));

  // Set wifi1 to connected. Icon should now be wifi connected icon.
  SetConnected(wifi1, true);
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareBitmaps(icon, wifi_connected_100_bitmap_));
}

}  // namespace chromeos
