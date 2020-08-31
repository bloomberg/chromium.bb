// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/mock_decrypter.h"

#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_bug_tracker.h"

using quic::DiversificationNonce;
using quic::Perspective;
using quic::QuicPacketNumber;
using quiche::QuicheStringPiece;

namespace net {

namespace {

const size_t kPaddingSize = 12;

}  // namespace

MockDecrypter::MockDecrypter(Perspective perspective) {}

bool MockDecrypter::SetKey(quiche::QuicheStringPiece key) {
  return key.empty();
}

bool MockDecrypter::SetHeaderProtectionKey(quiche::QuicheStringPiece key) {
  return key.empty();
}

std::string MockDecrypter::GenerateHeaderProtectionMask(
    quic::QuicDataReader* sample_reader) {
  return std::string(5, 0);
}

bool MockDecrypter::SetNoncePrefix(quiche::QuicheStringPiece nonce_prefix) {
  return nonce_prefix.empty();
}

bool MockDecrypter::SetIV(quiche::QuicheStringPiece iv) {
  return iv.empty();
}

bool MockDecrypter::SetPreliminaryKey(quiche::QuicheStringPiece key) {
  QUIC_BUG << "Should not be called";
  return false;
}

bool MockDecrypter::SetDiversificationNonce(const DiversificationNonce& nonce) {
  QUIC_BUG << "Should not be called";
  return true;
}

bool MockDecrypter::DecryptPacket(uint64_t /*packet_number*/,
                                  quiche::QuicheStringPiece associated_data,
                                  quiche::QuicheStringPiece ciphertext,
                                  char* output,
                                  size_t* output_length,
                                  size_t max_output_length) {
  if (ciphertext.length() < kPaddingSize) {
    return false;
  }
  size_t plaintext_size = ciphertext.length() - kPaddingSize;
  if (plaintext_size > max_output_length) {
    return false;
  }

  memcpy(output, ciphertext.data(), plaintext_size);
  *output_length = plaintext_size;
  return true;
}

size_t MockDecrypter::GetKeySize() const {
  return 0;
}

size_t MockDecrypter::GetNoncePrefixSize() const {
  return 0;
}

size_t MockDecrypter::GetIVSize() const {
  return 0;
}

quiche::QuicheStringPiece MockDecrypter::GetKey() const {
  return quiche::QuicheStringPiece();
}

quiche::QuicheStringPiece MockDecrypter::GetNoncePrefix() const {
  return quiche::QuicheStringPiece();
}

uint32_t MockDecrypter::cipher_id() const {
  return 0;
}

}  // namespace net
