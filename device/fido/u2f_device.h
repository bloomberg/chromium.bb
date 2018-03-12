// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_U2F_DEVICE_H_
#define DEVICE_FIDO_U2F_DEVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/u2f_apdu_response.h"
#include "device/fido/u2f_return_code.h"

namespace device {

// Device abstraction for an individual U2F device. A U2F device defines the
// standardized Register, Sign, and GetVersion methods.
class U2fDevice {
 public:
  enum class ProtocolVersion {
    U2F_V2,
    UNKNOWN,
  };

  using MessageCallback =
      base::OnceCallback<void(U2fReturnCode, const std::vector<uint8_t>&)>;
  using VersionCallback =
      base::OnceCallback<void(bool success, ProtocolVersion version)>;
  using DeviceCallback =
      base::OnceCallback<void(bool success,
                              std::unique_ptr<U2fApduResponse> response)>;
  using WinkCallback = base::OnceCallback<void()>;

  static constexpr auto kDeviceTimeout = base::TimeDelta::FromSeconds(3);

  U2fDevice();
  virtual ~U2fDevice();

  // TODO(hongjunchoi): https://crbug.com/810229 Move all encoding logic from
  // U2fDevice to U2fRequest.
  // Raw messages parameters are defined by the specification at
  // https://fidoalliance.org/specs/fido-u2f-v1.0-nfc-bt-amendment-20150514/fido-u2f-raw-message-formats.html
  void Register(const std::vector<uint8_t>& appid_digest,
                const std::vector<uint8_t>& challenge_digest,
                bool individual_attestation_ok,
                MessageCallback callback);
  void Version(VersionCallback callback);
  void Sign(const std::vector<uint8_t>& appid_digest,
            const std::vector<uint8_t>& challenge_digest,
            const std::vector<uint8_t>& key_handle,
            MessageCallback callback,
            bool check_only = false);

  virtual void TryWink(WinkCallback callback) = 0;
  virtual std::string GetId() const = 0;

 protected:
  // Pure virtual function defined by each device type, implementing
  // the device communication transaction.
  virtual void DeviceTransact(std::vector<uint8_t> command,
                              DeviceCallback callback) = 0;
  virtual base::WeakPtr<U2fDevice> GetWeakPtr() = 0;

 private:
  void OnRegisterComplete(MessageCallback callback,
                          bool success,
                          std::unique_ptr<U2fApduResponse> register_response);
  void OnSignComplete(MessageCallback callback,
                      bool success,
                      std::unique_ptr<U2fApduResponse> sign_response);
  void OnVersionComplete(VersionCallback callback,
                         bool legacy,
                         bool success,
                         std::unique_ptr<U2fApduResponse> version_response);

  DISALLOW_COPY_AND_ASSIGN(U2fDevice);
};

}  // namespace device

#endif  // DEVICE_FIDO_U2F_DEVICE_H_
