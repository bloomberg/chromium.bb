// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/active_host.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/components/tether/pref_names.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "components/cryptauth/remote_device.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

ActiveHost::ActiveHost(TetherHostFetcher* tether_host_fetcher,
                       PrefService* pref_service)
    : tether_host_fetcher_(tether_host_fetcher),
      pref_service_(pref_service),
      weak_ptr_factory_(this) {}

ActiveHost::~ActiveHost() {}

// static
void ActiveHost::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kActiveHostStatus,
      static_cast<int>(ActiveHostStatus::DISCONNECTED));
  registry->RegisterStringPref(prefs::kActiveHostDeviceId, "");
  registry->RegisterStringPref(prefs::kWifiNetworkId, "");
}

void ActiveHost::SetActiveHostDisconnected() {
  SetActiveHost(ActiveHostStatus::DISCONNECTED, "" /* active_host_device_id */,
                "" /* wifi_network_id */);
}

void ActiveHost::SetActiveHostConnecting(
    const std::string& active_host_device_id) {
  DCHECK(!active_host_device_id.empty());

  SetActiveHost(ActiveHostStatus::CONNECTING, active_host_device_id,
                "" /* wifi_network_id */);
}

void ActiveHost::SetActiveHostConnected(
    const std::string& active_host_device_id,
    const std::string& wifi_network_id) {
  DCHECK(!active_host_device_id.empty());
  DCHECK(!wifi_network_id.empty());

  SetActiveHost(ActiveHostStatus::CONNECTED, active_host_device_id,
                wifi_network_id);
}

void ActiveHost::GetActiveHost(const ActiveHostCallback& active_host_callback) {
  ActiveHostStatus status = GetActiveHostStatus();

  if (status == ActiveHostStatus::DISCONNECTED) {
    active_host_callback.Run(status, nullptr /* active_host */,
                             "" /* wifi_network_id */);
    return;
  }

  std::string active_host_device_id = GetActiveHostDeviceId();
  DCHECK(!active_host_device_id.empty());

  tether_host_fetcher_->FetchTetherHost(
      active_host_device_id,
      base::Bind(&ActiveHost::OnTetherHostFetched,
                 weak_ptr_factory_.GetWeakPtr(), active_host_callback));
}

ActiveHost::ActiveHostStatus ActiveHost::GetActiveHostStatus() const {
  return static_cast<ActiveHostStatus>(
      pref_service_->GetInteger(prefs::kActiveHostStatus));
}

std::string ActiveHost::GetActiveHostDeviceId() const {
  return pref_service_->GetString(prefs::kActiveHostDeviceId);
}

std::string ActiveHost::GetWifiNetworkId() const {
  return pref_service_->GetString(prefs::kWifiNetworkId);
}

void ActiveHost::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ActiveHost::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ActiveHost::SetActiveHost(ActiveHostStatus active_host_status,
                               const std::string& active_host_device_id,
                               const std::string& wifi_network_id) {
  bool status_changed = GetActiveHostStatus() != active_host_status;
  bool device_changed = GetActiveHostDeviceId() != active_host_device_id;
  bool network_id_changed = GetWifiNetworkId() != wifi_network_id;

  if (!status_changed && !device_changed && !network_id_changed) {
    // If nothing has changed, return early.
    return;
  }

  pref_service_->Set(prefs::kActiveHostStatus,
                     base::Value(static_cast<int>(active_host_status)));
  pref_service_->Set(prefs::kActiveHostDeviceId,
                     base::Value(active_host_device_id));
  pref_service_->Set(prefs::kWifiNetworkId, base::Value(wifi_network_id));

  // Now, send an active host changed update.
  GetActiveHost(base::Bind(&ActiveHost::SendActiveHostChangedUpdate,
                           weak_ptr_factory_.GetWeakPtr()));
}

void ActiveHost::OnTetherHostFetched(
    const ActiveHostCallback& active_host_callback,
    std::unique_ptr<cryptauth::RemoteDevice> remote_device) {
  if (GetActiveHostDeviceId().empty() || !remote_device) {
    DCHECK(GetActiveHostStatus() == ActiveHostStatus::DISCONNECTED);
    DCHECK(GetWifiNetworkId().empty());

    // If the active host became disconnected while the tether host was being
    // fetched, forward this information to the callback.
    active_host_callback.Run(ActiveHostStatus::DISCONNECTED,
                             nullptr /* active_host */,
                             "" /* wifi_network_id */);
    return;
  }

  if (GetActiveHostDeviceId() != remote_device->GetDeviceId()) {
    // If the active host has changed while the tether host was being fetched,
    // perform the fetch again.
    GetActiveHost(active_host_callback);
    return;
  }

  if (GetActiveHostStatus() == ActiveHostStatus::CONNECTING) {
    DCHECK(GetWifiNetworkId().empty());
    active_host_callback.Run(ActiveHostStatus::CONNECTING,
                             std::move(remote_device),
                             "" /* wifi_network_id */);
    return;
  }

  DCHECK(GetActiveHostStatus() == ActiveHostStatus::CONNECTED);
  DCHECK(!GetWifiNetworkId().empty());
  active_host_callback.Run(ActiveHostStatus::CONNECTED,
                           std::move(remote_device), GetWifiNetworkId());
}

void ActiveHost::SendActiveHostChangedUpdate(
    ActiveHostStatus active_host_status,
    std::unique_ptr<cryptauth::RemoteDevice> active_host,
    const std::string& wifi_network_id) {
  for (auto& observer : observer_list_) {
    std::unique_ptr<cryptauth::RemoteDevice> unique_remote_device =
        active_host ? base::MakeUnique<cryptauth::RemoteDevice>(*active_host)
                    : nullptr;
    observer.OnActiveHostChanged(
        active_host_status, std::move(unique_remote_device), wifi_network_id);
  }
}

}  // namespace tether

}  // namespace chromeos
