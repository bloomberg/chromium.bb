// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"

#include <cert.h>
#include <cryptohi.h>
#include <keyhi.h>
#include <keythi.h>
#include <pk11pub.h>
#include <sechash.h>
#include <secport.h>

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "crypto/nss_util.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/pem_tokenizer.h"
#include "net/cert/x509_certificate.h"

namespace {

// Parses |pem_data| for a PEM block of |pem_type|.
// Returns true if a |pem_type| block is found, storing the decoded result in
// |der_output|.
bool GetDERFromPEM(const std::string& pem_data,
                   const std::string& pem_type,
                   std::vector<uint8_t>* der_output) {
  std::vector<std::string> headers;
  headers.push_back(pem_type);
  net::PEMTokenizer pem_tok(pem_data, headers);
  if (!pem_tok.GetNext()) {
    return false;
  }

  der_output->assign(pem_tok.data().begin(), pem_tok.data().end());
  return true;
}

}  // namespace

namespace networking_private_crypto {

bool VerifyCredentials(const std::string& certificate,
                       const std::string& signature,
                       const std::string& data,
                       const std::string& connected_mac) {
  crypto::EnsureNSSInit();

  std::vector<uint8_t> cert_data;
  if (!GetDERFromPEM(certificate, "CERTIFICATE", &cert_data)) {
    LOG(ERROR) << "Failed to parse certificate.";
    return false;
  }
  SECItem der_cert;
  der_cert.type = siDERCertBuffer;
  der_cert.data = cert_data.data();
  der_cert.len = cert_data.size();

  // Parse into a certificate structure.
  typedef scoped_ptr<
      CERTCertificate,
      crypto::NSSDestroyer<CERTCertificate, CERT_DestroyCertificate> >
      ScopedCERTCertificate;
  ScopedCERTCertificate cert(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &der_cert, NULL, PR_FALSE, PR_TRUE));
  if (!cert.get()) {
    LOG(ERROR) << "Failed to parse certificate.";
    return false;
  }

  // Check that the certificate is signed by trusted CA.
  SECItem trusted_ca_key_der_item;
  trusted_ca_key_der_item.type = siDERCertBuffer;
  trusted_ca_key_der_item.data =
      const_cast<unsigned char*>(kTrustedCAPublicKeyDER);
  trusted_ca_key_der_item.len = kTrustedCAPublicKeyDERLength;
  crypto::ScopedSECKEYPublicKey ca_public_key(
      SECKEY_ImportDERPublicKey(&trusted_ca_key_der_item, CKK_RSA));
  SECStatus verified = CERT_VerifySignedDataWithPublicKey(
      &cert->signatureWrap, ca_public_key.get(), NULL);
  if (verified != SECSuccess) {
    LOG(ERROR) << "Certificate is not issued by the trusted CA.";
    return false;
  }

  // Check that the device listed in the certificate is correct.
  // Something like evt_e161 001a11ffacdf
  char* common_name = CERT_GetCommonName(&cert->subject);
  if (!common_name) {
    LOG(ERROR) << "Certificate does not have common name.";
    return false;
  }

  std::string subject_name(common_name);
  PORT_Free(common_name);
  std::string translated_mac;
  base::RemoveChars(connected_mac, ":", &translated_mac);
  if (!EndsWith(subject_name, translated_mac, false)) {
    LOG(ERROR) << "MAC addresses don't match.";
    return false;
  }

  // Make sure that the certificate matches the unsigned data presented.
  // Verify that the |signature| matches |data|.
  crypto::ScopedSECKEYPublicKey public_key(CERT_ExtractPublicKey(cert.get()));
  if (!public_key.get()) {
    LOG(ERROR) << "Unable to extract public key from certificate.";
    return false;
  }
  SECItem signature_item;
  signature_item.type = siBuffer;
  signature_item.data =
      reinterpret_cast<unsigned char*>(const_cast<char*>(signature.c_str()));
  signature_item.len = static_cast<unsigned int>(signature.size());
  verified = VFY_VerifyDataDirect(
      reinterpret_cast<unsigned char*>(const_cast<char*>(data.c_str())),
      data.size(),
      public_key.get(),
      &signature_item,
      SEC_OID_PKCS1_RSA_ENCRYPTION,
      SEC_OID_SHA1,
      NULL,
      NULL);
  if (verified != SECSuccess) {
    LOG(ERROR) << "Signed blobs did not match.";
    return false;
  }
  return true;
}

bool EncryptByteString(const std::vector<uint8_t>& pub_key_der,
                       const std::string& data,
                       std::vector<uint8_t>* encrypted_output) {
  crypto::EnsureNSSInit();

  SECItem pub_key_der_item;
  pub_key_der_item.type = siDERCertBuffer;
  pub_key_der_item.data = const_cast<unsigned char*>(pub_key_der.data());
  pub_key_der_item.len = pub_key_der.size();

  crypto::ScopedSECKEYPublicKey public_key(
      SECKEY_ImportDERPublicKey(&pub_key_der_item, CKK_RSA));
  if (!public_key.get()) {
    LOG(ERROR) << "Failed to parse public key.";
    return false;
  }

  size_t encrypted_length = SECKEY_PublicKeyStrength(public_key.get());
  // RSAES is defined as operating on messages up to a length of k - 11, where
  // k is the octet length of the RSA modulus.
  if (encrypted_length < data.size() + 11) {
    LOG(ERROR) << "Too much data to encrypt.";
    return false;
  }

  scoped_ptr<unsigned char[]> rsa_output(new unsigned char[encrypted_length]);
  SECStatus encrypted = PK11_PubEncryptPKCS1(
      public_key.get(),
      rsa_output.get(),
      reinterpret_cast<unsigned char*>(const_cast<char*>(data.data())),
      data.length(),
      NULL);
  if (encrypted != SECSuccess) {
    LOG(ERROR) << "Error during encryption.";
    return false;
  }
  encrypted_output->assign(rsa_output.get(),
                           rsa_output.get() + encrypted_length);
  return true;
}

bool DecryptByteString(const std::string& private_key_pem,
                       const std::vector<uint8_t>& encrypted_data,
                       std::string* decrypted_output) {
  crypto::EnsureNSSInit();

  std::vector<uint8_t> private_key_data;
  if (!GetDERFromPEM(private_key_pem, "PRIVATE KEY", &private_key_data)) {
    LOG(ERROR) << "Failed to parse private key PEM.";
    return false;
  }
  scoped_ptr<crypto::RSAPrivateKey> private_key(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(private_key_data));
  if (!private_key || !private_key->public_key()) {
    LOG(ERROR) << "Failed to parse private key DER.";
    return false;
  }

  size_t encrypted_length = SECKEY_SignatureLen(private_key->public_key());
  scoped_ptr<unsigned char[]> rsa_output(new unsigned char[encrypted_length]);
  unsigned int output_length = 0;
  SECStatus decrypted =
      PK11_PrivDecryptPKCS1(private_key->key(),
                            rsa_output.get(),
                            &output_length,
                            encrypted_length,
                            const_cast<unsigned char*>(encrypted_data.data()),
                            encrypted_data.size());
  if (decrypted != SECSuccess) {
    LOG(ERROR) << "Error during decryption.";
    return false;
  }
  decrypted_output->assign(reinterpret_cast<char*>(rsa_output.get()),
                           output_length);
  return true;
}

}  // namespace networking_private_crypto
