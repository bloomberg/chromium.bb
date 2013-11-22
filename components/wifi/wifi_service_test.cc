// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/onc/onc_constants.h"
#include "components/wifi/wifi_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wifi {

class WiFiServiceTest : public testing::Test {
 public:
  void LogError(const std::string& error_name,
                scoped_ptr<base::DictionaryValue> error_data) {
    LOG(ERROR) << "WiFi Error: " << error_name;
    QuitRunLoop();
  }

  void TestError(const std::string& error_name,
                 scoped_ptr<base::DictionaryValue> error_data) {
    LOG(ERROR) << "WiFi Error: " << error_name;
    FAIL();
  }

  void OnDictionaryResult(const std::string& network_guid,
                          const base::DictionaryValue& dictionary) {}

  void OnNetworkProperties(const std::string& network_guid,
                           const WiFiService::NetworkProperties& properties) {
    LOG(INFO) << "OnNetworkProperties" << *properties.ToValue(false).release();
  }

  void OnNetworksChangedEventWaitingForConnect(
      const WiFiService::NetworkGuidList& network_guid_list) {
    LOG(INFO) << "Networks Changed: " << network_guid_list.front();
    // Check that network is now connected.
    wifi_service_->GetProperties(
        connected_network_guid_,
        base::Bind(&WiFiServiceTest::WaitForConnect, base::Unretained(this)),
        base::Bind(&WiFiServiceTest::TestError, base::Unretained(this)));
  }

  void OnNetworkConnectStarted(const std::string& network_guid) {
    LOG(INFO) << "Started Network Connect:" << network_guid;
    wifi_service_->SetNetworksChangedObserver(
        base::Bind(&WiFiServiceTest::OnNetworksChangedEventWaitingForConnect,
                   base::Unretained(this)));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::MessageLoop::QuitClosure(),
        base::TimeDelta::FromSeconds(10));
  }

  void WaitForConnect(const std::string& network_guid,
                      const WiFiService::NetworkProperties& properties) {
    LOG(INFO) << "WaitForConnect" << *properties.ToValue(false).release();
    if (properties.connection_state == onc::connection_state::kConnected)
      QuitRunLoop();
  }

  void WaitForDisconnect(const std::string& network_guid,
                         const WiFiService::NetworkProperties& properties) {
    LOG(INFO) << "WaitForDisconnect" << *properties.ToValue(false).release();
    EXPECT_TRUE(properties.connection_state ==
                onc::connection_state::kNotConnected);
  }

  void OnNetworkDisconnectStarted(const std::string& network_guid) {
    LOG(INFO) << "Started Network Disconnect:" << network_guid;
    // Check that Network state has changed to 'Not Connected'.
    wifi_service_->GetProperties(
        connected_network_guid_,
        base::Bind(&WiFiServiceTest::WaitForDisconnect, base::Unretained(this)),
        base::Bind(&WiFiServiceTest::TestError, base::Unretained(this)));
    // Start connect back to the same network.
    wifi_service_->StartConnect(
        connected_network_guid_,
        base::Bind(&WiFiServiceTest::OnNetworkConnectStarted,
                   base::Unretained(this)),
        base::Bind(&WiFiServiceTest::TestError, base::Unretained(this)));
  }

  void OnVisibleNetworks(const WiFiService::NetworkList& network_list) {
    LOG(INFO) << "Visible WiFi Networks: " << network_list.size();
  }

  void FindConnectedNetwork(const WiFiService::NetworkList& network_list) {
    for (WiFiService::NetworkList::const_iterator net = network_list.begin();
         net != network_list.end();
         ++net) {
      if (net->connection_state == onc::connection_state::kConnected) {
        connected_network_guid_ = net->guid;
        LOG(INFO) << "Connected Network:\n" << *(net->ToValue(false).release());
      }
    }

    if (!connected_network_guid_.empty()) {
      wifi_service_->StartDisconnect(
          connected_network_guid_,
          base::Bind(&WiFiServiceTest::OnNetworkDisconnectStarted,
                     base::Unretained(this)),
          base::Bind(&WiFiServiceTest::TestError, base::Unretained(this)));
    } else {
      LOG(INFO) << "No Connected Networks, skipping disconnect.";
      QuitRunLoop();
    }
  }

  void QuitRunLoop() {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
  }

 protected:
  virtual void SetUp() { wifi_service_.reset(WiFiService::CreateService()); }
  virtual void TearDown() {}

  std::string connected_network_guid_;
  scoped_ptr<WiFiService> wifi_service_;
  base::MessageLoop loop_;
};

// Test getting list of visible networks.
TEST_F(WiFiServiceTest, GetVisibleNetworks) {
  wifi_service_->GetVisibleNetworks(
      base::Bind(&WiFiServiceTest::OnVisibleNetworks, base::Unretained(this)),
      base::Bind(&WiFiServiceTest::LogError, base::Unretained(this)));
}

// Test that connected WiFi network can be disconnected and reconnected.
// Disabled to avoid network connection interruption unless enabled explicitly.
TEST_F(WiFiServiceTest, DISABLED_Reconnect) {
  base::RunLoop run_loop;
  wifi_service_->GetVisibleNetworks(
      base::Bind(&WiFiServiceTest::FindConnectedNetwork,
                 base::Unretained(this)),
      base::Bind(&WiFiServiceTest::LogError, base::Unretained(this)));
  run_loop.Run();
}

}  // namespace wifi
