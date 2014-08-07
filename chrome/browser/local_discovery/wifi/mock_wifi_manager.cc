// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/wifi/mock_wifi_manager.h"

namespace local_discovery {

namespace wifi {

MockWifiManager::MockWifiManager() : weak_factory_(this) {
}

MockWifiManager::~MockWifiManager() {
}

void MockWifiManager::GetSSIDList(const SSIDListCallback& callback) {
  ssid_list_callback_ = callback;
  GetSSIDListInternal();
}

void MockWifiManager::CallSSIDListCallback(
    const std::vector<NetworkProperties>& networks) {
  ssid_list_callback_.Run(networks);
}

void MockWifiManager::ConfigureAndConnectNetwork(
    const std::string& ssid,
    const WifiCredentials& credentials,
    const SuccessCallback& callback) {
  configure_and_connect_network_callback_ = callback;
  ConfigureAndConnectNetworkInternal(ssid, credentials.psk);
}

void MockWifiManager::CallConfigureAndConnectNetworkCallback(bool success) {
  configure_and_connect_network_callback_.Run(success);
}

void MockWifiManager::ConnectToNetworkByID(const std::string& internal_id,
                                           const SuccessCallback& callback) {
  connect_by_id_callback_ = callback;
  ConnectToNetworkByIDInternal(internal_id);
}

void MockWifiManager::CallConnectToNetworkByIDCallback(bool success) {
  connect_by_id_callback_.Run(success);
}

void MockWifiManager::RequestNetworkCredentials(
    const std::string& internal_id,
    const CredentialsCallback& callback) {
  credentials_callback_ = callback;
  RequestNetworkCredentialsInternal(internal_id);
}

void MockWifiManager::CallRequestNetworkCredentialsCallback(
    bool success,
    const std::string& ssid,
    const std::string& password) {
  credentials_callback_.Run(success, ssid, password);
}

void MockWifiManager::CallNetworkListObservers(
    const std::vector<NetworkProperties>& ssids) {
  FOR_EACH_OBSERVER(
      NetworkListObserver, network_observers_, OnNetworkListChanged(ssids));
}

void MockWifiManager::AddNetworkListObserver(NetworkListObserver* observer) {
  network_observers_.AddObserver(observer);
}

void MockWifiManager::RemoveNetworkListObserver(NetworkListObserver* observer) {
  network_observers_.RemoveObserver(observer);
}

MockWifiManagerFactory::MockWifiManagerFactory() {
  WifiManager::SetFactory(this);
}

MockWifiManagerFactory::~MockWifiManagerFactory() {
  WifiManager::SetFactory(NULL);
}

scoped_ptr<WifiManager> MockWifiManagerFactory::CreateWifiManager() {
  last_created_manager_ = new MockWifiManager();

  WifiManagerCreated();

  return scoped_ptr<WifiManager>(last_created_manager_);
}

MockWifiManager* MockWifiManagerFactory::GetLastCreatedWifiManager() {
  return last_created_manager_;
}

}  // namespace wifi

}  // namespace local_discovery
