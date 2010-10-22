// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/rsa_private_key.h"

#include "base/logging.h"

namespace base {

// static
RSAPrivateKey* RSAPrivateKey::CreateWithParams(uint16 num_bits,
                                               bool permanent,
                                               bool sensitive) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
RSAPrivateKey* RSAPrivateKey::Create(uint16 num_bits) {
  return CreateWithParams(num_bits,
                          false /* not permanent */,
                          false /* not sensitive */);
}

// static
RSAPrivateKey* RSAPrivateKey::CreateSensitive(uint16 num_bits) {
  return CreateWithParams(num_bits,
                          true /* permanent */,
                          true /* sensitive */);
}

// static
RSAPrivateKey* RSAPrivateKey::CreateFromPrivateKeyInfoWithParams(
    const std::vector<uint8>& input, bool permanent, bool sensitive) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
RSAPrivateKey* RSAPrivateKey::CreateFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  return CreateFromPrivateKeyInfoWithParams(input,
                                            false /* not permanent */,
                                            false /* not sensitive */);
}

// static
RSAPrivateKey* RSAPrivateKey::CreateSensitiveFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  return CreateFromPrivateKeyInfoWithParams(input,
                                            true /* permanent */,
                                            true /* seneitive */);
}

// static
RSAPrivateKey* RSAPrivateKey::FindFromPublicKeyInfo(
    const std::vector<uint8>& input) {
  NOTIMPLEMENTED();
  return NULL;
}

RSAPrivateKey::RSAPrivateKey() {
}

RSAPrivateKey::~RSAPrivateKey() {
}

bool RSAPrivateKey::ExportPrivateKey(std::vector<uint8>* output) {
  NOTIMPLEMENTED();
  return false;
}

bool RSAPrivateKey::ExportPublicKey(std::vector<uint8>* output) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace base
