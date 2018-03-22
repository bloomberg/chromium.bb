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
#include "base/memory/scoped_refptr.h"
#include "device/fido/fido_device.h"

namespace crypto {
class ECPrivateKey;
}  // namespace crypto

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) VirtualU2fDevice : public FidoDevice {
 public:
  // Stores the state of the device. Since |U2fDevice| objects only persist for
  // the lifetime of a single request, keeping state in an external object is
  // neccessary in order to provide continuity between requests.
  class COMPONENT_EXPORT(DEVICE_FIDO) State : public base::RefCounted<State> {
   public:
    State();

    // The common name in the attestation certificate.
    std::string attestation_cert_common_name;

    // The common name in the attestation certificate if individual attestation
    // is requested.
    std::string individual_attestation_cert_common_name;

   private:
    friend class ::device::VirtualU2fDevice;
    friend class base::RefCounted<State>;
    ~State();

    struct Internal;
    std::unique_ptr<Internal> internal_;
  };

  // Constructs an object with ephemeral state. In order to have the state of
  // the device persist between operations, use the constructor that takes a
  // scoped_refptr<State>.
  VirtualU2fDevice();

  // Constructs an object that will read from, and write to, |state|.
  explicit VirtualU2fDevice(scoped_refptr<State> state);

  ~VirtualU2fDevice() override;

  void AddRegistration(std::vector<uint8_t> key_handle,
                       std::unique_ptr<crypto::ECPrivateKey> private_key,
                       std::vector<uint8_t> application_parameter,
                       uint32_t counter);

 protected:
  // U2fDevice:
  void TryWink(WinkCallback cb) override;
  std::string GetId() const override;
  void DeviceTransact(std::vector<uint8_t> command, DeviceCallback cb) override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

 private:
  base::Optional<std::vector<uint8_t>> DoRegister(
      uint8_t ins,
      uint8_t p1,
      uint8_t p2,
      base::span<const uint8_t> data);

  base::Optional<std::vector<uint8_t>> DoSign(uint8_t ins,
                                              uint8_t p1,
                                              uint8_t p2,
                                              base::span<const uint8_t> data);

  scoped_refptr<State> state_;
  base::WeakPtrFactory<FidoDevice> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VirtualU2fDevice);
};

}  // namespace device

#endif  // DEVICE_FIDO_VIRTUAL_U2F_DEVICE_H_
