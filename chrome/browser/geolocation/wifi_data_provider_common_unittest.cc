// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/wifi_data_provider_common.h"

#include <vector>

#include "base/dynamic_annotations.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  MockWlanApi() : calls_(0), bool_return_(true) {
    ANNOTATE_BENIGN_RACE(&calls_, "This is a test-only data race on a counter");
  }
  virtual bool GetAccessPointData(WifiData::AccessPointDataSet* data) {
    ++calls_;
    *data = data_out_;
    return bool_return_;
  }
  int calls_;
  bool bool_return_;
  WifiData::AccessPointDataSet data_out_;
};

// Stops the specified (nested) message loop when the listener is called back.
class MessageLoopQuitListener
    : public WifiDataProviderCommon::ListenerInterface {
 public:
  explicit MessageLoopQuitListener(MessageLoop* message_loop)
      : message_loop_to_quit_(message_loop) {
    CHECK(message_loop_to_quit_ != NULL);
  }
  // ListenerInterface
  virtual void DeviceDataUpdateAvailable(
      DeviceDataProvider<WifiData>* provider) {
    // Provider should call back on client's thread.
    EXPECT_EQ(MessageLoop::current(), message_loop_to_quit_);
    provider_ = provider;
    message_loop_to_quit_->Quit();
  }
  MessageLoop* message_loop_to_quit_;
  DeviceDataProvider<WifiData>* provider_;
};

class WifiDataProviderCommonWithMock : public WifiDataProviderCommon {
 public:
  WifiDataProviderCommonWithMock()
      : new_wlan_api_(new MockWlanApi) {}

  // WifiDataProviderCommon
  virtual WlanApiInterface* NewWlanApi() {
    CHECK(new_wlan_api_ != NULL);
    return new_wlan_api_.release();
  }
  virtual PollingPolicyInterface* NewPollingPolicy() {
    return new GenericPollingPolicy<1, 2, 3>;
  }

  scoped_ptr<MockWlanApi> new_wlan_api_;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderCommonWithMock);
};


WifiDataProviderImplBase* CreateWifiDataProviderCommonWithMock() {
  return new WifiDataProviderCommonWithMock;
}

// Main test fixture
class GeolocationWifiDataProviderCommonTest : public testing::Test {
 public:
  virtual void SetUp() {
    provider_ = new WifiDataProviderCommonWithMock;
    wlan_api_ = provider_->new_wlan_api_.get();
  }
  virtual void TearDown() {
    provider_->StopDataProvider();
    provider_ = NULL;
  }

 protected:
  MessageLoop main_message_loop_;
  scoped_refptr<WifiDataProviderCommonWithMock> provider_;
  MockWlanApi* wlan_api_;
};

TEST_F(GeolocationWifiDataProviderCommonTest, CreateDestroy) {
  // Test fixture members were SetUp correctly.
  EXPECT_EQ(&main_message_loop_, MessageLoop::current());
  EXPECT_TRUE(NULL != provider_.get());
  EXPECT_TRUE(NULL != wlan_api_);
}

TEST_F(GeolocationWifiDataProviderCommonTest, StartThread) {
  EXPECT_TRUE(provider_->StartDataProvider());
  provider_->StopDataProvider();
  SUCCEED();
}

TEST_F(GeolocationWifiDataProviderCommonTest, DoAnEmptyScan) {
  MessageLoopQuitListener quit_listener(&main_message_loop_);
  provider_->AddListener(&quit_listener);
  EXPECT_TRUE(provider_->StartDataProvider());
  main_message_loop_.Run();
  // Check we had at least one call. The worker thread may have raced ahead
  // and made multiple calls.
  EXPECT_GT(wlan_api_->calls_, 0);
  WifiData data;
  EXPECT_TRUE(provider_->GetData(&data));
  EXPECT_EQ(0, static_cast<int>(data.access_point_data.size()));
  provider_->RemoveListener(&quit_listener);
}

TEST_F(GeolocationWifiDataProviderCommonTest, DoScanWithResults) {
  MessageLoopQuitListener quit_listener(&main_message_loop_);
  provider_->AddListener(&quit_listener);
  AccessPointData single_access_point;
  single_access_point.channel = 2;
  single_access_point.mac_address = 3;
  single_access_point.radio_signal_strength = 4;
  single_access_point.signal_to_noise = 5;
  single_access_point.ssid = ASCIIToUTF16("foossid");
  wlan_api_->data_out_.insert(single_access_point);

  EXPECT_TRUE(provider_->StartDataProvider());
  main_message_loop_.Run();
  EXPECT_GT(wlan_api_->calls_, 0);
  WifiData data;
  EXPECT_TRUE(provider_->GetData(&data));
  EXPECT_EQ(1, static_cast<int>(data.access_point_data.size()));
  EXPECT_EQ(single_access_point.ssid, data.access_point_data.begin()->ssid);
  provider_->RemoveListener(&quit_listener);
}

TEST_F(GeolocationWifiDataProviderCommonTest,
       StartThreadViaDeviceDataProvider) {
  MessageLoopQuitListener quit_listener(&main_message_loop_);
  WifiDataProvider::SetFactory(CreateWifiDataProviderCommonWithMock);
  DeviceDataProvider<WifiData>::Register(&quit_listener);
  main_message_loop_.Run();
  DeviceDataProvider<WifiData>::Unregister(&quit_listener);
  DeviceDataProvider<WifiData>::ResetFactory();
}
