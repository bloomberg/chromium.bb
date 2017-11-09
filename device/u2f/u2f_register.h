// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_REGISTER_H_
#define DEVICE_U2F_U2F_REGISTER_H_

#include <memory>
#include <set>
#include <vector>
#include "device/u2f/u2f_request.h"

namespace device {

class U2fDiscovery;

class U2fRegister : public U2fRequest {
 public:
  U2fRegister(const std::vector<std::vector<uint8_t>>& registered_keys,
              const std::vector<uint8_t>& challenge_hash,
              const std::vector<uint8_t>& app_param,
              std::vector<std::unique_ptr<U2fDiscovery>> discoveries,
              const ResponseCallback& cb);
  ~U2fRegister() override;

  static std::unique_ptr<U2fRequest> TryRegistration(
      const std::vector<std::vector<uint8_t>>& registered_keys,
      const std::vector<uint8_t>& challenge_hash,
      const std::vector<uint8_t>& app_param,
      std::vector<std::unique_ptr<U2fDiscovery>> discoveries,
      const ResponseCallback& cb);

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

  std::vector<uint8_t> challenge_hash_;
  std::vector<uint8_t> app_param_;
  const std::vector<std::vector<uint8_t>> registered_keys_;
  // List of authenticators that did not create any of the key handles in the
  // exclude list.
  std::set<std::string> checked_device_id_list_;
  base::WeakPtrFactory<U2fRegister> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_REGISTER_H_
