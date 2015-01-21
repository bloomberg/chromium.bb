// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CAST_CAST_CERT_VALIDATOR_H_
#define EXTENSIONS_COMMON_CAST_CAST_CERT_VALIDATOR_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"

namespace extensions {
namespace core_api {
namespace cast_crypto {

// Status of a certificate or certificate verification operation.
struct VerificationResult {
  // Mapped to extensions::core_api::cast_channel::AuthResult::ErrorType in
  // cast_auto_util.cc. Update the mapping code when modifying this enum.
  enum ErrorType {
    // Verification has succeeded.
    ERROR_NONE = 0,
    // There was a problem with the certificate, such as invalid or corrupt
    // certificate data or invalid issuing certificate signature.
    ERROR_CERT_INVALID,
    // Certificate may be valid, but not trusted in this context.
    ERROR_CERT_UNTRUSTED,
    // Signature verification failed
    ERROR_SIGNATURE_INVALID,
    // Catch-all for internal errors that are not covered by the other error
    // types.
    ERROR_INTERNAL
  };

  // Constructs a VerificationResult that corresponds to success.
  VerificationResult();

  // Construct error-related objects
  VerificationResult(const std::string& error_message, ErrorType error_type);
  VerificationResult(const std::string& error_message,
                     ErrorType error_type,
                     int error_code);

  bool Success() const { return error_type == ERROR_NONE; }
  bool Failure() const { return error_type != ERROR_NONE; }

  // Generates a string representation of this object for logging.
  std::string GetLogString() const;

  ErrorType error_type;
  // Human-readable description of the problem if error_type != ERROR_NONE
  std::string error_message;
  // May contain the underlying crypto library error code.
  int library_error_code;
};

// An object of this type is returned by the VerifyCert function, and can be
// used for additional certificate-related operations, using the verified
// certificate.
class CertVerificationContext {
 public:
  CertVerificationContext() {}
  virtual ~CertVerificationContext() {}

  // Use the public key from the verified certificate to verify a
  // sha1WithRSAEncryption |signature| over arbitrary |data|. Both |signature|
  // and |data| hold raw binary data.
  virtual VerificationResult VerifySignatureOverData(
      const base::StringPiece& signature,
      const base::StringPiece& data) const = 0;

  // Retrieve the Common Name attribute of the subject's distinguished name from
  // the verified certificate, if present.  Returns an empty string if no Common
  // Name is found.
  virtual std::string GetCommonName() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CertVerificationContext);
};

// Verify a cast device certificate, using optional intermediate certificate
// authority certificates. |context| will be populated with an instance of
// CertVerificationContext, which allows to perform additional verification
// steps as required.
VerificationResult VerifyDeviceCert(
    const base::StringPiece& device_cert,
    const std::vector<std::string>& ica_certs,
    scoped_ptr<CertVerificationContext>* context);

}  // namespace cast_crypto
}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_CAST_CAST_CERT_VALIDATOR_H_
