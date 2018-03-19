// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_VIRTUAL_U2F_DEVICE_H_
#define DEVICE_FIDO_VIRTUAL_U2F_DEVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "device/fido/fido_device.h"

namespace crypto {
class ECPrivateKey;
}  // namespace crypto

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) VirtualU2fDevice : public FidoDevice {
 public:
  VirtualU2fDevice();
  ~VirtualU2fDevice() override;

  void AddRegistration(std::vector<uint8_t> key_handle,
                       std::unique_ptr<crypto::ECPrivateKey> private_key,
                       std::vector<uint8_t> app_id_hash,
                       uint32_t counter);

 protected:
  // U2fDevice:
  void TryWink(WinkCallback cb) override;
  std::string GetId() const override;
  void DeviceTransact(std::vector<uint8_t> command, DeviceCallback cb) override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

 private:
  struct RegistrationData {
    RegistrationData();
    RegistrationData(std::unique_ptr<crypto::ECPrivateKey> private_key,
                     std::vector<uint8_t> app_id_hash,
                     uint32_t counter);
    RegistrationData(RegistrationData&& data);

    RegistrationData& operator=(RegistrationData&& other);
    ~RegistrationData();

    std::unique_ptr<crypto::ECPrivateKey> private_key;
    std::vector<uint8_t> app_id_hash;
    uint32_t counter = 0;

    DISALLOW_COPY_AND_ASSIGN(RegistrationData);
  };

  base::Optional<std::vector<uint8_t>> DoRegister(
      uint8_t ins,
      uint8_t p1,
      uint8_t p2,
      base::span<const uint8_t> data);

  base::Optional<std::vector<uint8_t>> DoSign(uint8_t ins,
                                              uint8_t p1,
                                              uint8_t p2,
                                              base::span<const uint8_t> data);

  // TODO(agl): this state is in the wrong place: U2fDevice objects are
  // ephemeral so to maintain state across requests this will have to be kept
  // elsewhere.
  // Keyed on appId/rpId hash (aka "applicationParam")
  std::map<std::vector<uint8_t>, RegistrationData> registrations_;
  base::WeakPtrFactory<FidoDevice> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VirtualU2fDevice);
};

}  // namespace device

#endif  // DEVICE_FIDO_VIRTUAL_U2F_DEVICE_H_
