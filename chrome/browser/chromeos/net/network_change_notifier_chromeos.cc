// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_change_notifier_chromeos.h"

#include "base/task.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

NetworkChangeNotifierChromeos::NetworkChangeNotifierChromeos()
    : has_active_network_(false),
      connectivity_state_(chromeos::CONN_STATE_UNKNOWN),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {

  chromeos::NetworkLibrary* lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  lib->AddNetworkManagerObserver(this);
}

NetworkChangeNotifierChromeos::~NetworkChangeNotifierChromeos() {
  chromeos::NetworkLibrary* lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  lib->RemoveNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);
}

void NetworkChangeNotifierChromeos::OnNetworkManagerChanged(
    chromeos::NetworkLibrary* cros) {
  UpdateNetworkState(cros);
}

bool NetworkChangeNotifierChromeos::IsCurrentlyOffline() const {
  return connectivity_state_ != chromeos::CONN_STATE_UNRESTRICTED;
}

void NetworkChangeNotifierChromeos::UpdateNetworkState(
    chromeos::NetworkLibrary* lib) {
  const chromeos::Network* network = lib->active_network();

  // Check if active network was added, removed or changed.
  if ((!network && has_active_network_) ||
      (network && (!has_active_network_ ||
                   network->service_path() != service_path_ ||
                   network->ip_address() != ip_address_))) {
    if (has_active_network_)
      lib->RemoveObserverForAllNetworks(this);
    if (!network) {
      has_active_network_ = false;
      service_path_.clear();
      ip_address_.clear();
    } else {
      has_active_network_ = true;
      service_path_ = network->service_path();
      lib->AddNetworkObserver(network->service_path(), this);
      ip_address_ = network->ip_address();
    }
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(
            &NetworkChangeNotifierChromeos::NotifyObserversOfIPAddressChange));
  }
}

void NetworkChangeNotifierChromeos::OnNetworkChanged(
    chromeos::NetworkLibrary* cros,
    const chromeos::Network* network) {
  if (!network) {
    NOTREACHED();
    return;
  }
  // Active network changed?
  if (network->service_path() != service_path_) {
    UpdateNetworkState(cros);
    return;
  }
  if (network->connectivity_state() != connectivity_state_) {
    connectivity_state_ = network->connectivity_state();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        method_factory_.NewRunnableMethod(
          &NetworkChangeNotifierChromeos::NotifyObserversOfOnlineStateChange));
  }
}

}  // namespace net
