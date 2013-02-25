// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/quic_data_writer.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;
using std::string;

namespace net {

const size_t kHashSize = 16;  // size of uint128 serialized

bool NullEncrypter::SetKey(StringPiece key) {
  return key.empty();
}

bool NullEncrypter::SetNoncePrefix(StringPiece nonce_prefix) {
  return nonce_prefix.empty();
}

QuicData* NullEncrypter::Encrypt(QuicPacketSequenceNumber /*sequence_number*/,
                                 StringPiece associated_data,
                                 StringPiece plaintext) {
  // TODO(rch): avoid buffer copy here
  string buffer = associated_data.as_string();
  plaintext.AppendToString(&buffer);
  uint128 hash = QuicUtils::FNV1a_128_Hash(buffer.data(), buffer.length());
  QuicDataWriter writer(plaintext.length() + kHashSize);
  writer.WriteUInt128(hash);
  writer.WriteBytes(plaintext.data(), plaintext.length());
  size_t len = writer.length();
  return new QuicData(writer.take(), len, true);
}

size_t NullEncrypter::GetKeySize() const {
  return 0;
}

size_t NullEncrypter::GetNoncePrefixSize() const {
  return 0;
}

size_t NullEncrypter::GetMaxPlaintextSize(size_t ciphertext_size) const {
  return ciphertext_size - kHashSize;
}

size_t NullEncrypter::GetCiphertextSize(size_t plaintext_size) const {
  return plaintext_size + kHashSize;
}

}  // namespace net
