// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/null_decrypter.h"
#include "net/quic/quic_utils.h"
#include "net/quic/quic_data_reader.h"

using base::StringPiece;
using std::string;

namespace net {

bool NullDecrypter::SetKey(StringPiece key) {
  return key.empty();
}

bool NullDecrypter::SetNoncePrefix(StringPiece nonce_prefix) {
  return nonce_prefix.empty();
}

bool NullDecrypter::Decrypt(StringPiece /*nonce*/,
                            StringPiece associated_data,
                            StringPiece ciphertext,
                            unsigned char* output,
                            size_t* output_length) {
  QuicDataReader reader(ciphertext.data(), ciphertext.length());

  uint128 hash;
  if (!reader.ReadUInt128(&hash)) {
    return false;
  }

  StringPiece plaintext = reader.ReadRemainingPayload();

  // TODO(rch): avoid buffer copy here
  string buffer = associated_data.as_string();
  plaintext.AppendToString(&buffer);

  if (hash != QuicUtils::FNV1a_128_Hash(buffer.data(), buffer.length())) {
    return false;
  }
  memcpy(output, plaintext.data(), plaintext.length());
  *output_length = plaintext.length();
  return true;
}

QuicData* NullDecrypter::DecryptPacket(QuicPacketSequenceNumber /*seq_number*/,
                                       StringPiece associated_data,
                                       StringPiece ciphertext) {
  // It's worth duplicating |Decrypt|, above, in order to save a copy by using
  // the shared-data QuicData constructor directly.
  QuicDataReader reader(ciphertext.data(), ciphertext.length());

  uint128 hash;
  if (!reader.ReadUInt128(&hash)) {
    return NULL;
  }

  StringPiece plaintext = reader.ReadRemainingPayload();

  // TODO(rch): avoid buffer copy here
  string buffer = associated_data.as_string();
  plaintext.AppendToString(&buffer);

  if (hash != QuicUtils::FNV1a_128_Hash(buffer.data(), buffer.length())) {
    return NULL;
  }
  return new QuicData(plaintext.data(), plaintext.length());
}

StringPiece NullDecrypter::GetKey() const {
  return StringPiece();
}

StringPiece NullDecrypter::GetNoncePrefix() const {
  return StringPiece();
}

}  // namespace net
