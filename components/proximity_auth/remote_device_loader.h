// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_REMOTE_DEVICE_LOADER_H
#define COMPONENTS_PROXIMITY_REMOTE_DEVICE_LOADER_H

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"
#include "components/proximity_auth/remote_device.h"

namespace proximity_auth {

class ProximityAuthPrefManager;
class SecureMessageDelegate;

// Loads a collection of RemoteDevice objects from the given ExternalDeviceInfo
// protos that were synced from CryptAuth. We need to derive the PSK, which is
// a symmetric key used to authenticate each remote device.
class RemoteDeviceLoader {
 public:
  // Creates the instance:
  // |unlock_keys|: The unlock keys previously synced from CryptAuth.
  // |user_private_key|: The private key of the user's local device. Used to
  //                     derive the PSK.
  // |secure_message_delegate|: Used to derive each persistent symmetric key.
  // |pref_manager|: Used to retrieve the Bluetooth address of BLE devices.
  RemoteDeviceLoader(
      const std::vector<cryptauth::ExternalDeviceInfo>& unlock_keys,
      const std::string& user_id,
      const std::string& user_private_key,
      std::unique_ptr<SecureMessageDelegate> secure_message_delegate,
      ProximityAuthPrefManager* pref_manager);

  ~RemoteDeviceLoader();

  // Loads the RemoteDevice objects. |callback| will be invoked upon completion.
  typedef base::Callback<void(const RemoteDeviceList&)> RemoteDeviceCallback;
  void Load(const RemoteDeviceCallback& callback);

 private:
  // Called when the PSK is derived for each unlock key. If the PSK for all
  // unlock have been derived, then we can invoke |callback_|.
  void OnPSKDerived(const cryptauth::ExternalDeviceInfo& unlock_key,
                    const std::string& psk);

  // The remaining unlock keys whose PSK we're waiting on.
  std::vector<cryptauth::ExternalDeviceInfo> remaining_unlock_keys_;

  // The id of the user who the remote devices belong to.
  const std::string user_id_;

  // The private key of the user's local device.
  const std::string user_private_key_;

  // Performs the PSK key derivation.
  std::unique_ptr<SecureMessageDelegate> secure_message_delegate_;

  // Used to retrieve the address for BLE devices. Not owned.
  ProximityAuthPrefManager* pref_manager_;

  // Invoked when the RemoteDevices are loaded.
  RemoteDeviceCallback callback_;

  // The collection of RemoteDevices to return.
  RemoteDeviceList remote_devices_;

  base::WeakPtrFactory<RemoteDeviceLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteDeviceLoader);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_REMOTE_DEVICE_LOADER_H
