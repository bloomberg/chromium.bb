// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

class NetObserver : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  NetObserver() { net::NetworkChangeNotifier::AddNetworkChangeObserver(this); }

  ~NetObserver() override {
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

  void WaitForConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    while (last_connection_type != type) {
      run_loop_.reset(new base::RunLoop());
      run_loop_->Run();
      run_loop_.reset();
    }
  }

  // net::NetworkChangeNotifier:NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    change_count++;
    last_connection_type = type;
    if (run_loop_)
      run_loop_->Quit();
  }

  int change_count = 0;
  net::NetworkChangeNotifier::ConnectionType last_connection_type =
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};

class NetworkServiceObserver
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  NetworkServiceObserver() {
    content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  }

  ~NetworkServiceObserver() override {
    content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
        this);
  }

  void WaitForConnectionType(network::mojom::ConnectionType type) {
    while (last_connection_type != type) {
      run_loop_.reset(new base::RunLoop());
      run_loop_->Run();
      run_loop_.reset();
    }
  }

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    change_count++;
    last_connection_type = type;
    if (run_loop_)
      run_loop_->Quit();
  }

  int change_count = 0;
  network::mojom::ConnectionType last_connection_type =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

class NetworkChangeManagerClientBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    service_client_ =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_client_->ClearServices();
  }

  ShillServiceClient::TestInterface* service_client() {
    return service_client_;
  }

 private:
  ShillServiceClient::TestInterface* service_client_;
};

// Tests that network changes from shill are received by both the
// NetworkChangeNotifier and NetworkConnectionTracker.
IN_PROC_BROWSER_TEST_F(NetworkChangeManagerClientBrowserTest,
                       ReceiveNotifications) {
  NetObserver net_observer;
  NetworkServiceObserver network_service_observer;

  service_client()->AddService("wifi", "wifi", "wifi", shill::kTypeWifi,
                               shill::kStateOnline, true);

  net_observer.WaitForConnectionType(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  // NetworkChangeNotifier will send a CONNECTION_NONE notification before
  // the CONNECTION_WIFI one.
  EXPECT_EQ(2, net_observer.change_count);
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_WIFI,
            net_observer.last_connection_type);

  network_service_observer.WaitForConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_EQ(2, network_service_observer.change_count);
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_WIFI,
            network_service_observer.last_connection_type);
};

// Tests that the NetworkChangeManagerClient reconnects to the network service
// after it gets disconnected.
IN_PROC_BROWSER_TEST_F(NetworkChangeManagerClientBrowserTest,
                       ReconnectToNetworkService) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  {
    // Make sure everyone thinks we have an ethernet connection.
    NetObserver net_observer;
    NetworkServiceObserver network_service_observer;
    net_observer.WaitForConnectionType(
        net::NetworkChangeNotifier::CONNECTION_ETHERNET);
    network_service_observer.WaitForConnectionType(
        network::mojom::ConnectionType::CONNECTION_ETHERNET);
  }

  NetObserver net_observer;
  NetworkServiceObserver network_service_observer;

  SimulateNetworkServiceCrash();
  service_client()->AddService("wifi", "wifi", "wifi", shill::kTypeWifi,
                               shill::kStateOnline, true);

  net_observer.WaitForConnectionType(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  // NetworkChangeNotifier will send a CONNECTION_NONE notification before
  // the CONNECTION_WIFI one.
  EXPECT_EQ(2, net_observer.change_count);
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_WIFI,
            net_observer.last_connection_type);

  network_service_observer.WaitForConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_EQ(2, network_service_observer.change_count);
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_WIFI,
            network_service_observer.last_connection_type);
};

}  // namespace chromeos
