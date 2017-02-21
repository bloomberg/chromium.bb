// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_DEVICE_H_
#define DEVICE_U2F_U2F_DEVICE_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "u2f_apdu_response.h"

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
  enum class ReturnCode { SUCCESS, HW_FAILURE, INVALID_PARAMS };

  using MessageCallback =
      base::Callback<void(ReturnCode, std::vector<uint8_t>)>;
  using VersionCallback =
      base::Callback<void(bool success, ProtocolVersion version)>;
  using DeviceCallback =
      base::Callback<void(bool success,
                          scoped_refptr<U2fApduResponse> response)>;

  ~U2fDevice();

  // Raw messages parameters are defined by the specification at
  // https://fidoalliance.org/specs/fido-u2f-v1.0-nfc-bt-amendment-20150514/fido-u2f-raw-message-formats.html
  void Register(const std::vector<uint8_t>& appid_digest,
                const ProtocolVersion version,
                const std::vector<uint8_t>& challenge_digest,
                const MessageCallback& callback);
  void Version(const VersionCallback& callback);
  void Sign(const std::vector<uint8_t>& appid_digest,
            const std::vector<uint8_t>& challenge_digest,
            const std::vector<uint8_t>& key_handle,
            const MessageCallback& callback);

 protected:
  U2fDevice();

  // Pure virtual function defined by each device type, implementing
  // the device communication transaction.
  virtual void DeviceTransact(scoped_refptr<U2fApduCommand> command,
                              const DeviceCallback& callback) = 0;

 private:
  // TODO Callback functions for device calls
  void OnRegisterComplete(const MessageCallback& callback,
                          bool success,
                          scoped_refptr<U2fApduResponse> register_response);
  void OnSignComplete(const MessageCallback& callback,
                      bool success,
                      scoped_refptr<U2fApduResponse> sign_response);
  void OnVersionComplete(const VersionCallback& callback,
                         bool success,
                         scoped_refptr<U2fApduResponse> version_response);
  void OnLegacyVersionComplete(
      const VersionCallback& callback,
      bool success,
      scoped_refptr<U2fApduResponse> legacy_version_response);

  base::WeakPtrFactory<U2fDevice> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(U2fDevice);
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_DEVICE_H_
