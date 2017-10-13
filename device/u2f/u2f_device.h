// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_DEVICE_H_
#define DEVICE_U2F_U2F_DEVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/u2f/u2f_apdu_response.h"
#include "device/u2f/u2f_return_code.h"

namespace device {

class U2fApduCommand;

// Device abstraction for an individual U2F device. A U2F device defines the
// standardized Register, Sign, and GetVersion methods.
class U2fDevice {
 public:
  enum class ProtocolVersion {
    U2F_V2,
    UNKNOWN,
  };

  using MessageCallback =
      base::Callback<void(U2fReturnCode, const std::vector<uint8_t>&)>;
  using VersionCallback =
      base::Callback<void(bool success, ProtocolVersion version)>;
  using DeviceCallback =
      base::Callback<void(bool success,
                          std::unique_ptr<U2fApduResponse> response)>;
  using WinkCallback = base::Callback<void()>;

  virtual ~U2fDevice();

  // Raw messages parameters are defined by the specification at
  // https://fidoalliance.org/specs/fido-u2f-v1.0-nfc-bt-amendment-20150514/fido-u2f-raw-message-formats.html
  void Register(const std::vector<uint8_t>& appid_digest,
                const std::vector<uint8_t>& challenge_digest,
                const MessageCallback& callback);
  void Version(const VersionCallback& callback);
  void Sign(const std::vector<uint8_t>& appid_digest,
            const std::vector<uint8_t>& challenge_digest,
            const std::vector<uint8_t>& key_handle,
            const MessageCallback& callback);
  virtual void TryWink(const WinkCallback& callback) = 0;
  virtual std::string GetId() const = 0;

 protected:
  static constexpr uint8_t kWinkCapability = 0x01;
  static constexpr uint8_t kLockCapability = 0x02;
  static constexpr uint32_t kBroadcastChannel = 0xffffffff;

  U2fDevice();

  // Pure virtual function defined by each device type, implementing
  // the device communication transaction.
  virtual void DeviceTransact(std::unique_ptr<U2fApduCommand> command,
                              const DeviceCallback& callback) = 0;
  virtual base::WeakPtr<U2fDevice> GetWeakPtr() = 0;

  uint32_t channel_id_;
  uint8_t capabilities_;

 private:
  void OnRegisterComplete(const MessageCallback& callback,
                          bool success,
                          std::unique_ptr<U2fApduResponse> register_response);
  void OnSignComplete(const MessageCallback& callback,
                      bool success,
                      std::unique_ptr<U2fApduResponse> sign_response);
  void OnVersionComplete(const VersionCallback& callback,
                         bool success,
                         std::unique_ptr<U2fApduResponse> version_response);
  void OnLegacyVersionComplete(
      const VersionCallback& callback,
      bool success,
      std::unique_ptr<U2fApduResponse> legacy_version_response);
  void OnWink(const WinkCallback& callback,
              bool success,
              std::unique_ptr<U2fApduResponse> response);

  DISALLOW_COPY_AND_ASSIGN(U2fDevice);
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_DEVICE_H_
