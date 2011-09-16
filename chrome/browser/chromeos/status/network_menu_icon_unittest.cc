// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu_icon.h"

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/test/base/testing_browser_process.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

bool CompareBitmaps(const SkBitmap* a, const SkBitmap* b) {
  CHECK(a);
  CHECK(b);
  if (a->getSize() != b->getSize()) {
    LOG(WARNING) << "Mistmatched size: "
                 << a->getSize() << " != " << b->getSize();
    return false;
  }
  size_t bytes = a->getSize();
  SkAutoLockPixels locka(*a);
  SkAutoLockPixels lockb(*b);
  const char* pixa = static_cast<const char*>(a->getPixels());
  const char* pixb = static_cast<const char*>(b->getPixels());
  if (!pixa || !pixb) {
    if (!pixa)
      LOG(WARNING) << "getPixels() returned NULL for LHS";
    if (!pixb)
      LOG(WARNING) << "getPixels() returned NULL for RHS";
    return false;
  }
  size_t width = a->width();
  size_t height = a->height();
  size_t bpp = a->bytesPerPixel();
  if (width * height * bpp != bytes) {
    LOG(WARNING) << "Width: " << width << " x Height: " << height
                 << " x bpp: " << bpp << " != Size: " << bytes;
    return false;
  }
  for (int y=0; y<a->height(); ++y) {
    for (int x=0; x<a->width(); ++x) {
      for (size_t i = 0; i<bpp; ++i) {
        if (*pixa++ != *pixb++) {
          LOG(WARNING) << "Icon: " << width << " x " << height << " x " << bpp
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
    NetworkMenuIcon::GenerateBitmapFromComponents(
        rb_.GetBitmapNamed(IDR_STATUSBAR_WIRED),
        NULL, NULL, NULL, NULL,
        &ethernet_connected_bitmap_);
    // Ethernet disonnected = WIRED icon + DISCONNECTED badge.
    NetworkMenuIcon::GenerateBitmapFromComponents(
        rb_.GetBitmapNamed(IDR_STATUSBAR_WIRED),
        NULL, NULL, NULL,
        rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED),
        &ethernet_disconnected_bitmap_);
    // Wifi connected, strength = 100% = ARCS7 icon, no badges.
    NetworkMenuIcon::GenerateBitmapFromComponents(
        rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_ARCS7),
        NULL, NULL, NULL, NULL,
        &wifi_connected_100_bitmap_);
    // Wifi connected, strength = 50%, encrypted = ARCS4 icon + SECURE badge.
    NetworkMenuIcon::GenerateBitmapFromComponents(
        rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_ARCS4),
        NULL, NULL, NULL, rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE),
        &wifi_encrypted_50_bitmap_);
    // Wifi disconnected (strength = 0%) = ARCS0 icon.
    NetworkMenuIcon::GenerateBitmapFromComponents(
        rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_ARCS0),
        NULL, NULL, NULL, NULL,
        &wifi_disconnected_bitmap_);
    // Wifi connecting = IDR_STATUSBAR_NETWORK_ARCS1 (faded).
    NetworkMenuIcon::GenerateConnectingBitmap(
        rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_ARCS1),
        &wifi_connecting_bitmap_);
    // 3G connected, strength = 100% = BARS4 icon + 3G badge.
    NetworkMenuIcon::GenerateBitmapFromComponents(
        rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS4),
        NULL, NULL, NULL, rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_3G),
        &cellular_connected_100_bitmap_);
    // 3G connected, strength = 50%, roaming = BARS2 icon + roaming & 3G badges.
    NetworkMenuIcon::GenerateBitmapFromComponents(
        rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS3),
        rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_ROAMING), NULL,
        NULL, rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_3G),
       &cellular_roaming_50_bitmap_);
    // 3G disconnected (strength = 0%) = BARS0 icon + 3G badge.
    NetworkMenuIcon::GenerateBitmapFromComponents(
        rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0),
        NULL, NULL, NULL, rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_3G),
        &cellular_disconnected_bitmap_);
    // 3G connecting = IDR_STATUSBAR_NETWORK_BARS1 (faded).
    NetworkMenuIcon::GenerateConnectingBitmap(
        rb_.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS1),
        &cellular_connecting_bitmap_);
  }
  virtual void TearDown() OVERRIDE {
  }

  void SetConnected(Network* network, bool connected) {
    Network::TestApi test_network(network);
    test_network.SetConnected(connected);
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

  ScopedStubCrosEnabler cros_stub_;
  NetworkLibrary* cros_;
  ResourceBundle& rb_;
  SkBitmap ethernet_connected_bitmap_;
  SkBitmap ethernet_disconnected_bitmap_;
  SkBitmap wifi_connected_100_bitmap_;
  SkBitmap wifi_encrypted_50_bitmap_;
  SkBitmap wifi_disconnected_bitmap_;
  SkBitmap wifi_connecting_bitmap_;
  SkBitmap cellular_connected_100_bitmap_;
  SkBitmap cellular_roaming_50_bitmap_;
  SkBitmap cellular_disconnected_bitmap_;
  SkBitmap cellular_connecting_bitmap_;
};

