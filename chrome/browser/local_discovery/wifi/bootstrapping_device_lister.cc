// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/wifi/bootstrapping_device_lister.h"

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"

namespace local_discovery {

namespace wifi {

namespace {

const char kPrivetSuffix[] = "prv";
// 3 for prv, 3 for type, one for connection status.
const size_t kPrivetCharactersAfterSeparator = 7;

const struct {
  const char* const short_name;
  const char* const long_name;
} kPrivetShortNames[] = {{"cam", "camera"}, {"pri", "printer"}};

const struct {
  char signifier;
  BootstrappingDeviceDescription::ConnectionStatus status;
} kPrivetConnectionStatuses[] = {
      {'C', BootstrappingDeviceDescription::CONNECTING},
      {'F', BootstrappingDeviceDescription::OFFLINE},
      {'L', BootstrappingDeviceDescription::LOCAL_ONLY},
      {'N', BootstrappingDeviceDescription::NOT_CONFIGURED},
      {'O', BootstrappingDeviceDescription::ONLINE},
};

const char kPrivetDeviceLongName[] = "device";

std::string ExpandDeviceKind(const std::string& device_kind_short) {
  for (size_t i = 0; i < arraysize(kPrivetShortNames); i++) {
    if (device_kind_short == kPrivetShortNames[i].short_name) {
      return kPrivetShortNames[i].long_name;
    }
  }
  return kPrivetDeviceLongName;
}

BootstrappingDeviceDescription::ConnectionStatus GetConnectionStatus(
    char signifier) {
  for (size_t i = 0; i < arraysize(kPrivetConnectionStatuses); i++) {
    if (signifier == kPrivetConnectionStatuses[i].signifier) {
      return kPrivetConnectionStatuses[i].status;
    }
  }

  LOG(WARNING) << "Unknown WiFi connection state signifier " << signifier;
  return BootstrappingDeviceDescription::NOT_CONFIGURED;
}

// Return true if the SSID is a privet ssid and fills |description| with its
// attributes.
bool ParsePrivetSSID(const std::string& internal_id,
                     const std::string& ssid,
                     BootstrappingDeviceDescription* description) {
  if (!EndsWith(ssid, kPrivetSuffix, true))
    return false;

  size_t at_pos = ssid.length() - kPrivetCharactersAfterSeparator - 1;

  if (ssid[at_pos] != '@' || ssid.find('@', at_pos + 1) != std::string::npos)
    return false;

  description->device_network_id = internal_id;
  description->device_ssid = ssid;

  description->device_name = ssid.substr(0, at_pos);
  description->device_kind = ExpandDeviceKind(ssid.substr(at_pos + 1, 3));
  description->connection_status = GetConnectionStatus(ssid.at(at_pos + 4));

  return true;
}

}  // namespace

BootstrappingDeviceDescription::BootstrappingDeviceDescription()
    : connection_status(NOT_CONFIGURED) {
}

BootstrappingDeviceDescription::~BootstrappingDeviceDescription() {
}

BootstrappingDeviceLister::BootstrappingDeviceLister(
    WifiManager* wifi_manager,
    const UpdateCallback& update_callback)
    : wifi_manager_(wifi_manager),
      update_callback_(update_callback),
      started_(false),
      weak_factory_(this) {
}

BootstrappingDeviceLister::~BootstrappingDeviceLister() {
  if (started_)
    wifi_manager_->RemoveNetworkListObserver(this);
}

void BootstrappingDeviceLister::Start() {
  DCHECK(!started_);

  started_ = true;

  wifi_manager_->AddNetworkListObserver(this);

  wifi_manager_->GetSSIDList(
      base::Bind(&BootstrappingDeviceLister::OnNetworkListChanged,
                 weak_factory_.GetWeakPtr()));
}

void BootstrappingDeviceLister::OnNetworkListChanged(
    const std::vector<NetworkProperties>& ssids) {
  ActiveDeviceList new_devices;

  for (size_t i = 0; i < ssids.size(); i++) {
    new_devices.push_back(make_pair(ssids[i].ssid, ssids[i].guid));
  }

  std::sort(new_devices.begin(), new_devices.end());

  base::WeakPtr<BootstrappingDeviceLister> weak_this =
      weak_factory_.GetWeakPtr();
  // Find new or changed SSIDs
  UpdateChangedSSIDs(true, new_devices, active_devices_);
  if (!weak_this)
    return;

  // Find removed SSIDs
  UpdateChangedSSIDs(false, active_devices_, new_devices);
  if (!weak_this)
    return;

  active_devices_.swap(new_devices);
}

void BootstrappingDeviceLister::UpdateChangedSSIDs(
    bool available,
    const ActiveDeviceList& changed,
    const ActiveDeviceList& original) {
  base::WeakPtr<BootstrappingDeviceLister> weak_this =
      weak_factory_.GetWeakPtr();

  ActiveDeviceList changed_devices =
    base::STLSetDifference<ActiveDeviceList>(changed, original);

  for (ActiveDeviceList::reverse_iterator i = changed_devices.rbegin();
       weak_this && i != changed_devices.rend();
       i++) {
    BootstrappingDeviceDescription description;
    if (ParsePrivetSSID(i->second, i->first, &description)) {
      update_callback_.Run(available, description);
    }
  }
}

}  // namespace wifi

}  // namespace local_discovery
