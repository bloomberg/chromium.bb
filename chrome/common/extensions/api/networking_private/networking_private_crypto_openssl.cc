// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"

#include <openssl/digest.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include "base/logging.h"
#include "crypto/openssl_util.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_openssl_types.h"
#include "net/cert/pem_tokenizer.h"

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

bool EncryptByteString(const std::vector<uint8_t>& pub_key_der,
                       const std::string& data,
                       std::vector<uint8_t>* encrypted_output) {
  crypto::EnsureOpenSSLInit();
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const uint8_t* ptr = pub_key_der.empty() ? NULL : &pub_key_der[0];
  const uint8_t* end = ptr + pub_key_der.size();
  crypto::ScopedRSA rsa(d2i_RSAPublicKey(NULL, &ptr, pub_key_der.size()));
  if (!rsa || ptr != end || RSA_size(rsa.get()) == 0) {
    LOG(ERROR) << "Failed to parse public key";
    return false;
  }

  scoped_ptr<uint8_t[]> rsa_output(new uint8_t[RSA_size(rsa.get())]);
  int encrypted_length =
      RSA_public_encrypt(data.size(),
                         reinterpret_cast<const uint8_t*>(data.data()),
                         rsa_output.get(),
                         rsa.get(),
                         RSA_PKCS1_PADDING);
  if (encrypted_length < 0) {
    LOG(ERROR) << "Error during decryption";
    return false;
  }
  encrypted_output->assign(rsa_output.get(),
                           rsa_output.get() + encrypted_length);
  return true;
}

bool DecryptByteString(const std::string& private_key_pem,
                       const std::vector<uint8_t>& encrypted_data,
                       std::string* decrypted_output) {
  crypto::EnsureOpenSSLInit();
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  std::vector<uint8_t> private_key_data;
  if (!GetDERFromPEM(private_key_pem, "PRIVATE KEY", &private_key_data)) {
    LOG(ERROR) << "Failed to parse private key PEM.";
    return false;
  }
  scoped_ptr<crypto::RSAPrivateKey> private_key(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(private_key_data));
  if (!private_key || !private_key->key()) {
    LOG(ERROR) << "Failed to parse private key DER.";
    return false;
  }

  crypto::ScopedRSA rsa(EVP_PKEY_get1_RSA(private_key->key()));
  if (!rsa || RSA_size(rsa.get()) == 0) {
    LOG(ERROR) << "Failed to get RSA key.";
    return false;
  }

  scoped_ptr<uint8_t[]> rsa_output(new uint8_t[RSA_size(rsa.get())]);
  int output_length = RSA_private_decrypt(encrypted_data.size(),
                                          &encrypted_data[0],
                                          rsa_output.get(),
                                          rsa.get(),
                                          RSA_PKCS1_PADDING);
  if (output_length < 0) {
    LOG(ERROR) << "Error during decryption.";
    return false;
  }
  decrypted_output->assign(reinterpret_cast<char*>(rsa_output.get()),
                           output_length);
  return true;
}

}  // namespace networking_private_crypto