// Test icon cache consistency.
TEST_F(NetworkMenuIconTest, NetworkIconCache) {
  Network* network1 = cros_->FindNetworkByPath("wifi1");  // connected
  Network* network2 = cros_->FindNetworkByPath("wifi2");  // connecting
  ASSERT_NE(static_cast<const Network*>(NULL), network1);
  ASSERT_NE(static_cast<const Network*>(NULL), network2);
  // Different network paths should return different icons.
  const SkBitmap* icon1 = NetworkMenuIcon::GetBitmap(network1);
  const SkBitmap* icon2 = NetworkMenuIcon::GetBitmap(network2);
  EXPECT_NE(icon1, icon2);
  // Same network paths should return the same icon.
  const SkBitmap* icon1b = NetworkMenuIcon::GetBitmap(network1);
  EXPECT_EQ(icon1, icon1b);
  // Changing properties of a network should update the icon, but not change
  // the returned pointer.
  SetConnected(network2, false);
  const SkBitmap* icon2b = NetworkMenuIcon::GetBitmap(network2);
  EXPECT_EQ(icon2, icon2b);
}

// Compare icon cache results against expected results fron SetUp().
TEST_F(NetworkMenuIconTest, EthernetIcon) {
  Network* network = cros_->FindNetworkByPath("eth1");
  ASSERT_NE(static_cast<const Network*>(NULL), network);
  SetConnected(network, true);
  const SkBitmap* icon = NetworkMenuIcon::GetBitmap(network);
  EXPECT_TRUE(CompareBitmaps(icon, &ethernet_connected_bitmap_));

  SetConnected(network, false);
  icon = NetworkMenuIcon::GetBitmap(network);
  EXPECT_TRUE(CompareBitmaps(icon, &ethernet_disconnected_bitmap_));
}

TEST_F(NetworkMenuIconTest, WifiIcon) {
  WifiNetwork* network = cros_->FindWifiNetworkByPath("wifi1");
  ASSERT_NE(static_cast<const Network*>(NULL), network);
  const SkBitmap* icon = NetworkMenuIcon::GetBitmap(network);
  EXPECT_TRUE(CompareBitmaps(icon, &wifi_connected_100_bitmap_));

  SetStrength(network, 50);
  SetEncryption(network, SECURITY_RSN);
  icon = NetworkMenuIcon::GetBitmap(network);
  EXPECT_TRUE(CompareBitmaps(icon, &wifi_encrypted_50_bitmap_));

  SetConnected(network, false);
  SetStrength(network, 0);
  SetEncryption(network, SECURITY_NONE);
  icon = NetworkMenuIcon::GetBitmap(network);
  EXPECT_TRUE(CompareBitmaps(icon, &wifi_disconnected_bitmap_));
}

