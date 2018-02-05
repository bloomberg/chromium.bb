// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP_CONSTANTS_H_
#define DEVICE_FIDO_CTAP_CONSTANTS_H_

#include <stdint.h>

#include <array>
#include <vector>

namespace device {

// CTAP protocol device response code, as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#authenticator-api
enum class CTAPDeviceResponseCode : uint8_t {
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

// Commands supported by CTAPHID device as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#ctaphid-commands
enum class CTAPHIDDeviceCommand : uint8_t {
  kCtapHidMsg = 0x03,
  kCtapHidCBOR = 0x10,
  kCtapHidInit = 0x06,
  kCtapHidPing = 0x01,
  kCtapHidCancel = 0x11,
  kCtapHidError = 0x3F,
  kCtapHidKeepAlive = 0x3B,
  kCtapHidWink = 0x08,
  kCtapHidLock = 0x04,
};

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

constexpr std::array<CTAPDeviceResponseCode, 51> GetCTAPResponseCodeList() {
  return {CTAPDeviceResponseCode::kSuccess,
          CTAPDeviceResponseCode::kCtap1ErrInvalidCommand,
          CTAPDeviceResponseCode::kCtap1ErrInvalidParameter,
          CTAPDeviceResponseCode::kCtap1ErrInvalidLength,
          CTAPDeviceResponseCode::kCtap1ErrInvalidSeq,
          CTAPDeviceResponseCode::kCtap1ErrTimeout,
          CTAPDeviceResponseCode::kCtap1ErrChannelBusy,
          CTAPDeviceResponseCode::kCtap1ErrLockRequired,
          CTAPDeviceResponseCode::kCtap1ErrInvalidChannel,
          CTAPDeviceResponseCode::kCtap2ErrCBORParsing,
          CTAPDeviceResponseCode::kCtap2ErrUnexpectedType,
          CTAPDeviceResponseCode::kCtap2ErrInvalidCBOR,
          CTAPDeviceResponseCode::kCtap2ErrInvalidCBORType,
          CTAPDeviceResponseCode::kCtap2ErrMissingParameter,
          CTAPDeviceResponseCode::kCtap2ErrLimitExceeded,
          CTAPDeviceResponseCode::kCtap2ErrUnsupportedExtension,
          CTAPDeviceResponseCode::kCtap2ErrTooManyElements,
          CTAPDeviceResponseCode::kCtap2ErrExtensionNotSupported,
          CTAPDeviceResponseCode::kCtap2ErrCredentialExcluded,
          CTAPDeviceResponseCode::kCtap2ErrCredentialNotValid,
          CTAPDeviceResponseCode::kCtap2ErrProcesssing,
          CTAPDeviceResponseCode::kCtap2ErrInvalidCredential,
          CTAPDeviceResponseCode::kCtap2ErrUserActionPending,
          CTAPDeviceResponseCode::kCtap2ErrOperationPending,
          CTAPDeviceResponseCode::kCtap2ErrNoOperations,
          CTAPDeviceResponseCode::kCtap2ErrUnsupportedAlgorithms,
          CTAPDeviceResponseCode::kCtap2ErrOperationDenied,
          CTAPDeviceResponseCode::kCtap2ErrKeyStoreFull,
          CTAPDeviceResponseCode::kCtap2ErrNotBusy,
          CTAPDeviceResponseCode::kCtap2ErrNoOperationPending,
          CTAPDeviceResponseCode::kCtap2ErrUnsupportedOption,
          CTAPDeviceResponseCode::kCtap2ErrInvalidOption,
          CTAPDeviceResponseCode::kCtap2ErrKeepAliveCancel,
          CTAPDeviceResponseCode::kCtap2ErrNoCredentials,
          CTAPDeviceResponseCode::kCtap2ErrUserActionTimeout,
          CTAPDeviceResponseCode::kCtap2ErrNotAllowed,
          CTAPDeviceResponseCode::kCtap2ErrPinInvalid,
          CTAPDeviceResponseCode::kCtap2ErrPinBlocked,
          CTAPDeviceResponseCode::kCtap2ErrPinAuthInvalid,
          CTAPDeviceResponseCode::kCtap2ErrPinAuthBlocked,
          CTAPDeviceResponseCode::kCtap2ErrPinNotSet,
          CTAPDeviceResponseCode::kCtap2ErrPinRequired,
          CTAPDeviceResponseCode::kCtap2ErrPinPolicyViolation,
          CTAPDeviceResponseCode::kCtap2ErrPinTokenExpired,
          CTAPDeviceResponseCode::kCtap2ErrRequestTooLarge,
          CTAPDeviceResponseCode::kCtap2ErrOther,
          CTAPDeviceResponseCode::kCtap2ErrSpecLast,
          CTAPDeviceResponseCode::kCtap2ErrExtensionFirst,
          CTAPDeviceResponseCode::kCtap2ErrExtensionLast,
          CTAPDeviceResponseCode::kCtap2ErrVendorFirst,
          CTAPDeviceResponseCode::kCtap2ErrVendorLast};
}

extern const char kResidentKeyMapKey[];
extern const char kUserVerificationMapKey[];
extern const char kUserPresenceMapKey[];

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_CONSTANTS_H_
