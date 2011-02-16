// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/encryptor.h"

#include <CommonCrypto/CommonCryptor.h>

#include "base/crypto/symmetric_key.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace base {

Encryptor::Encryptor()
    : key_(NULL),
      mode_(CBC) {
}

Encryptor::~Encryptor() {
}

bool Encryptor::Init(SymmetricKey* key, Mode mode, const std::string& iv) {
  DCHECK(key);
  DCHECK_EQ(CBC, mode) << "Unsupported mode of operation";
  CSSM_DATA raw_key = key->cssm_data();
  if (raw_key.Length != kCCKeySizeAES128 &&
      raw_key.Length != kCCKeySizeAES192 &&
      raw_key.Length != kCCKeySizeAES256)
    return false;
  if (iv.size() != kCCBlockSizeAES128)
    return false;

  key_ = key;
  mode_ = mode;
  iv_ = iv;
  return true;
}

bool Encryptor::Crypt(int /*CCOperation*/ op,
                      const std::string& input,
                      std::string* output) {
  DCHECK(key_);
  CSSM_DATA raw_key = key_->cssm_data();
  // CommonCryptor.h: "A general rule for the size of the output buffer which
  // must be provided by the caller is that for block ciphers, the output
  // length is never larger than the input length plus the block size."

  size_t output_size = input.size() + iv_.size();
  CCCryptorStatus err = CCCrypt(op,
                                kCCAlgorithmAES128,
                                kCCOptionPKCS7Padding,
                                raw_key.Data, raw_key.Length,
                                iv_.data(),
                                input.data(), input.size(),
                                WriteInto(output, output_size+1),
                                output_size,
                                &output_size);
  if (err) {
    output->resize(0);
    LOG(ERROR) << "CCCrypt returned " << err;
    return false;
  }
  output->resize(output_size);
  return true;
}

bool Encryptor::Encrypt(const std::string& plaintext, std::string* ciphertext) {
  return Crypt(kCCEncrypt, plaintext, ciphertext);
}

bool Encryptor::Decrypt(const std::string& ciphertext, std::string* plaintext) {
  return Crypt(kCCDecrypt, ciphertext, plaintext);
}

}  // namespace base
