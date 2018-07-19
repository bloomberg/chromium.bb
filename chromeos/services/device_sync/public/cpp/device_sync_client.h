// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "components/cryptauth/remote_device_ref.h"

namespace chromeos {

namespace device_sync {

// Provides clients access to the DeviceSync API.
class DeviceSyncClient {
 public:
  class Observer {
   public:
    // Called when the DeviceSyncClient is ready, i.e. local device metadata
    // and synced devices are available.
    virtual void OnReady() {}

    // OnEnrollmentFinished() and OnNewDevicesSynced() will only be called once
    // DeviceSyncClient is ready, i.e., OnReady() will always be the first
    // callback called.
    virtual void OnEnrollmentFinished() {}
    virtual void OnNewDevicesSynced() {}

   protected:
    virtual ~Observer() = default;
  };

  using FindEligibleDevicesCallback =
      base::OnceCallback<void(mojom::NetworkRequestResult,
                              cryptauth::RemoteDeviceRefList,
                              cryptauth::RemoteDeviceRefList)>;

  DeviceSyncClient();
  virtual ~DeviceSyncClient();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Clients of DeviceSyncClient should ensure that this returns true before
  // calling any methods. If false, clients should listen on and wait for the
  // OnReady() callback.
  bool is_ready() { return is_ready_; }

  virtual void ForceEnrollmentNow(
      mojom::DeviceSync::ForceEnrollmentNowCallback callback) = 0;
  virtual void ForceSyncNow(
      mojom::DeviceSync::ForceSyncNowCallback callback) = 0;
  virtual cryptauth::RemoteDeviceRefList GetSyncedDevices() = 0;
  virtual base::Optional<cryptauth::RemoteDeviceRef>
  GetLocalDeviceMetadata() = 0;

  // Note: In the special case of passing |software_feature| =
  // SoftwareFeature::EASY_UNLOCK_HOST and |enabled| = false, |public_key| is
  // ignored.
  virtual void SetSoftwareFeatureState(
      const std::string public_key,
      cryptauth::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      mojom::DeviceSync::SetSoftwareFeatureStateCallback callback) = 0;
  virtual void FindEligibleDevices(cryptauth::SoftwareFeature software_feature,
                                   FindEligibleDevicesCallback callback) = 0;
  virtual void GetDebugInfo(
      mojom::DeviceSync::GetDebugInfoCallback callback) = 0;

 protected:
  void NotifyReady();
  void NotifyEnrollmentFinished();
  void NotifyNewDevicesSynced();

 private:
  bool is_ready_ = false;
  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncClient);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_H_
