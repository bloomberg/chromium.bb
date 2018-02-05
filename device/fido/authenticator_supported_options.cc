// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_supported_options.h"

namespace device {

AuthenticatorSupportedOptions::AuthenticatorSupportedOptions() = default;

AuthenticatorSupportedOptions::AuthenticatorSupportedOptions(
    AuthenticatorSupportedOptions&& other) = default;

AuthenticatorSupportedOptions& AuthenticatorSupportedOptions::operator=(
    AuthenticatorSupportedOptions&& other) = default;

AuthenticatorSupportedOptions::~AuthenticatorSupportedOptions() = default;

AuthenticatorSupportedOptions&
AuthenticatorSupportedOptions::SetIsPlatformDevice(bool is_platform_device) {
  is_platform_device_ = is_platform_device;
  return *this;
}

AuthenticatorSupportedOptions&
AuthenticatorSupportedOptions::SetSupportsResidentKey(
    bool supports_resident_key) {
  supports_resident_key_ = supports_resident_key;
  return *this;
}

AuthenticatorSupportedOptions&
AuthenticatorSupportedOptions::SetUserVerificationRequired(
    bool user_verification_required) {
  user_verification_required_ = user_verification_required;
  return *this;
}

AuthenticatorSupportedOptions&
AuthenticatorSupportedOptions::SetUserPresenceRequired(
    bool user_presence_required) {
  user_presence_required_ = user_presence_required;
  return *this;
}

AuthenticatorSupportedOptions&
AuthenticatorSupportedOptions::SetClientPinStored(bool client_pin_stored) {
  client_pin_stored_ = client_pin_stored;
  return *this;
}

}  // namespace device
