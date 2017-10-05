// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_REGISTER_H_
#define DEVICE_U2F_U2F_REGISTER_H_

#include <vector>

#include "device/u2f/u2f_request.h"

namespace service_manager {
class Connector;
}

namespace device {

class U2fRegister : public U2fRequest {
 public:
  U2fRegister(const std::vector<uint8_t>& challenge_hash,
              const std::vector<uint8_t>& app_param,
              const ResponseCallback& cb,
              service_manager::Connector* connector);
  ~U2fRegister() override;

  static std::unique_ptr<U2fRequest> TryRegistration(
      const std::vector<uint8_t>& challenge_hash,
      const std::vector<uint8_t>& app_param,
      const ResponseCallback& cb,
      service_manager::Connector* connector);

 private:
  void TryDevice() override;
  void OnTryDevice(U2fReturnCode return_code,
                   const std::vector<uint8_t>& response_data);

  std::vector<uint8_t> challenge_hash_;
  std::vector<uint8_t> app_param_;
  base::WeakPtrFactory<U2fRegister> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_REGISTER_H_
