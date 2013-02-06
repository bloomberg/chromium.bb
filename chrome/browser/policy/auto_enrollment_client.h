// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_AUTO_ENROLLMENT_CLIENT_H_
#define CHROME_BROWSER_POLICY_AUTO_ENROLLMENT_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

class PrefService;
class PrefRegistrySimple;

namespace enterprise_management {
class DeviceManagementResponse;
}

namespace policy {

class DeviceManagementRequestJob;
class DeviceManagementService;

// Interacts with the device management service and determines whether this
// machine should automatically enter the Enterprise Enrollment screen during
// OOBE.
class AutoEnrollmentClient {
 public:
  // |completion_callback| will be invoked on completion of the protocol, after
  // Start() is invoked.
  // Takes ownership of |device_management_service|.
  // The result of the protocol will be cached in |local_state|.
  // |power_initial| and |power_limit| are exponents of power-of-2 values which
  // will be the initial modulus and the maximum modulus used by this client.
  AutoEnrollmentClient(const base::Closure& completion_callback,
                       DeviceManagementService* device_management_service,
                       PrefService* local_state,
                       const std::string& serial_number,
                       int power_initial,
                       int power_limit);
  virtual ~AutoEnrollmentClient();

  // Registers preferences in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns true if auto-enrollment is disabled in this device. In that case,
  // instances returned by Create() fail immediately once Start() is invoked.
  static bool IsDisabled();

  // Convenience method to create instances of this class.
  static AutoEnrollmentClient* Create(const base::Closure& completion_callback);

  // Cancels auto-enrollment.
  // This function does not interrupt a running auto-enrollment check. It only
  // stores a pref in |local_state| that prevents the client from entering
  // auto-enrollment mode for the future.
  static void CancelAutoEnrollment();

  // Starts the auto-enrollment check protocol with the device management
  // service. Subsequent calls drop any previous requests. Notice that this
  // call can invoke the |completion_callback_| if errors occur.
  void Start();

  // Cancels any pending requests. |completion_callback_| will not be invoked.
  // |this| will delete itself.
  void CancelAndDeleteSoon();

  // Returns true if the protocol completed successfully and determined that
  // this device should do enterprise enrollment.
  bool should_auto_enroll() const { return should_auto_enroll_; }

  // Returns the device_id randomly generated for the auto-enrollment requests.
  // It can be reused for subsequent requests to the device management service.
  std::string device_id() const { return device_id_; }

 private:
  // Tries to load the result of a previous execution of the protocol from
  // local state. Returns true if that decision has been made and is valid.
  bool GetCachedDecision();

  // Sends an auto-enrollment check request to the device management service.
  // |power| is the power of the power-of-2 to use as a modulus for this
  // request.
  void SendRequest(int power);

  // Handles auto-enrollment request completion.
  void OnRequestCompletion(
      DeviceManagementStatus status,
      const enterprise_management::DeviceManagementResponse& response);

  // Returns true if |serial_number_hash_| is contained in |hashes|.
  bool IsSerialInProtobuf(
      const google::protobuf::RepeatedPtrField<std::string>& hashes);

  // Invoked when the protocol completes. This invokes the callback and records
  // some UMA metrics.
  void OnProtocolDone();

  // Callback to invoke when the protocol completes.
  base::Closure completion_callback_;

  // Whether to auto-enroll or not. This is reset by calls to Start(), and only
  // turns true if the protocol and the serial number check succeed.
  bool should_auto_enroll_;

  // Randomly generated device id for the auto-enrollment requests.
  std::string device_id_;

  // SHA256 hash of the device's serial number. Empty if the serial couldn't be
  // retrieved.
  std::string serial_number_hash_;

  // Power of the power-of-2 modulus used in the initial auto-enrollment
  // request.
  int power_initial_;

  // Power of the maximum power-of-2 modulus that this client will accept from
  // a retry response from the server.
  int power_limit_;

  // Number of requests sent to the server so far.
  // Used to determine if the server keeps asking for different moduli.
  int requests_sent_;

  // Used to communicate with the device management service.
  scoped_ptr<DeviceManagementService> device_management_service_;
  scoped_ptr<DeviceManagementRequestJob> request_job_;

  // PrefService where the protocol's results are cached.
  PrefService* local_state_;

  // Times used to determine the duration of the protocol, and the extra time
  // needed to complete after the signin was complete.
  // If |time_start_| is not null, the protocol is still running.
  // If |time_extra_start_| is not null, the protocol is still running but our
  // owner has relinquished ownership.
  base::Time time_start_;
  base::Time time_extra_start_;

  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentClient);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_AUTO_ENROLLMENT_CLIENT_H_
