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
      connection_state_(chromeos::STATE_UNKNOWN) {

  chromeos::NetworkLibrary* lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  lib->AddNetworkManagerObserver(this);

  chromeos::PowerLibrary* power =
      chromeos::CrosLibrary::Get()->GetPowerLibrary();
  power->AddObserver(this);
}

NetworkChangeNotifierChromeos::~NetworkChangeNotifierChromeos() {
  chromeos::NetworkLibrary* lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  lib->RemoveNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);

  chromeos::PowerLibrary* power =
      chromeos::CrosLibrary::Get()->GetPowerLibrary();
  power->RemoveObserver(this);
}

void NetworkChangeNotifierChromeos::PowerChanged(PowerLibrary* obj) {
}

void NetworkChangeNotifierChromeos::SystemResumed() {
  // Force invalidation of various net resources on system resume.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(
          &NetworkChangeNotifier::NotifyObserversOfIPAddressChange));
}


void NetworkChangeNotifierChromeos::OnNetworkManagerChanged(
    chromeos::NetworkLibrary* cros) {
  UpdateNetworkState(cros);
}

bool NetworkChangeNotifierChromeos::IsCurrentlyOffline() const {
  return connection_state_ != chromeos::STATE_ONLINE;
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
      ip_address_ = network->ip_address();
      lib->AddNetworkObserver(network->service_path(), this);
    }
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(
            &NetworkChangeNotifier::NotifyObserversOfIPAddressChange));
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
  }

  // We don't care about all transitions of ConnectionState.  OnlineStateChange
  // notification should trigger if
  //   a) we were online and went offline
  //   b) we were offline and went online
  //   c) switched to/from captive portal
  chromeos::ConnectionState new_connection_state = network->connection_state();
  bool is_online = (new_connection_state == chromeos::STATE_ONLINE);
  bool was_online = (connection_state_ == chromeos::STATE_ONLINE);
  bool is_portal = (new_connection_state == chromeos::STATE_PORTAL);
  bool was_portal = (connection_state_ == chromeos::STATE_PORTAL);

  if (is_online != was_online || is_portal != was_portal) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(
           &NetworkChangeNotifierChromeos::NotifyObserversOfOnlineStateChange));
  }
  connection_state_ = new_connection_state;
}

}  // namespace net
