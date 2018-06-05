// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_OBSERVER_NOTIFIER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_OBSERVER_NOTIFIER_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
}

namespace chromeos {

namespace multidevice_setup {

// TODO(jordynass): Handle cases (2) and (3) below.

// Notifies the observer of MultiDeviceSetup for each of the following changes:
// (1) a potential host is found for someone who has not gone through the setup
//     flow before,
// (2) the host has switched for someone who has, or
// (3) a new Chromebook has been added to an account for someone who has.
class ObserverNotifier : public device_sync::DeviceSyncClient::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual std::unique_ptr<ObserverNotifier> BuildInstance(
        device_sync::DeviceSyncClient* device_sync_client,
        PrefService* pref_service,
        base::Clock* clock);

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~ObserverNotifier() override;

  void SetMultiDeviceSetupObserverPtr(
      mojom::MultiDeviceSetupObserverPtr observer_ptr);

 private:
  friend class ObserverNotifierTest;

  static const char kNewUserPotentialHostExists[];
  static const char kExistingUserHostSwitched[];
  static const char kExistingUserChromebookAdded[];

  ObserverNotifier(device_sync::DeviceSyncClient* device_sync_client,
                   PrefService* pref_service,
                   base::Clock* clock);

  // device_sync::DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;

  void CheckForEligibleDevices();

  void FlushForTesting();

  mojom::MultiDeviceSetupObserverPtr observer_ptr_;
  device_sync::DeviceSyncClient* device_sync_client_;
  PrefService* pref_service_;
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(ObserverNotifier);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_OBSERVER_NOTIFIER_H_
