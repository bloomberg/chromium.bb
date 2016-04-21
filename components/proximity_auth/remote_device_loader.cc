// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/remote_device_loader.h"

#include <algorithm>
#include <utility>

#include "base/base64url.h"
#include "base/bind.h"
#include "components/proximity_auth/cryptauth/secure_message_delegate.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/proximity_auth_pref_manager.h"

namespace proximity_auth {

RemoteDeviceLoader::RemoteDeviceLoader(
    const std::vector<cryptauth::ExternalDeviceInfo>& unlock_keys,
    const std::string& user_id,
    const std::string& user_private_key,
    std::unique_ptr<SecureMessageDelegate> secure_message_delegate,
    ProximityAuthPrefManager* pref_manager)
    : remaining_unlock_keys_(unlock_keys),
      user_id_(user_id),
      user_private_key_(user_private_key),
      secure_message_delegate_(std::move(secure_message_delegate)),
      pref_manager_(pref_manager),
      weak_ptr_factory_(this) {}

RemoteDeviceLoader::~RemoteDeviceLoader() {}

void RemoteDeviceLoader::Load(const RemoteDeviceCallback& callback) {
  DCHECK(callback_.is_null());
  callback_ = callback;
  PA_LOG(INFO) << "Loading " << remaining_unlock_keys_.size()
               << " remote devices";

  if (remaining_unlock_keys_.empty()) {
    callback_.Run(remote_devices_);
    return;
  }

  std::vector<cryptauth::ExternalDeviceInfo> all_unlock_keys =
      remaining_unlock_keys_;

  for (const auto& unlock_key : all_unlock_keys) {
    secure_message_delegate_->DeriveKey(
        user_private_key_, unlock_key.public_key(),
        base::Bind(&RemoteDeviceLoader::OnPSKDerived,
                   weak_ptr_factory_.GetWeakPtr(), unlock_key));
  }
}

void RemoteDeviceLoader::OnPSKDerived(
    const cryptauth::ExternalDeviceInfo& unlock_key,
    const std::string& psk) {
  std::string public_key = unlock_key.public_key();
  auto iterator = std::find_if(
      remaining_unlock_keys_.begin(), remaining_unlock_keys_.end(),
      [&public_key](const cryptauth::ExternalDeviceInfo& unlock_key) {
        return unlock_key.public_key() == public_key;
      });

  DCHECK(iterator != remaining_unlock_keys_.end());
  remaining_unlock_keys_.erase(iterator);
  PA_LOG(INFO) << "Derived PSK for " << unlock_key.friendly_device_name()
               << ", " << remaining_unlock_keys_.size() << " keys remaining.";

  // TODO(tengs): We assume that devices without a |bluetooth_address| field are
  // BLE devices. Ideally, we should have a separate field for this information.
  RemoteDevice::BluetoothType bluetooth_type =
      unlock_key.bluetooth_address().empty() ? RemoteDevice::BLUETOOTH_LE
                                             : RemoteDevice::BLUETOOTH_CLASSIC;

  std::string bluetooth_address = unlock_key.bluetooth_address();
  if (bluetooth_address.empty() && pref_manager_) {
    std::string b64_public_key;
    base::Base64UrlEncode(unlock_key.public_key(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &b64_public_key);
    bluetooth_address = pref_manager_->GetDeviceAddress(b64_public_key);
    PA_LOG(INFO) << "The BLE address of " << unlock_key.friendly_device_name()
                 << " is " << bluetooth_address;
  }

  remote_devices_.push_back(RemoteDevice(
      user_id_, unlock_key.friendly_device_name(), unlock_key.public_key(),
      bluetooth_type, bluetooth_address, psk, std::string()));

  if (!remaining_unlock_keys_.size())
    callback_.Run(remote_devices_);
}

}  // namespace proximity_auth
