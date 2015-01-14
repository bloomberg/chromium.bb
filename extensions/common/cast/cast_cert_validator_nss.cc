// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/cast/cast_cert_validator.h"

#include <cert.h>
#include <cryptohi.h>
#include <pk11pub.h>
#include <seccomon.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "extensions/browser/api/cast_channel/cast_auth_ica.h"

namespace extensions {
namespace core_api {
namespace cast_crypto {

namespace {

typedef scoped_ptr<
    CERTCertificate,
    crypto::NSSDestroyer<CERTCertificate, CERT_DestroyCertificate>>
    ScopedCERTCertificate;

class CertVerificationContextNSS : public CertVerificationContext {
 public:
  explicit CertVerificationContextNSS(CERTCertificate* certificate)
      : certificate_(certificate) {}

  VerificationResult VerifySignatureOverData(
      const base::StringPiece& signature,
      const base::StringPiece& data) const override {
    // Retrieve public key object
    crypto::ScopedSECKEYPublicKey public_key_obj(
        CERT_ExtractPublicKey(certificate_.get()));
    if (!public_key_obj.get()) {
      return VerificationResult(
          "Failed to extract device certificate public key.",
          VerificationResult::ERROR_CERT_INVALID);
    }
    // Verify signature.
    SECItem signature_item;
    signature_item.type = siBuffer;
    signature_item.data =
        reinterpret_cast<unsigned char*>(const_cast<char*>(signature.data()));
    signature_item.len = signature.length();
    if (VFY_VerifyDataDirect(
            reinterpret_cast<unsigned char*>(const_cast<char*>(data.data())),
            data.size(), public_key_obj.get(), &signature_item,
            SEC_OID_PKCS1_RSA_ENCRYPTION, SEC_OID_SHA1, NULL,
            NULL) != SECSuccess) {
      return VerificationResult("Signature verification failed.",
                                VerificationResult::ERROR_SIGNATURE_INVALID,
                                PORT_GetError());
    }
    return VerificationResult();
  }

  std::string GetCommonName() const override {
    char* common_name = CERT_GetCommonName(&certificate_->subject);
    if (!common_name)
      return std::string();

    std::string result(common_name);
    PORT_Free(common_name);
    return result;
  }

 private:
  ScopedCERTCertificate certificate_;
};

}  // namespace

VerificationResult VerifyDeviceCert(
    const base::StringPiece& device_cert,
    const std::vector<std::string>& ica_certs,
    scoped_ptr<CertVerificationContext>* context) {
  crypto::EnsureNSSInit();

  // If the list of intermediates is empty then use kPublicKeyICA1 as
  // the trusted CA (legacy case).
  // Otherwise, use the first intermediate in the list as long as it
  // is in the allowed list of intermediates.
  base::StringPiece ica_public_key_der =
      (ica_certs.size() == 0)
          ? cast_channel::GetDefaultTrustedICAPublicKey()
          : cast_channel::GetTrustedICAPublicKey(ica_certs[0]);

  if (ica_public_key_der.empty()) {
    return VerificationResult(
        "Device certificate is not signed by a trusted CA",
        VerificationResult::ERROR_CERT_UNTRUSTED);
  }
  // Initialize the ICA public key.
  SECItem ica_public_key_der_item;
  ica_public_key_der_item.type = SECItemType::siDERCertBuffer;
  ica_public_key_der_item.data = const_cast<uint8_t*>(
      reinterpret_cast<const uint8_t*>(ica_public_key_der.data()));
  ica_public_key_der_item.len = ica_public_key_der.size();

  crypto::ScopedSECKEYPublicKey ica_public_key_obj(
      SECKEY_ImportDERPublicKey(&ica_public_key_der_item, CKK_RSA));
  if (!ica_public_key_obj) {
    return VerificationResult("Failed to import trusted public key.",
                              VerificationResult::ERROR_INTERNAL,
                              PORT_GetError());
  }
  SECItem device_cert_der_item;
  device_cert_der_item.type = siDERCertBuffer;
  // Make a copy of certificate string so it is safe to type cast.
  device_cert_der_item.data =
      reinterpret_cast<unsigned char*>(const_cast<char*>(device_cert.data()));
  device_cert_der_item.len = device_cert.length();

  // Parse into a certificate structure.
  ScopedCERTCertificate device_cert_obj(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &device_cert_der_item, NULL, PR_FALSE, PR_TRUE));
  if (!device_cert_obj.get()) {
    return VerificationResult("Failed to parse device certificate.",
                              VerificationResult::ERROR_CERT_INVALID,
                              PORT_GetError());
  }
  if (CERT_VerifySignedDataWithPublicKey(&device_cert_obj->signatureWrap,
                                         ica_public_key_obj.get(),
                                         NULL) != SECSuccess) {
    return VerificationResult("Signature verification failed.",
                              VerificationResult::ERROR_SIGNATURE_INVALID,
                              PORT_GetError());
  }
  if (context) {
    scoped_ptr<CertVerificationContext> tmp_context(
        new CertVerificationContextNSS(device_cert_obj.release()));
    tmp_context.swap(*context);
  }

  return VerificationResult();
}

std::string VerificationResult::GetLogString() const {
  std::string nssError = "NSS Error Code: ";
  nssError += base::IntToString(library_error_code);
  return error_message.size()
             ? std::string("Error: ") + error_message + ", " + nssError
             : nssError;
}

}  // namespace cast_crypto
}  // namespace core_api
}  // namespace extensions
