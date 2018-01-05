// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_CTAP_CTAP_RESPONSE_CODE_H_
#define DEVICE_CTAP_CTAP_RESPONSE_CODE_H_

#include <stdint.h>
#include <vector>

namespace device {

// CTAP protocol device response code, as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#authenticator-api
enum class CTAPResponseCode : uint8_t {
  kSuccess = 0x00,
  kCtap1ErrInvalidCommand = 0x01,
  kCtap1ErrInvalidParameter = 0x02,
  kCtap1ErrInvalidLength = 0x03,
  kCtap1ErrInvalidSeq = 0x04,
  kCtap1ErrTimeout = 0x05,
  kCtap1ErrChannelBusy = 0x06,
  kCtap1ErrLockRequired = 0x0A,
  kCtap1ErrInvalidChannel = 0x0B,
  kCtap2ErrCBORParsing = 0x10,
  kCtap2ErrUnexpectedType = 0x11,
  kCtap2ErrInvalidCBOR = 0x12,
  kCtap2ErrInvalidCBORType = 0x13,
  kCtap2ErrMissingParameter = 0x14,
  kCtap2ErrLimitExceeded = 0x15,
  kCtap2ErrUnsupportedExtension = 0x16,
  kCtap2ErrTooManyElements = 0x17,
  kCtap2ErrExtensionNotSupported = 0x18,
  kCtap2ErrCredentialExcluded = 0x19,
  kCtap2ErrCredentialNotValid = 0x20,
  kCtap2ErrProcesssing = 0x21,
  kCtap2ErrInvalidCredential = 0x22,
  kCtap2ErrUserActionPending = 0x23,
  kCtap2ErrOperationPending = 0x24,
  kCtap2ErrNoOperations = 0x25,
  kCtap2ErrUnsupportedAlgorithms = 0x26,
  kCtap2ErrOperationDenied = 0x27,
  kCtap2ErrKeyStoreFull = 0x28,
  kCtap2ErrNotBusy = 0x29,
  kCtap2ErrNoOperationPending = 0x2A,
  kCtap2ErrUnsupportedOption = 0x2B,
  kCtap2ErrInvalidOption = 0x2C,
  kCtap2ErrKeepAliveCancel = 0x2D,
  kCtap2ErrNoCredentials = 0x2E,
  kCtap2ErrUserActionTimeout = 0x2F,
  kCtap2ErrNotAllowed = 0x30,
  kCtap2ErrPinInvalid = 0x31,
  kCtap2ErrPinBlocked = 0x32,
  kCtap2ErrPinAuthInvalid = 0x33,
  kCtap2ErrPinAuthBlocked = 0x34,
  kCtap2ErrPinNotSet = 0x35,
  kCtap2ErrPinRequired = 0x36,
  kCtap2ErrPinPolicyViolation = 0x37,
  kCtap2ErrPinTokenExpired = 0x38,
  kCtap2ErrRequestTooLarge = 0x39,
  kCtap2ErrOther = 0x7F,
  kCtap2ErrSpecLast = 0xDF,
  kCtap2ErrExtensionFirst = 0xE0,
  kCtap2ErrExtensionLast = 0xEF,
  kCtap2ErrVendorFirst = 0xF0,
  kCtap2ErrVendorLast = 0xFF
};

}  // namespace device

#endif  // DEVICE_CTAP_CTAP_RESPONSE_CODE_H_
