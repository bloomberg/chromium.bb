// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_REGISTER_H_
#define DEVICE_U2F_U2F_REGISTER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/optional.h"
#include "device/u2f/u2f_request.h"

namespace device {

class RegisterResponseData;
class U2fDiscovery;

class U2fRegister : public U2fRequest {
 public:
  using RegisterResponseCallback = base::OnceCallback<void(
      U2fReturnCode status_code,
      base::Optional<RegisterResponseData> response_data)>;

  U2fRegister(std::string relying_party_id,
              std::vector<U2fDiscovery*> discoveries,
              const std::vector<std::vector<uint8_t>>& registered_keys,
              const std::vector<uint8_t>& challenge_hash,
              const std::vector<uint8_t>& app_param,
              RegisterResponseCallback completion_callback);
  ~U2fRegister() override;

  static std::unique_ptr<U2fRequest> TryRegistration(
      std::string relying_party_id,
      std::vector<U2fDiscovery*> discoveries,
      const std::vector<std::vector<uint8_t>>& registered_keys,
      const std::vector<uint8_t>& challenge_hash,
      const std::vector<uint8_t>& app_param,
      RegisterResponseCallback completion_callback);

 private:
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

  const std::vector<std::vector<uint8_t>> registered_keys_;
  std::vector<uint8_t> challenge_hash_;
  std::vector<uint8_t> app_param_;
  RegisterResponseCallback completion_callback_;

  // List of authenticators that did not create any of the key handles in the
  // exclude list.
  std::set<std::string> checked_device_id_list_;
  base::WeakPtrFactory<U2fRegister> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_REGISTER_H_
