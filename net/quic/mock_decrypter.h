// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_MOCK_DECRYPTER_H_
#define NET_QUIC_MOCK_DECRYPTER_H_

#include <cstddef>
#include <cstdint>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/third_party/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace net {

// A MockDecrypter is a QuicDecrypter that does no validation of
// the given ciphertext and returns it untouched, ignoring the
// associated data. This is used to allow fuzzing to mutate
// plaintext packets.
class MockDecrypter : public quic::QuicDecrypter {
 public:
  explicit MockDecrypter(quic::Perspective perspective);
  ~MockDecrypter() override {}

  // QuicDecrypter implementation
  bool SetKey(quic::QuicStringPiece key) override;
  bool SetNoncePrefix(quic::QuicStringPiece nonce_prefix) override;
  bool SetIV(quic::QuicStringPiece iv) override;
  bool SetPreliminaryKey(quic::QuicStringPiece key) override;
  bool SetDiversificationNonce(
      const quic::DiversificationNonce& nonce) override;
  bool DecryptPacket(quic::QuicTransportVersion version,
                     quic::QuicPacketNumber packet_number,
                     quic::QuicStringPiece associated_data,
                     quic::QuicStringPiece ciphertext,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override;
  size_t GetKeySize() const override;
  size_t GetIVSize() const override;
  quic::QuicStringPiece GetKey() const override;
  quic::QuicStringPiece GetNoncePrefix() const override;

  uint32_t cipher_id() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDecrypter);
};

}  // namespace net

#endif  // NET_QUIC_MOCK_DECRYPTER_H_
