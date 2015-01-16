// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_SERVICE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/policy/consumer_management_stage.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/dbus/dbus_method_call_status.h"

class PrefRegistrySimple;

namespace chromeos {
class CryptohomeClient;
}

namespace cryptohome {
class BaseReply;
}

namespace policy {

// The consumer management service handles several things:
//
// 1. The consumer management status: The consumer management status is an enum
//    indicating if the device is consumer-managed and if enrollment or un-
//    enrollment is in progress. The service can be observed and the observers
//    will be notified when the status is changed. Note that the observers may
//    be notified even when the status is NOT changed. The observers need to
//    check the status upon receiving the notification.
//
// 2. The consumer management stage: The consumer management stage is a value
//    indicating the enrollment or the unenrollment process, stored in local
//    state to pass the information across reboots and between components,
//    including settings page, sign-in screen, and user notification.
//
// 3. Boot lockbox owner ID: Unlike the owner ID in CrosSettings, the owner ID
//    stored in the boot lockbox can only be modified after reboot and before
//    the first session starts. It is guaranteed that if the device is consumer
//    managed, the owner ID in the boot lockbox will be available, but not the
//    other way.
class ConsumerManagementService
    : public chromeos::DeviceSettingsService::Observer {
 public:
  // The status indicates if the device is enrolled, or if enrollment or
  // unenrollment is in progress. If you want to add a value here, please also
  // update |kStatusString| in the .cc file, and |ConsumerManagementStatus| in
  // chrome/browser/resources/options/chromeos/consumer_management_overlay.js
  enum Status {
    // The status is currently unavailable.
    STATUS_UNKNOWN = 0,

    STATUS_ENROLLED,
    STATUS_ENROLLING,
    STATUS_UNENROLLED,
    STATUS_UNENROLLING,

    // This should always be the last one.
    STATUS_LAST,
  };

  class Observer {
   public:
    // Called when the status changes.
    virtual void OnConsumerManagementStatusChanged() = 0;
  };

  // GetOwner() invokes this with an argument set to the owner user ID,
  // or an empty string on failure.
  typedef base::Callback<void(const std::string&)> GetOwnerCallback;

  // SetOwner() invokes this with an argument indicating success or failure.
  typedef base::Callback<void(bool)> SetOwnerCallback;

  // |client| and |device_settings_service| should outlive this object.
  ConsumerManagementService(
      chromeos::CryptohomeClient* client,
      chromeos::DeviceSettingsService* device_settings_service);

  ~ConsumerManagementService() override;

  // Registers prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the status.
  virtual Status GetStatus() const;

  // Returns the string value of the status.
  std::string GetStatusString() const;

  // Returns the stage.
  virtual ConsumerManagementStage GetStage() const;

  // Sets the stage.
  virtual void SetStage(const ConsumerManagementStage& stage);

  // Returns the device owner stored in the boot lockbox via |callback|.
  void GetOwner(const GetOwnerCallback& callback);

  // Stores the device owner user ID into the boot lockbox and signs it.
  // |callback| is invoked with an agument indicating success or failure.
  void SetOwner(const std::string& user_id, const SetOwnerCallback& callback);

  // chromeos::DeviceSettingsService::Observer:
  void OwnershipStatusChanged() override;
  void DeviceSettingsUpdated() override;
  void OnDeviceSettingsServiceShutdown() override;

 protected:
  void NotifyStatusChanged();

 private:
  void OnGetBootAttributeDone(
      const GetOwnerCallback& callback,
      chromeos::DBusMethodCallStatus call_status,
      bool dbus_success,
      const cryptohome::BaseReply& reply);

  void OnSetBootAttributeDone(const SetOwnerCallback& callback,
                              chromeos::DBusMethodCallStatus call_status,
                              bool dbus_success,
                              const cryptohome::BaseReply& reply);

  void OnFlushAndSignBootAttributesDone(
      const SetOwnerCallback& callback,
      chromeos::DBusMethodCallStatus call_status,
      bool dbus_success,
      const cryptohome::BaseReply& reply);

  chromeos::CryptohomeClient* client_;
  chromeos::DeviceSettingsService* device_settings_service_;

  ObserverList<Observer, true> observers_;
  base::WeakPtrFactory<ConsumerManagementService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConsumerManagementService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_SERVICE_H_
