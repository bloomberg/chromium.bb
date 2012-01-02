// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_AUTO_ENROLLMENT_CLIENT_H_
#define CHROME_BROWSER_POLICY_AUTO_ENROLLMENT_CLIENT_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace policy {

class DeviceManagementService;

// Interacts with the device management service and determines whether this
// machine should automatically enter the Enterprise Enrollment screen during
// OOBE.
class AutoEnrollmentClient
    : public DeviceManagementBackend::DeviceAutoEnrollmentResponseDelegate {
 public:
  // |completion_callback| will be invoked on completion of the protocol, after
  // Start() is invoked.
  // Takes ownership of |device_management_service|.
  // |power_initial| and |power_limit| are exponents of power-of-2 values which
  // will be the initial modulus and the maximum modulus used by this client.
  AutoEnrollmentClient(const base::Closure& completion_callback,
                       DeviceManagementService* device_management_service,
                       const std::string& serial_number,
                       int power_initial,
                       int power_limit);
  virtual ~AutoEnrollmentClient();

  // Returns true if auto-enrollment is disabled in this device. In that case,
  // instances returned by Create() fail immediately once Start() is invoked.
  static bool IsDisabled();

  // Convenience method to create instances of this class.
  static AutoEnrollmentClient* Create(const base::Closure& completion_callback);

  // Starts the auto-enrollment check protocol with the device management
  // service. Subsequent calls drop any previous requests. Notice that this
  // call can invoke the |completion_callback_| if errors occur.
  void Start();

  // Returns true if the protocol completed successfully and determined that
  // this device should do enterprise enrollment.
  bool should_auto_enroll() const { return should_auto_enroll_; }

  // Returns the device_id randomly generated for the auto-enrollment requests.
  // It can be reused for subsequent requests to the device management service.
  std::string device_id() const { return device_id_; }

 private:
  // Sends an auto-enrollment check request to the device management service.
  // |power| is the power of the power-of-2 to use as a modulus for this
  // request.
  void SendRequest(int power);

  // Implementation of DeviceAutoEnrollmentResponseDelegate:
  virtual void HandleAutoEnrollmentResponse(
      const enterprise_management::DeviceAutoEnrollmentResponse&
          response) OVERRIDE;
  virtual void OnError(DeviceManagementBackend::ErrorCode code) OVERRIDE;

  // Returns true if |serial_number_hash_| is contained in |hashes|.
  bool IsSerialInProtobuf(
      const google::protobuf::RepeatedPtrField<std::string>& hashes);

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

  // Modulus used in the last request sent to the server.
  // Used to determine if the server is asking for the same modulus.
  int last_power_used_;

  // Used to communicate with the device management service.
  scoped_ptr<DeviceManagementService> device_management_service_;
  scoped_ptr<DeviceManagementBackend> device_management_backend_;

  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentClient);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_AUTO_ENROLLMENT_CLIENT_H_
