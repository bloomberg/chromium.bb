// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/wifi_data_provider_win.h"

#include <vector>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/geolocation/wifi_data_provider_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class MockWlanApi : public Win32WifiDataProvider::WlanApiInterface {
 public:
  MockWlanApi() : calls_(0), bool_return_(true) {
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

class MockPollingPolicy : public PollingPolicyInterface {
 public:
  virtual void UpdatePollingInterval(bool scan_results_differ) {
    results_differed_.push_back(scan_results_differ);
  }
  virtual int PollingInterval() { return 1; }
  std::vector<bool> results_differed_;
};

// Stops the specified (nested) message loop when the listener is called back.
class MessageLoopQuitListener
    : public Win32WifiDataProvider::ListenerInterface {
 public:
  explicit MessageLoopQuitListener(MessageLoop* message_loop)
      : message_loop_to_quit_(message_loop) {
    DCHECK(message_loop_to_quit_ != NULL);
  }
  // ListenerInterface
  virtual void DeviceDataUpdateAvailable(
      DeviceDataProvider<WifiData>* provider) {
    // We expect the callback to come from the provider's internal worker
    // thread. This is not a strict requirement, just a lot of the complexity
    // here is predicated on the need to cope with this scenario!
    EXPECT_NE(MessageLoop::current(), message_loop_to_quit_);
    provider_ = provider;
    // Can't call Quit() directly on another thread's message loop.
    message_loop_to_quit_->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  }
  MessageLoop* message_loop_to_quit_;
  DeviceDataProvider<WifiData>* provider_;
};

// Main test fixture
class Win32WifiDataProviderTest : public testing::Test {
 public:
  static Win32WifiDataProvider* CreateWin32WifiDataProvider(
      MockWlanApi** wlan_api_out) {
    Win32WifiDataProvider* provider = new Win32WifiDataProvider;
    *wlan_api_out = new MockWlanApi;
    provider->inject_mock_wlan_api(*wlan_api_out);  // Takes ownership.
    provider->inject_mock_polling_policy(new MockPollingPolicy);  // ditto
    return provider;
  }
  virtual void SetUp() {
    provider_.reset(CreateWin32WifiDataProvider(&wlan_api_));
  }
  virtual void TearDown() {
    provider_.reset();
  }

 protected:
  MessageLoop main_message_loop_;
  scoped_ptr<Win32WifiDataProvider> provider_;
  MockWlanApi* wlan_api_;
};

WifiDataProviderImplBase* CreateWin32WifiDataProviderStatic() {
  MockWlanApi* wlan_api;
  return Win32WifiDataProviderTest::CreateWin32WifiDataProvider(&wlan_api);
}
}  // namespace

TEST_F(Win32WifiDataProviderTest, CreateDestroy) {
  // Test fixture members were SetUp correctly.
  EXPECT_EQ(&main_message_loop_, MessageLoop::current());
  EXPECT_TRUE(NULL != provider_.get());
  EXPECT_TRUE(NULL != wlan_api_);
}

TEST_F(Win32WifiDataProviderTest, StartThread) {
  EXPECT_TRUE(provider_->StartDataProvider());
  provider_.reset();  // Stop()s the thread.
  SUCCEED();
}

TEST_F(Win32WifiDataProviderTest, DoAnEmptyScan) {
  MessageLoopQuitListener quit_listener(&main_message_loop_);
  provider_->AddListener(&quit_listener);
  EXPECT_TRUE(provider_->StartDataProvider());
  main_message_loop_.Run();
  // Check we had at least one call. The worker thread may have raced ahead
  // and made multiple calls.
  EXPECT_GT(wlan_api_->calls_, 0);
  WifiData data;
  EXPECT_TRUE(provider_->GetData(&data));
  EXPECT_EQ(0, data.access_point_data.size());
  provider_->RemoveListener(&quit_listener);
}

TEST_F(Win32WifiDataProviderTest, DoScanWithResults) {
  MessageLoopQuitListener quit_listener(&main_message_loop_);
  provider_->AddListener(&quit_listener);
  AccessPointData single_access_point;
  single_access_point.age = 1;
  single_access_point.channel = 2;
  single_access_point.mac_address = 3;
  single_access_point.radio_signal_strength = 4;
  single_access_point.signal_to_noise = 5;
  single_access_point.ssid = L"foossid";
  wlan_api_->data_out_.insert(single_access_point);

  EXPECT_TRUE(provider_->StartDataProvider());
  main_message_loop_.Run();
  EXPECT_GT(wlan_api_->calls_, 0);
  WifiData data;
  EXPECT_TRUE(provider_->GetData(&data));
  EXPECT_EQ(1, data.access_point_data.size());
  EXPECT_EQ(single_access_point.age, data.access_point_data.begin()->age);
  EXPECT_EQ(single_access_point.ssid, data.access_point_data.begin()->ssid);
  provider_->RemoveListener(&quit_listener);
}

TEST_F(Win32WifiDataProviderTest, StartThreadViaDeviceDataProvider) {
  MessageLoopQuitListener quit_listener(&main_message_loop_);
  DeviceDataProvider<WifiData>::SetFactory(CreateWin32WifiDataProviderStatic);
  DeviceDataProvider<WifiData>::Register(&quit_listener);
  main_message_loop_.Run();
  DeviceDataProvider<WifiData>::Unregister(&quit_listener);
  DeviceDataProvider<WifiData>::ResetFactory();
}
