// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/scheduler/network_status_listener.h"

#include "services/network/public/cpp/network_connection_tracker.h"

namespace download {

NetworkStatusListener::NetworkStatusListener() = default;

void NetworkStatusListener::Start(NetworkStatusListener::Observer* observer) {
  observer_ = observer;
}

void NetworkStatusListener::Stop() {
  observer_ = nullptr;
}

NetworkStatusListenerImpl::NetworkStatusListenerImpl(
    network::NetworkConnectionTracker* network_connection_tracker)
    : network_connection_tracker_(network_connection_tracker),
      connection_type_(network::mojom::ConnectionType::CONNECTION_UNKNOWN) {}

NetworkStatusListenerImpl::~NetworkStatusListenerImpl() = default;

void NetworkStatusListenerImpl::Start(
    NetworkStatusListener::Observer* observer) {
  NetworkStatusListener::Start(observer);
  network_connection_tracker_->AddNetworkConnectionObserver(this);
  network_connection_tracker_->GetConnectionType(
      &connection_type_,
      base::BindOnce(&NetworkStatusListenerImpl::OnConnectionChanged,
                     base::Unretained(this)));
}

void NetworkStatusListenerImpl::Stop() {
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  NetworkStatusListener::Stop();
}

network::mojom::ConnectionType NetworkStatusListenerImpl::GetConnectionType() {
  return connection_type_;
}

void NetworkStatusListenerImpl::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK(observer_);
  observer_->OnNetworkChanged(type);
}

}  // namespace download
