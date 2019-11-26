// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_PASSPHRASE_ENUMS_H_
#define COMPONENTS_SYNC_BASE_PASSPHRASE_ENUMS_H_

#include "base/optional.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

// The different states for the encryption passphrase. These control if and how
// the user should be prompted for a decryption passphrase.
// Do not re-order or delete these entries; they are used in a UMA histogram.
// Please edit SyncPassphraseType in enums.xml if a value is added.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.sync
enum class PassphraseType {
  // GAIA-based passphrase (deprecated).
  kImplicitPassphrase = 0,
  // Keystore passphrase.
  kKeystorePassphrase = 1,
  // Frozen GAIA passphrase.
  kFrozenImplicitPassphrase = 2,
  // User-provided passphrase.
  kCustomPassphrase = 3,
  // Trusted-vault passphrase.
  kTrustedVaultPassphrase = 4,
  // Alias used by UMA macros to deduce the correct boundary value.
  kMaxValue = kTrustedVaultPassphrase
};

bool IsExplicitPassphrase(PassphraseType type);

// Function meant to convert |NigoriSpecifics.passphrase_type| into enum.
// All unknown values are returned as UNKNOWN, which mainly represents future
// values of the enum that are not currently supported. Note however that if the
// field is not populated, it defaults to IMPLICIT_PASSPHRASE for backwards
// compatibility reasons.
sync_pb::NigoriSpecifics::PassphraseType ProtoPassphraseInt32ToProtoEnum(
    ::google::protobuf::int32 type);

// Returns base::nullopt if |type| represents an unknown value, likely coming
// from a future version of the browser. Note however that if the field is not
// populated, it defaults to IMPLICIT_PASSPHRASE for backwards compatibility
// reasons.
base::Optional<PassphraseType> ProtoPassphraseInt32ToEnum(
    ::google::protobuf::int32 type);

sync_pb::NigoriSpecifics::PassphraseType EnumPassphraseTypeToProto(
    PassphraseType type);

// Different key derivation methods. Used for deriving the encryption key from a
// user's custom passphrase.
enum class KeyDerivationMethod {
  PBKDF2_HMAC_SHA1_1003 = 0,  // PBKDF2-HMAC-SHA1 with 1003 iterations.
  SCRYPT_8192_8_11 = 1,  // scrypt with N = 2^13, r = 8, p = 11 and random salt.
  UNSUPPORTED = 2,       // Unsupported method, likely from a future version.
};

// This function accepts an integer and not KeyDerivationMethod from the proto
// in order to be able to handle new, unknown values.
KeyDerivationMethod ProtoKeyDerivationMethodToEnum(
    ::google::protobuf::int32 method);
// Note that KeyDerivationMethod::UNSUPPORTED is an invalid input for this
// function, since it corresponds to a method that we are not aware of and so
// cannot meaningfully convert. The caller is responsible for ensuring that
// KeyDerivationMethod::UNSUPPORTED is never passed to this function.
sync_pb::NigoriSpecifics::KeyDerivationMethod EnumKeyDerivationMethodToProto(
    KeyDerivationMethod method);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_PASSPHRASE_ENUMS_H_
