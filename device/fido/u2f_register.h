// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_U2F_REGISTER_H_
#define DEVICE_FIDO_U2F_REGISTER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "device/fido/u2f_request.h"
#include "device/fido/u2f_transport_protocol.h"

namespace service_manager {
class Connector;
};

namespace device {

class RegisterResponseData;

class U2fRegister : public U2fRequest {
 public:
  using RegisterResponseCallback = base::OnceCallback<void(
      U2fReturnCode status_code,
      base::Optional<RegisterResponseData> response_data)>;

  static std::unique_ptr<U2fRequest> TryRegistration(
      service_manager::Connector* connector,
      const base::flat_set<U2fTransportProtocol>& protocols,
      std::vector<std::vector<uint8_t>> registered_keys,
      std::vector<uint8_t> challenge_digest,
      std::vector<uint8_t> application_parameter,
      bool individual_attestation_ok,
      RegisterResponseCallback completion_callback);

  U2fRegister(service_manager::Connector* connector,
              const base::flat_set<U2fTransportProtocol>& protocols,
              std::vector<std::vector<uint8_t>> registered_keys,
              std::vector<uint8_t> challenge_digest,
              std::vector<uint8_t> application_parameter,
              bool individual_attestation_ok,
              RegisterResponseCallback completion_callback);
  ~U2fRegister() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(U2fRegisterTest, TestCreateU2fRegisterCommand);

  void TryDevice() override;
  void OnTryDevice(bool is_duplicate_registration,
                   U2fReturnCode return_code,
                   const std::vector<uint8_t>& response_data);

  // Callback function called when non-empty exclude list was provided. This
  // function iterates through all key handles in |registered_keys_| for all
  // devices and checks for already registered keys.
  void OnTryCheckRegistration(
      std::vector<std::vector<uint8_t>>::const_iterator it,
      U2fReturnCode return_code,
      const std::vector<uint8_t>& response_data);
  // Function handling registration flow after all devices were checked for
  // already registered keys.
  void CompleteNewDeviceRegistration();
  // Returns whether |current_device_| has been checked for duplicate
  // registration for all key handles provided in |registered_keys_|.
  bool CheckedForDuplicateRegistration();

  // Indicates whether the token should be signaled that using an individual
  // attestation certificate is acceptable.
  const bool individual_attestation_ok_;
  RegisterResponseCallback completion_callback_;

  // List of authenticators that did not create any of the key handles in the
  // exclude list.
  std::set<std::string> checked_device_id_list_;
  base::WeakPtrFactory<U2fRegister> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_FIDO_U2F_REGISTER_H_
