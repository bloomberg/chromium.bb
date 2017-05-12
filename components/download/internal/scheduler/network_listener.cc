// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/scheduler/network_listener.h"

namespace download {

namespace {

// Converts a ConnectionType to NetworkListener::NetworkStatus.
NetworkListener::NetworkStatus ToNetworkStatus(
    net::NetworkChangeNotifier::ConnectionType type) {
  switch (type) {
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return NetworkListener::NetworkStatus::UNMETERED;
    case net::NetworkChangeNotifier::CONNECTION_2G:
    case net::NetworkChangeNotifier::CONNECTION_3G:
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return NetworkListener::NetworkStatus::METERED;
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
    case net::NetworkChangeNotifier::CONNECTION_NONE:
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return NetworkListener::NetworkStatus::DISCONNECTED;
  }
  NOTREACHED();
  return NetworkListener::NetworkStatus::DISCONNECTED;
}

}  // namespace

NetworkListener::NetworkListener()
    : status_(NetworkStatus::DISCONNECTED), listening_(false) {}

NetworkListener::~NetworkListener() {
  Stop();
}

NetworkListener::NetworkStatus NetworkListener::CurrentNetworkStatus() const {
  return ToNetworkStatus(net::NetworkChangeNotifier::GetConnectionType());
}

void NetworkListener::Start() {
  if (listening_)
    return;

  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  status_ = ToNetworkStatus(net::NetworkChangeNotifier::GetConnectionType());
  listening_ = true;
}

void NetworkListener::Stop() {
  if (!listening_)
    return;

  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  status_ = ToNetworkStatus(net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  listening_ = false;
}

void NetworkListener::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NetworkListener::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkListener::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  NetworkStatus new_status = ToNetworkStatus(type);
  if (status_ != new_status) {
    status_ = new_status;
    NotifyNetworkChange(status_);
  }
}

void NetworkListener::NotifyNetworkChange(NetworkStatus network_status) {
  for (auto& observer : observers_)
    observer.OnNetworkChange(network_status);
}

}  // namespace download
