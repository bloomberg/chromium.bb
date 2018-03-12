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
enum class CtapDeviceResponseCode : uint8_t {
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

constexpr std::array<CtapDeviceResponseCode, 51> GetCtapResponseCodeList() {
  return {CtapDeviceResponseCode::kSuccess,
          CtapDeviceResponseCode::kCtap1ErrInvalidCommand,
          CtapDeviceResponseCode::kCtap1ErrInvalidParameter,
          CtapDeviceResponseCode::kCtap1ErrInvalidLength,
          CtapDeviceResponseCode::kCtap1ErrInvalidSeq,
          CtapDeviceResponseCode::kCtap1ErrTimeout,
          CtapDeviceResponseCode::kCtap1ErrChannelBusy,
          CtapDeviceResponseCode::kCtap1ErrLockRequired,
          CtapDeviceResponseCode::kCtap1ErrInvalidChannel,
          CtapDeviceResponseCode::kCtap2ErrCBORParsing,
          CtapDeviceResponseCode::kCtap2ErrUnexpectedType,
          CtapDeviceResponseCode::kCtap2ErrInvalidCBOR,
          CtapDeviceResponseCode::kCtap2ErrInvalidCBORType,
          CtapDeviceResponseCode::kCtap2ErrMissingParameter,
          CtapDeviceResponseCode::kCtap2ErrLimitExceeded,
          CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension,
          CtapDeviceResponseCode::kCtap2ErrTooManyElements,
          CtapDeviceResponseCode::kCtap2ErrExtensionNotSupported,
          CtapDeviceResponseCode::kCtap2ErrCredentialExcluded,
          CtapDeviceResponseCode::kCtap2ErrCredentialNotValid,
          CtapDeviceResponseCode::kCtap2ErrProcesssing,
          CtapDeviceResponseCode::kCtap2ErrInvalidCredential,
          CtapDeviceResponseCode::kCtap2ErrUserActionPending,
          CtapDeviceResponseCode::kCtap2ErrOperationPending,
          CtapDeviceResponseCode::kCtap2ErrNoOperations,
          CtapDeviceResponseCode::kCtap2ErrUnsupportedAlgorithms,
          CtapDeviceResponseCode::kCtap2ErrOperationDenied,
          CtapDeviceResponseCode::kCtap2ErrKeyStoreFull,
          CtapDeviceResponseCode::kCtap2ErrNotBusy,
          CtapDeviceResponseCode::kCtap2ErrNoOperationPending,
          CtapDeviceResponseCode::kCtap2ErrUnsupportedOption,
          CtapDeviceResponseCode::kCtap2ErrInvalidOption,
          CtapDeviceResponseCode::kCtap2ErrKeepAliveCancel,
          CtapDeviceResponseCode::kCtap2ErrNoCredentials,
          CtapDeviceResponseCode::kCtap2ErrUserActionTimeout,
          CtapDeviceResponseCode::kCtap2ErrNotAllowed,
          CtapDeviceResponseCode::kCtap2ErrPinInvalid,
          CtapDeviceResponseCode::kCtap2ErrPinBlocked,
          CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid,
          CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked,
          CtapDeviceResponseCode::kCtap2ErrPinNotSet,
          CtapDeviceResponseCode::kCtap2ErrPinRequired,
          CtapDeviceResponseCode::kCtap2ErrPinPolicyViolation,
          CtapDeviceResponseCode::kCtap2ErrPinTokenExpired,
          CtapDeviceResponseCode::kCtap2ErrRequestTooLarge,
          CtapDeviceResponseCode::kCtap2ErrOther,
          CtapDeviceResponseCode::kCtap2ErrSpecLast,
          CtapDeviceResponseCode::kCtap2ErrExtensionFirst,
          CtapDeviceResponseCode::kCtap2ErrExtensionLast,
          CtapDeviceResponseCode::kCtap2ErrVendorFirst,
          CtapDeviceResponseCode::kCtap2ErrVendorLast};
}

// Commands supported by CTAPHID device as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#ctaphid-commands
enum class CtapHidDeviceCommand : uint8_t {
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

constexpr std::array<CtapHidDeviceCommand, 9> GetCtapHidDeviceCommandList() {
  return {CtapHidDeviceCommand::kCtapHidMsg,
          CtapHidDeviceCommand::kCtapHidCBOR,
          CtapHidDeviceCommand::kCtapHidInit,
          CtapHidDeviceCommand::kCtapHidPing,
          CtapHidDeviceCommand::kCtapHidCancel,
          CtapHidDeviceCommand::kCtapHidError,
          CtapHidDeviceCommand::kCtapHidKeepAlive,
          CtapHidDeviceCommand::kCtapHidWink,
          CtapHidDeviceCommand::kCtapHidLock};
}

// Authenticator API commands supported by CTAP devices, as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#authenticator-api
enum class CtapRequestCommand : uint8_t {
  kAuthenticatorMakeCredential = 0x01,
  kAuthenticatorGetAssertion = 0x02,
  kAuthenticatorGetNextAssertion = 0x08,
  kAuthenticatorCancel = 0x03,
  kAuthenticatorGetInfo = 0x04,
  kAuthenticatorClientPin = 0x06,
  kAuthenticatorReset = 0x07,
};

enum class kCoseAlgorithmIdentifier : int { kCoseEs256 = -7 };

// String key values for CTAP request optional parameters and
// AuthenticatorGetInfo response.
extern const char kResidentKeyMapKey[];
extern const char kUserVerificationMapKey[];
extern const char kUserPresenceMapKey[];

// HID transport specific constants.
extern const size_t kHidPacketSize;
extern const uint32_t kHidBroadcastChannel;
extern const size_t kHidInitPacketHeaderSize;
extern const size_t kHidContinuationPacketHeader;
extern const size_t kHidMaxPacketSize;
extern const size_t kHidInitPacketDataSize;
extern const size_t kHidContinuationPacketDataSize;
extern const uint8_t kHidMaxLockSeconds;
// Messages are limited to an initiation packet and 128 continuation packets.
extern const size_t kHidMaxMessageSize;

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_CONSTANTS_H_
