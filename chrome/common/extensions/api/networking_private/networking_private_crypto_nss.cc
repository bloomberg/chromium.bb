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

const unsigned char kTrustedCAPublicKeyDER[] = {
    0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xbc, 0x22, 0x80,
    0xbd, 0x80, 0xf6, 0x3a, 0x21, 0x00, 0x3b, 0xae, 0x76, 0x5e, 0x35, 0x7f,
    0x3d, 0xc3, 0x64, 0x5c, 0x55, 0x94, 0x86, 0x34, 0x2f, 0x05, 0x87, 0x28,
    0xcd, 0xf7, 0x69, 0x8c, 0x17, 0xb3, 0x50, 0xa7, 0xb8, 0x82, 0xfa, 0xdf,
    0xc7, 0x43, 0x2d, 0xd6, 0x7e, 0xab, 0xa0, 0x6f, 0xb7, 0x13, 0x72, 0x80,
    0xa4, 0x47, 0x15, 0xc1, 0x20, 0x99, 0x50, 0xcd, 0xec, 0x14, 0x62, 0x09,
    0x5b, 0xa4, 0x98, 0xcd, 0xd2, 0x41, 0xb6, 0x36, 0x4e, 0xff, 0xe8, 0x2e,
    0x32, 0x30, 0x4a, 0x81, 0xa8, 0x42, 0xa3, 0x6c, 0x9b, 0x33, 0x6e, 0xca,
    0xb2, 0xf5, 0x53, 0x66, 0xe0, 0x27, 0x53, 0x86, 0x1a, 0x85, 0x1e, 0xa7,
    0x39, 0x3f, 0x4a, 0x77, 0x8e, 0xfb, 0x54, 0x66, 0x66, 0xfb, 0x58, 0x54,
    0xc0, 0x5e, 0x39, 0xc7, 0xf5, 0x50, 0x06, 0x0b, 0xe0, 0x8a, 0xd4, 0xce,
    0xe1, 0x6a, 0x55, 0x1f, 0x8b, 0x17, 0x00, 0xe6, 0x69, 0xa3, 0x27, 0xe6,
    0x08, 0x25, 0x69, 0x3c, 0x12, 0x9d, 0x8d, 0x05, 0x2c, 0xd6, 0x2e, 0xa2,
    0x31, 0xde, 0xb4, 0x52, 0x50, 0xd6, 0x20, 0x49, 0xde, 0x71, 0xa0, 0xf9,
    0xad, 0x20, 0x40, 0x12, 0xf1, 0xdd, 0x25, 0xeb, 0xd5, 0xe6, 0xb8, 0x36,
    0xf4, 0xd6, 0x8f, 0x7f, 0xca, 0x43, 0xdc, 0xd7, 0x10, 0x5b, 0xe6, 0x3f,
    0x51, 0x8a, 0x85, 0xb3, 0xf3, 0xff, 0xf6, 0x03, 0x2d, 0xcb, 0x23, 0x4f,
    0x9c, 0xad, 0x18, 0xe7, 0x93, 0x05, 0x8c, 0xac, 0x52, 0x9a, 0xf7, 0x4c,
    0xe9, 0x99, 0x7a, 0xbe, 0x6e, 0x7e, 0x4d, 0x0a, 0xe3, 0xc6, 0x1c, 0xa9,
    0x93, 0xfa, 0x3a, 0xa5, 0x91, 0x5d, 0x1c, 0xbd, 0x66, 0xeb, 0xcc, 0x60,
    0xdc, 0x86, 0x74, 0xca, 0xcf, 0xf8, 0x92, 0x1c, 0x98, 0x7d, 0x57, 0xfa,
    0x61, 0x47, 0x9e, 0xab, 0x80, 0xb7, 0xe4, 0x48, 0x80, 0x2a, 0x92, 0xc5,
    0x1b, 0x02, 0x03, 0x01, 0x00, 0x01};

namespace {

// Parses |pem_data| for a PEM block of |pem_type|.
// Returns true if a |pem_type| block is found, storing the decoded result in
// |der_output|.
bool GetDERFromPEM(const std::string& pem_data,
                   const std::string& pem_type,
                   std::vector<uint8>* der_output) {
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

NetworkingPrivateCrypto::NetworkingPrivateCrypto() {
}

NetworkingPrivateCrypto::~NetworkingPrivateCrypto() {
}

bool NetworkingPrivateCrypto::VerifyCredentials(
    const std::string& certificate,
    const std::string& signature,
    const std::string& data,
    const std::string& connected_mac) {
  crypto::EnsureNSSInit();

  std::vector<uint8> cert_data;
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
      const_cast<unsigned char*>(kTrustedCAPublicKeyDER),
  trusted_ca_key_der_item.len = sizeof(kTrustedCAPublicKeyDER);
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

bool NetworkingPrivateCrypto::EncryptByteString(
    const std::vector<uint8>& pub_key_der,
    const std::string& data,
    std::vector<uint8>* encrypted_output) {
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

bool NetworkingPrivateCrypto::DecryptByteString(
    const std::string& private_key_pem,
    const std::vector<uint8>& encrypted_data,
    std::string* decrypted_output) {
  crypto::EnsureNSSInit();

  std::vector<uint8> private_key_data;
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
