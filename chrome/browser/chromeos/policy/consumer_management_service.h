// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_SERVICE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/dbus_method_call_status.h"

class PrefRegistrySimple;

namespace chromeos {
class CryptohomeClient;
}

namespace cryptohome {
class BaseReply;
}

namespace policy {

// The consumer management service handles the enrollment state, which is an
// enum value stored in local state to pass the information across reboots
// and between compoments, including settings page, sign-in screen, and user
// notification. It also handles the owner user ID stored in the boot lockbox.
class ConsumerManagementService {
 public:
  enum EnrollmentState {
    ENROLLMENT_NONE = 0,        // Not enrolled, or the enrollment is completed.
    ENROLLMENT_ENROLLING,       // Enrollment is in progress.
    ENROLLMENT_SUCCESS,         // Success. The notification is not sent yet.
    ENROLLMENT_CANCELED,        // Canceled by the user.
    ENROLLMENT_BOOT_LOCKBOX_FAILED,  // Failed to write to the boot lockbox.
    ENROLLMENT_DM_SERVER_FAILED,     // Failed to register the device.

    ENROLLMENT_LAST,            // This should always be the last one.
  };

  // GetOwner() invokes this with an argument set to the owner user ID,
  // or an empty string on failure.
  typedef base::Callback<void(const std::string&)> GetOwnerCallback;

  // SetOwner() invokes this with an argument indicating success or failure.
  typedef base::Callback<void(bool)> SetOwnerCallback;

  explicit ConsumerManagementService(chromeos::CryptohomeClient* client);

  virtual ~ConsumerManagementService();

  // Registers prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the enrollment state.
  EnrollmentState GetEnrollmentState() const;

  // Sets the enrollment state.
  void SetEnrollmentState(EnrollmentState state);

  // Returns the device owner stored in the boot lockbox via |callback|.
  void GetOwner(const GetOwnerCallback& callback);

  // Stores the device owner user ID into the boot lockbox and signs it.
  // |callback| is invoked with an agument indicating success or failure.
  void SetOwner(const std::string& user_id, const SetOwnerCallback& callback);

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
  base::WeakPtrFactory<ConsumerManagementService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConsumerManagementService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_SERVICE_H_
