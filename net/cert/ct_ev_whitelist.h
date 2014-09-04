// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_EV_WHITELIST_H_
#define NET_CERT_CT_EV_WHITELIST_H_

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "net/base/net_export.h"

namespace net {

namespace ct {

namespace internal {

// Abstraction over a stream of bits, to be read independently
// of the bytes they're packed into. Bits are read MSB-first from the stream.
// It is limited to 64-bit reads and is inefficient as a design choice - Since
// it is used infrequently to unpack the Golomb-coded EV certificate hashes
// whitelist in a blocking thread.
//
// This class is declared here so it can be tested.
class NET_EXPORT_PRIVATE BitStreamReader {
 public:
  BitStreamReader(const char* source, size_t length);

  // Reads unary-encoded number into |out|. Returns true if
  // there was at least one bit to read, false otherwise.
  bool ReadUnaryEncoding(uint64* out);
  // Reads |num_bits| (up to 64) into |out|. |out| is filled from the MSB to the
  // LSB. If |num_bits| is less than 64, the most significant |64 - num_bits|
  // bits are unused and left as zeros. Returns true if the stream had the
  // requested |num_bits|, false otherwise.
  bool ReadBits(uint8 num_bits, uint64* out);
  // Returns the number of bits left in the stream.
  uint64 BitsLeft() const;

 private:
  // Reads a single bit. Within a byte, the bits are read from the MSB to the
  // LSB.
  uint8 ReadBit();

  const char* const source_;
  const size_t length_;

  // Index of the byte currently being read from.
  uint64 current_byte_;
  // Index of the last bit read within |current_byte_|. Since bits are read
  // from the MSB to the LSB, this value is initialized to 7 and decremented
  // after each read.
  int8 current_bit_;
};

// Given a Golomb-coded list of hashes in |compressed_whitelist|, unpack into
// |uncompressed_list|. Returns true if the format of the compressed whitelist
// is valid, false otherwise.
NET_EXPORT_PRIVATE bool UncompressEVWhitelist(
    const std::string& compressed_whitelist,
    std::set<std::string>* uncompressed_list);

// Sets the given |ev_whitelist| into the global context.
// Note that |ev_whitelist| will contain the old EV whitelist data after this
// call as the implementation is using set::swap() to efficiently switch the
// sets.
NET_EXPORT_PRIVATE void SetEVWhitelistData(std::set<std::string>& ev_whitelist);

}  // namespace internal

// Sets the global EV certificate hashes whitelist from
// |compressed_whitelist_file| in the global context, after uncompressing it.
// If the data in |compressed_whitelist_file| is not a valid compressed
// whitelist, does nothing.
NET_EXPORT void SetEVWhitelistFromFile(
    const base::FilePath& compressed_whitelist_file);

// Returns true if the |certificate_hash| appears in the EV certificate hashes
// whitelist.
NET_EXPORT bool IsCertificateHashInWhitelist(
    const std::string& certificate_hash);

// Returns true if the global EV certificate hashes whitelist is non-empty,
// false otherwise.
NET_EXPORT bool HasValidEVWhitelist();

}  // namespace ct

}  // namespace net

#endif  // NET_CERT_CT_EV_WHITELIST_H_
