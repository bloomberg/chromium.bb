// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"

#include "base/logging.h"

NetworkingPrivateCrypto::NetworkingPrivateCrypto() {
}

NetworkingPrivateCrypto::~NetworkingPrivateCrypto() {
}

bool NetworkingPrivateCrypto::VerifyCredentials(
    const std::string& certificate,
    const std::string& signature,
    const std::string& data,
    const std::string& connected_mac) {
  // https://crbug.com/393023
  NOTIMPLEMENTED();
  return false;
}

bool NetworkingPrivateCrypto::EncryptByteString(
    const std::vector<uint8>& pub_key_der,
    const std::string& data,
    std::vector<uint8>* encrypted_output) {
  // https://crbug.com/393023
  NOTIMPLEMENTED();
  return false;
}

bool NetworkingPrivateCrypto::DecryptByteString(
    const std::string& private_key_pem,
    const std::vector<uint8>& encrypted_data,
    std::string* decrypted_output) {
  // https://crbug.com/393023
  NOTIMPLEMENTED();
  return false;
}
