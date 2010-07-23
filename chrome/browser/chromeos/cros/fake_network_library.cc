// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/fake_network_library.h"

namespace chromeos {

FakeNetworkLibrary::FakeNetworkLibrary() : ip_address_("1.1.1.1") {
}

void FakeNetworkLibrary::AddObserver(Observer* observer) {
}

void FakeNetworkLibrary::RemoveObserver(Observer* observer) {
}

bool FakeNetworkLibrary::Connected() const {
  return true;
}

bool FakeNetworkLibrary::Connecting() const {
  return false;
}

const std::string& FakeNetworkLibrary::IPAddress() const {
  return ip_address_;
}

bool FakeNetworkLibrary::FindWifiNetworkByPath(
    const std::string& path, WifiNetwork* result) const {
  return false;
}

bool FakeNetworkLibrary::FindCellularNetworkByPath(
    const std::string& path, CellularNetwork* result) const {
  return false;
}

void FakeNetworkLibrary::RequestWifiScan() {
}

bool FakeNetworkLibrary::GetWifiAccessPoints(WifiAccessPointVector* result) {
  return true;
}

bool FakeNetworkLibrary::ConnectToPreferredNetworkIfAvailable() {
  return false;
}

bool FakeNetworkLibrary::PreferredNetworkConnected() {
  return false;
}

bool FakeNetworkLibrary::PreferredNetworkFailed() {
  return false;
}

void FakeNetworkLibrary::ConnectToWifiNetwork(WifiNetwork network,
                                              const std::string& password,
                                              const std::string& identity,
                                              const std::string& certpath) {
}

void FakeNetworkLibrary::ConnectToWifiNetwork(const std::string& ssid,
                                              const std::string& password,
                                              const std::string& identity,
                                              const std::string& certpath,
                                              bool auto_connect) {
}

void FakeNetworkLibrary::ConnectToCellularNetwork(CellularNetwork network) {
}

void FakeNetworkLibrary::DisconnectFromWirelessNetwork(
    const WirelessNetwork& network) {
}

void FakeNetworkLibrary::SaveWifiNetwork(const WifiNetwork& network) {
}

void FakeNetworkLibrary::ForgetWirelessNetwork(const WirelessNetwork& network) {
}

void FakeNetworkLibrary::EnableEthernetNetworkDevice(bool enable) {
}

void FakeNetworkLibrary::EnableWifiNetworkDevice(bool enable) {
}

void FakeNetworkLibrary::EnableCellularNetworkDevice(bool enable) {
}

void FakeNetworkLibrary::EnableOfflineMode(bool enable) {
}

NetworkIPConfigVector FakeNetworkLibrary::GetIPConfigs(
    const std::string& device_path) {
  NetworkIPConfigVector ipconfig_vector;
  return ipconfig_vector;
}

std::string FakeNetworkLibrary::GetHtmlInfo(int refresh) {
  return "";
}

}  // namespace chromeos
