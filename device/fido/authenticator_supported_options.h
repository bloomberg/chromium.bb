// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTHENTICATOR_SUPPORTED_OPTIONS_H_
#define DEVICE_FIDO_AUTHENTICATOR_SUPPORTED_OPTIONS_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/cbor/cbor_values.h"

namespace device {

// Represents CTAP device properties and capabilities received as a response to
// AuthenticatorGetInfo command.
class COMPONENT_EXPORT(DEVICE_FIDO) AuthenticatorSupportedOptions {
 public:
  AuthenticatorSupportedOptions();
  AuthenticatorSupportedOptions(AuthenticatorSupportedOptions&& other);
  AuthenticatorSupportedOptions& operator=(
      AuthenticatorSupportedOptions&& other);
  ~AuthenticatorSupportedOptions();

  cbor::CBORValue ConvertToCBOR() const;
  AuthenticatorSupportedOptions& SetIsPlatformDevice(bool is_platform_device);
  AuthenticatorSupportedOptions& SetSupportsResidentKey(
      bool supports_resident_key);
  AuthenticatorSupportedOptions& SetUserVerificationRequired(
      bool user_verification_required);
  AuthenticatorSupportedOptions& SetUserPresenceRequired(
      bool user_presence_required);
  AuthenticatorSupportedOptions& SetClientPinStored(bool client_pin_stored);

  bool is_platform_device() const { return is_platform_device_; }
  bool supports_resident_key() const { return supports_resident_key_; }
  bool user_verification_required() const {
    return user_verification_required_;
  }
  bool user_presence_required() const { return user_presence_required_; }
  const base::Optional<bool>& client_pin_stored() const {
    return client_pin_stored_;
  }

 private:
  // Indicates that the device is attached to the client and therefore can't be
  // removed and used on another client.
  bool is_platform_device_ = false;
  // Indicates that the device is capable of storing keys on the device itself
  // and therefore can satisfy the authenticatorGetAssertion request with
  // allowList parameter not specified or empty.
  bool supports_resident_key_ = false;
  bool user_verification_required_ = false;
  bool user_presence_required_ = true;
  // Represents whether client pin in set and stored in device. Set as null
  // optional if client pin capability is not supported by the authenticator.
  base::Optional<bool> client_pin_stored_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorSupportedOptions);
};

}  // namespace device

#endif  // DEVICE_FIDO_AUTHENTICATOR_SUPPORTED_OPTIONS_H_
