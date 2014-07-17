// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

NetworkStateHandlerObserver::NetworkStateHandlerObserver() {
}

NetworkStateHandlerObserver::~NetworkStateHandlerObserver() {
}

void NetworkStateHandlerObserver::NetworkListChanged() {
}

void NetworkStateHandlerObserver::DeviceListChanged() {
}

void NetworkStateHandlerObserver::DefaultNetworkChanged(
    const NetworkState* network) {
}

void NetworkStateHandlerObserver::NetworkConnectionStateChanged(
    const NetworkState* network) {
}

void NetworkStateHandlerObserver::NetworkPropertiesUpdated(
    const NetworkState* network) {
}

void NetworkStateHandlerObserver::DevicePropertiesUpdated(
    const chromeos::DeviceState* device) {
}

void NetworkStateHandlerObserver::IsShuttingDown() {
}

}  // namespace chromeos
