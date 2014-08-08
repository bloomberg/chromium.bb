// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/crypto_verify_impl.h"

#include "base/base64.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_credentials_getter.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_service_client.h"
#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"

namespace {

bool VerifyCredentials(const CryptoVerifyImpl::Credentials& credentials) {
  return networking_private_crypto::VerifyCredentials(credentials.certificate,
                                                      credentials.signed_data,
                                                      credentials.unsigned_data,
                                                      credentials.device_bssid);
}

}  // namespace

using extensions::NetworkingPrivateServiceClient;
using extensions::NetworkingPrivateCredentialsGetter;

NetworkingPrivateServiceClient::CryptoVerify*
NetworkingPrivateServiceClient::CryptoVerify::Create() {
  return new CryptoVerifyImpl();
}

CryptoVerifyImpl::CryptoVerifyImpl() {
}

CryptoVerifyImpl::~CryptoVerifyImpl() {
}

void CryptoVerifyImpl::VerifyDestination(const Credentials& credentials,
                                         bool* verified,
                                         std::string* error) {
  *verified = VerifyCredentials(credentials);
}

void CryptoVerifyImpl::VerifyAndEncryptCredentials(
    const std::string& network_guid,
    const Credentials& credentials,
    const VerifyAndEncryptCredentialsCallback& callback) {
  if (!VerifyCredentials(credentials)) {
    callback.Run("", "VerifyError");
    return;
  }

  scoped_ptr<NetworkingPrivateCredentialsGetter> credentials_getter(
      NetworkingPrivateCredentialsGetter::Create());

  // Start getting credentials. On Windows |callback| will be called
  // asynchronously on a different thread after |credentials_getter|
  // is deleted.
  credentials_getter->Start(network_guid, credentials.public_key, callback);
}

void CryptoVerifyImpl::VerifyAndEncryptData(
    const Credentials& credentials,
    const std::string& data,
    std::string* base64_encoded_ciphertext,
    std::string* error) {
  if (!VerifyCredentials(credentials)) {
    *error = "VerifyError";
    return;
  }

  std::vector<uint8> public_key_data(credentials.public_key.begin(),
                                     credentials.public_key.end());
  std::vector<uint8> ciphertext;
  if (!networking_private_crypto::EncryptByteString(
          public_key_data, data, &ciphertext)) {
    *error = "EncryptError";
    return;
  }

  base::Base64Encode(std::string(ciphertext.begin(), ciphertext.end()),
                     base64_encoded_ciphertext);
}
