// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CREDENTIAL_MANAGEMENT_H_
#define DEVICE_FIDO_CREDENTIAL_MANAGEMENT_H_

#include "base/component_export.h"

namespace device {

// https://drafts.fidoalliance.org/fido-2/latest/fido-client-to-authenticator-protocol-v2.0-wd-20190409.html#authenticatorCredentialManagement

enum class CredentialManagementRequestKey : uint8_t {
  kSubCommand = 0x01,
  kSubCommandParams = 0x02,
  kPinProtocol = 0x03,
  kPinAuth = 0x04,
};

enum class CredentialManagementRequestParamKey : uint8_t {
  kRPIDHash = 0x01,
  kCredentialID = 0x02,
};

enum class CredentialManagementResponseKey : uint8_t {
  kExistingResidentCredentialsCount = 0x01,
  kMaxPossibleRemainingResidentCredentialsCount = 0x02,
  kRP = 0x03,
  kRPIDHash = 0x04,
  kTotalRPs = 0x05,
  kUser = 0x06,
  kCredentialID = 0x07,
  kPublicKey = 0x08,
  kTotalCredentials = 0x09,
  kCredProtect = 0x0a,
};

enum class CredentialManagementSubCommand : uint8_t {
  kGetCredsMetadata = 0x01,
  kEnumerateRPsBegin = 0x02,
  kEnumerateRPsGetNextRP = 0x03,
  kEnumerateCredentialsBegin = 0x04,
  kEnumerateCredentialsGetNextCredential = 0x05,
  kDeleteCredential = 0x06,
};

}  // namespace device

#endif  // DEVICE_FIDO_CREDENTIAL_MANAGEMENT_H_
