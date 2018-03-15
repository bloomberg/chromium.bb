// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_CONSTANTS_H_
#define DEVICE_FIDO_FIDO_CONSTANTS_H_

#include <stdint.h>

#include <array>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"

namespace device {

enum class ProtocolVersion {
  kCtap,
  kU2f,
  kUnknown,
};

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
enum class FidoHidDeviceCommand : uint8_t {
  kMsg = 0x03,
  kCbor = 0x10,
  kInit = 0x06,
  kPing = 0x01,
  kCancel = 0x11,
  kError = 0x3F,
  kKeepAlive = 0x3B,
  kWink = 0x08,
  kLock = 0x04,
};

constexpr std::array<FidoHidDeviceCommand, 9> GetFidoHidDeviceCommandList() {
  return {FidoHidDeviceCommand::kMsg,       FidoHidDeviceCommand::kCbor,
          FidoHidDeviceCommand::kInit,      FidoHidDeviceCommand::kPing,
          FidoHidDeviceCommand::kCancel,    FidoHidDeviceCommand::kError,
          FidoHidDeviceCommand::kKeepAlive, FidoHidDeviceCommand::kWink,
          FidoHidDeviceCommand::kLock};
}

// BLE device command as specified in
//  https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#command-status-and-error-constants
// U2F BLE device does not support cancel command.
enum class FidoBleDeviceCommand : uint8_t {
  kPing = 0x81,
  kKeepAlive = 0x82,
  kMsg = 0x83,
  kCancel = 0xBE,
  kError = 0xBF,
};

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

enum class CoseAlgorithmIdentifier : int { kCoseEs256 = -7 };

// APDU instruction code for U2F request encoding.
// https://fidoalliance.org/specs/fido-u2f-v1.0-ps-20141009/fido-u2f-u2f.h-v1.0-ps-20141009.pdf
enum class U2fApduInstruction : uint8_t {
  kRegister = 0x01,
  kSign = 0x02,
  kVersion = 0x03,
  kVendorFirst = 0x40,
  kVenderLast = 0xBF,
};

// Parameters for fake U2F registration used to check for user presence.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const std::array<uint8_t, 32> kBogusAppParam;
COMPONENT_EXPORT(DEVICE_FIDO)
extern const std::array<uint8_t, 32> kBogusChallenge;

// String key values for CTAP request optional parameters and
// AuthenticatorGetInfo response.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kResidentKeyMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kUserVerificationMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kUserPresenceMapKey[];

// HID transport specific constants.
COMPONENT_EXPORT(DEVICE_FIDO) extern const size_t kHidPacketSize;
COMPONENT_EXPORT(DEVICE_FIDO) extern const uint32_t kHidBroadcastChannel;
COMPONENT_EXPORT(DEVICE_FIDO) extern const size_t kHidInitPacketHeaderSize;
COMPONENT_EXPORT(DEVICE_FIDO) extern const size_t kHidContinuationPacketHeader;
COMPONENT_EXPORT(DEVICE_FIDO) extern const size_t kHidMaxPacketSize;
COMPONENT_EXPORT(DEVICE_FIDO) extern const size_t kHidInitPacketDataSize;
COMPONENT_EXPORT(DEVICE_FIDO)
extern const size_t kHidContinuationPacketDataSize;

COMPONENT_EXPORT(DEVICE_FIDO) extern const uint8_t kHidMaxLockSeconds;

// Messages are limited to an initiation packet and 128 continuation packets.
COMPONENT_EXPORT(DEVICE_FIDO) extern const size_t kHidMaxMessageSize;

// U2F APDU encoding constants, as specified in
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#bib-U2FHeader
COMPONENT_EXPORT(DEVICE_FIDO) extern const size_t kU2fMaxResponseSize;

// P1 instructions.
COMPONENT_EXPORT(DEVICE_FIDO) extern const uint8_t kP1TupRequired;
COMPONENT_EXPORT(DEVICE_FIDO) extern const uint8_t kP1TupConsumed;
COMPONENT_EXPORT(DEVICE_FIDO) extern const uint8_t kP1TupRequiredConsumed;

// Control byte used for check-only setting. The check-only command is used to
// determine if the provided key handle was originally created by this token
// and whether it was created for the provided application parameter.
COMPONENT_EXPORT(DEVICE_FIDO) extern const uint8_t kP1CheckOnly;

// Indicates that an individual attestation certificate is acceptable to
// return with this registration.
COMPONENT_EXPORT(DEVICE_FIDO) extern const uint8_t kP1IndividualAttestation;
COMPONENT_EXPORT(DEVICE_FIDO) extern const size_t kMaxKeyHandleLength;
COMPONENT_EXPORT(DEVICE_FIDO) extern const size_t kU2fParameterLength;

// Suffix added to APDU encoded command for legacy version request.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const std::array<uint8_t, 2> kLegacyVersionSuffix;

// Expected response data for version request from U2F device.
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#getversion-request-and-response---u2f_version
COMPONENT_EXPORT(DEVICE_FIDO)
extern const std::array<uint8_t, 6> kU2fVersionResponse;

// Maximum wait time before client error outs on device.
COMPONENT_EXPORT(DEVICE_FIDO) extern const base::TimeDelta kDeviceTimeout;

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_CONSTANTS_H_
