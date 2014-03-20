// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/status.h"

namespace content {

namespace webcrypto {

bool Status::IsError() const { return type_ == TYPE_ERROR; }

bool Status::IsSuccess() const { return type_ == TYPE_SUCCESS; }

bool Status::HasErrorDetails() const { return !error_details_.empty(); }

std::string Status::ToString() const {
  return IsSuccess() ? "Success" : error_details_;
}

Status Status::Success() { return Status(TYPE_SUCCESS); }

Status Status::Error() { return Status(TYPE_ERROR); }

Status Status::ErrorJwkNotDictionary() {
  return Status("JWK input could not be parsed to a JSON dictionary");
}

Status Status::ErrorJwkPropertyMissing(const std::string& property) {
  return Status("The required JWK property \"" + property + "\" was missing");
}

Status Status::ErrorJwkPropertyWrongType(const std::string& property,
                                         const std::string& expected_type) {
  return Status("The JWK property \"" + property + "\" must be a " +
                expected_type);
}

Status Status::ErrorJwkBase64Decode(const std::string& property) {
  return Status("The JWK property \"" + property +
                "\" could not be base64 decoded");
}

Status Status::ErrorJwkExtInconsistent() {
  return Status(
      "The \"ext\" property of the JWK dictionary is inconsistent what that "
      "specified by the Web Crypto call");
}

Status Status::ErrorJwkUnrecognizedAlgorithm() {
  return Status("The JWK \"alg\" property was not recognized");
}

Status Status::ErrorJwkAlgorithmInconsistent() {
  return Status(
      "The JWK \"alg\" property was inconsistent with that specified "
      "by the Web Crypto call");
}

Status Status::ErrorJwkAlgorithmMissing() {
  return Status(
      "The JWK optional \"alg\" property is missing or not a string, "
      "and one wasn't specified by the Web Crypto call");
}

Status Status::ErrorJwkUnrecognizedUse() {
  return Status("The JWK \"use\" property could not be parsed");
}

Status Status::ErrorJwkUnrecognizedKeyop() {
  return Status("The JWK \"key_ops\" property could not be parsed");
}

Status Status::ErrorJwkUseInconsistent() {
  return Status(
      "The JWK \"use\" property was inconsistent with that specified "
      "by the Web Crypto call. The JWK usage must be a superset of "
      "those requested");
}

Status Status::ErrorJwkKeyopsInconsistent() {
  return Status(
      "The JWK \"key_ops\" property was inconsistent with that "
      "specified by the Web Crypto call. The JWK usage must be a "
      "superset of those requested");
}

Status Status::ErrorJwkUseAndKeyopsInconsistent() {
  return Status(
      "The JWK \"use\" and \"key_ops\" properties were both found "
      "but are inconsistent with each other.");
}

Status Status::ErrorJwkRsaPrivateKeyUnsupported() {
  return Status(
      "JWK RSA key contained \"d\" property: Private key import is "
      "not yet supported");
}

Status Status::ErrorJwkUnrecognizedKty() {
  return Status("The JWK \"kty\" property was unrecognized");
}

Status Status::ErrorJwkIncorrectKeyLength() {
  return Status(
      "The JWK \"k\" property did not include the right length "
      "of key data for the given algorithm.");
}

Status Status::ErrorImportEmptyKeyData() {
  return Status("No key data was provided");
}

Status Status::ErrorUnexpectedKeyType() {
  return Status("The key is not of the expected type");
}

Status Status::ErrorIncorrectSizeAesCbcIv() {
  return Status("The \"iv\" has an unexpected length -- must be 16 bytes");
}

Status Status::ErrorDataTooLarge() {
  return Status("The provided data is too large");
}

Status Status::ErrorDataTooSmall() {
  return Status("The provided data is too small");
}

Status Status::ErrorUnsupported() {
  return Status("The requested operation is unsupported");
}

Status Status::ErrorUnexpected() {
  return Status("Something unexpected happened...");
}

Status Status::ErrorInvalidAesGcmTagLength() {
  return Status(
      "The tag length is invalid: Must be 32, 64, 96, 104, 112, 120, or 128 "
      "bits");
}

Status Status::ErrorInvalidAesKwDataLength() {
  return Status(
      "The AES-KW input data length is invalid: not a multiple of 8 "
      "bytes");
}

Status Status::ErrorGenerateKeyPublicExponent() {
  return Status("The \"publicExponent\" is either empty, zero, or too large");
}

Status Status::ErrorMissingAlgorithmImportRawKey() {
  return Status(
      "The key's algorithm must be specified when importing "
      "raw-formatted key.");
}

Status Status::ErrorMissingAlgorithmUnwrapRawKey() {
  return Status(
      "The key's algorithm must be specified when unwrapping a "
      "raw-formatted key.");
}

Status Status::ErrorImportRsaEmptyModulus() {
  return Status("The modulus is empty");
}

Status Status::ErrorGenerateRsaZeroModulus() {
  return Status("The modulus bit length cannot be zero");
}

Status Status::ErrorImportRsaEmptyExponent() {
  return Status("No bytes for the exponent were provided");
}

Status Status::ErrorKeyNotExtractable() {
  return Status("They key is not extractable");
}

Status Status::ErrorGenerateKeyLength() {
  return Status(
      "Invalid key length: it is either zero or not a multiple of 8 "
      "bits");
}

Status::Status(const std::string& error_details_utf8)
    : type_(TYPE_ERROR), error_details_(error_details_utf8) {}

Status::Status(Type type) : type_(type) {}


}  // namespace webcrypto

}  // namespace content
