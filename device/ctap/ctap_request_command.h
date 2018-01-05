// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_CTAP_CTAP_REQUEST_COMMAND_H_
#define DEVICE_CTAP_CTAP_REQUEST_COMMAND_H_

#include <stdint.h>

namespace device {

// Authenticator API commands supported by CTAP devices, as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#authenticator-api
enum class CTAPRequestCommand : uint8_t {
  kAuthenticatorMakeCredential = 0x01,
  kAuthenticatorGetAssertion = 0x02,
  kAuthenticatorGetNextAssertion = 0x08,
  kAuthenticatorCancel = 0x03,
  kAuthenticatorGetInfo = 0x04,
  kAuthenticatorClientPin = 0x06,
  kAuthenticatorReset = 0x07,
};

}  // namespace device

#endif  // DEVICE_CTAP_CTAP_REQUEST_COMMAND_H_
