// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "content/browser/geolocation/wifi_data_provider_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgumentPointee;

namespace chromeos {

class GeolocationChromeOsWifiDataProviderTest : public testing::Test {
 protected:
  GeolocationChromeOsWifiDataProviderTest()
      : api_(WifiDataProviderChromeOs::NewWlanApi(&net_lib_)) {
  }

  static WifiAccessPointVector MakeWifiAps(int ssids, int aps_per_ssid) {
    WifiAccessPointVector ret;
    for (int i = 0; i < ssids; ++i) {
      for (int j = 0; j < aps_per_ssid; ++j) {
        WifiAccessPoint ap;
        ap.name = StringPrintf("SSID %d", i);
        ap.channel = i * 10 + j;
        ap.mac_address = StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X",
            i, j, 3, 4, 5, 6);
        ap.signal_strength = j;
        ap.signal_to_noise = i;
        ret.push_back(ap);
      }
    }
    return ret;
  }

  chromeos::MockNetworkLibrary net_lib_;
  scoped_ptr<WifiDataProviderCommon::WlanApiInterface> api_;
  WifiData::AccessPointDataSet ap_data_;
};

TEST_F(GeolocationChromeOsWifiDataProviderTest, WifiPoweredOff) {
  EXPECT_CALL(net_lib_, GetWifiAccessPoints(NotNull()))
      .WillOnce(Return(false));
  EXPECT_FALSE(api_->GetAccessPointData(&ap_data_));
  EXPECT_EQ(0u, ap_data_.size());
}

TEST_F(GeolocationChromeOsWifiDataProviderTest, NoAccessPointsInRange) {
  EXPECT_CALL(net_lib_, GetWifiAccessPoints(NotNull()))
      .WillOnce(Return(true));
  EXPECT_CALL(net_lib_, wifi_enabled())
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(api_->GetAccessPointData(&ap_data_));
  EXPECT_EQ(0u, ap_data_.size());
}

TEST_F(GeolocationChromeOsWifiDataProviderTest, GetOneAccessPoint) {
  EXPECT_CALL(net_lib_, GetWifiAccessPoints(NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(MakeWifiAps(1, 1)), Return(true)));
  EXPECT_TRUE(api_->GetAccessPointData(&ap_data_));
  ASSERT_EQ(1u, ap_data_.size());
  EXPECT_EQ("00:00:03:04:05:06", UTF16ToUTF8(ap_data_.begin()->mac_address));
  EXPECT_EQ("SSID 0", UTF16ToUTF8(ap_data_.begin()->ssid));
}

TEST_F(GeolocationChromeOsWifiDataProviderTest, GetManyAccessPoints) {
  EXPECT_CALL(net_lib_, GetWifiAccessPoints(NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(MakeWifiAps(3, 4)), Return(true)));
  EXPECT_TRUE(api_->GetAccessPointData(&ap_data_));
  ASSERT_EQ(12u, ap_data_.size());
}

}  // namespace chromeos