TEST_F(NetworkMenuIconTest, CellularIcon) {
  CellularNetwork* network = cros_->FindCellularNetworkByPath("cellular1");
  ASSERT_NE(static_cast<const Network*>(NULL), network);
  SetConnected(network, true);
  SetStrength(network, 100);
  SetRoamingState(network, ROAMING_STATE_HOME);
  const SkBitmap* icon = NetworkMenuIcon::GetBitmap(network);
  EXPECT_TRUE(CompareBitmaps(icon, &cellular_connected_100_bitmap_));

  SetStrength(network, 50);
  SetRoamingState(network, ROAMING_STATE_ROAMING);
  icon = NetworkMenuIcon::GetBitmap(network);
  EXPECT_TRUE(CompareBitmaps(icon, &cellular_roaming_50_bitmap_));

  SetConnected(network, false);
  SetStrength(network, 0);
  SetRoamingState(network, ROAMING_STATE_HOME);
  icon = NetworkMenuIcon::GetBitmap(network);
  EXPECT_TRUE(CompareBitmaps(icon, &cellular_disconnected_bitmap_));
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
//  wifi1: connected, strength 100 (active wifi)
//  wifi2: connecting
//  wifi3: disconnected, WEP
//  cellular1: connecting, activated, roaming (active cellular)
// See network_library_unit_test.cc for more info.

TEST_F(NetworkMenuIconTest, StatusIconMenuMode) {
  TestNetworkMenuIcon menu_icon(NetworkMenuIcon::MENU_MODE);
  const SkBitmap* icon;

  // Set up the initial network state.
  CellularNetwork* cellular1 = cros_->FindCellularNetworkByPath("cellular1");
  ASSERT_NE(static_cast<const Network*>(NULL), cellular1);
  SetRoamingState(cellular1, ROAMING_STATE_HOME);  // Clear romaing state

  // For MENU_MODE, we always display the connecting icon (cellular1).
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareBitmaps(icon, &cellular_connecting_bitmap_));

  // Set cellular1 to connected; ethernet icon should be shown.
  SetConnected(cellular1, true);
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareBitmaps(icon, &ethernet_connected_bitmap_));

  // Set ethernet to inactive/disconnected; wifi icon should be shown.
  Network* eth1 = cros_->FindNetworkByPath("eth1");
  ASSERT_NE(static_cast<const Network*>(NULL), eth1);
  SetActive(eth1, false);
  SetConnected(eth1, false);
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareBitmaps(icon, &wifi_connected_100_bitmap_));
}

TEST_F(NetworkMenuIconTest, StatusIconDropdownMode) {
  TestNetworkMenuIcon menu_icon(NetworkMenuIcon::DROPDOWN_MODE);
  const SkBitmap* icon;

  // Start with the default stub network state.

  // For DROPDOWN_MODE, we prioritize the connected network (ethernet).
  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareBitmaps(icon, &ethernet_connected_bitmap_));

  // Set ethernet to disconnected+inactive; wifi icon should be shown.
  Network* ethernet = cros_->FindNetworkByPath("eth1");
  ASSERT_NE(static_cast<const Network*>(NULL), ethernet);
  SetActive(ethernet, false);
  SetConnected(ethernet, false);

  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareBitmaps(icon, &wifi_connected_100_bitmap_));

  // Set wifi2 to active; wifi connecting icon should be shown.
  WifiNetwork* wifi1 = cros_->FindWifiNetworkByPath("wifi1");
  WifiNetwork* wifi2 = cros_->FindWifiNetworkByPath("wifi2");
  ASSERT_NE(static_cast<const Network*>(NULL), wifi2);
  SetConnected(wifi1, false);
  SetActive(wifi2, true);

  icon = menu_icon.GetIconAndText(NULL);
  EXPECT_TRUE(CompareBitmaps(icon, &wifi_connecting_bitmap_));
}

}  // namespace chromeos
