// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_ev_whitelist.h"

#include <set>

#include "base/big_endian.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"

namespace net {

namespace ct {

namespace {
base::LazyInstance<std::set<std::string> >::Leaky g_current_ev_whitelist =
    LAZY_INSTANCE_INITIALIZER;

const uint8 kCertHashLengthBits = 64;     // 8 bytes
const uint64 kGolombMParameterBits = 47;  // 2^47
}  // namespace

namespace internal {

BitStreamReader::BitStreamReader(const char* source, size_t length)
    : source_(source), length_(length), current_byte_(0), current_bit_(7) {
}

bool BitStreamReader::ReadUnaryEncoding(uint64* out) {
  uint64 res = 0;
  if (BitsLeft() == 0)
    return false;

  while ((BitsLeft() > 0) && ReadBit())
    res++;

  *out = res;
  return true;
}

bool BitStreamReader::ReadBits(uint8 num_bits, uint64* out) {
  if (num_bits > 64)
    return false;

  if (BitsLeft() < num_bits)
    return false;

  uint64 res = 0;
  for (uint8 i = 0; i < num_bits; ++i)
    res |= (static_cast<uint64>(ReadBit()) << (num_bits - (i + 1)));

  *out = res;
  return true;
}

uint64 BitStreamReader::BitsLeft() const {
  if (current_byte_ == length_)
    return 0;
  return (length_ - (current_byte_ + 1)) * 8 + current_bit_ + 1;
}

uint8 BitStreamReader::ReadBit() {
  DCHECK_GT(BitsLeft(), 0u);
  DCHECK(current_bit_ < 8 && current_bit_ >= 0);
  uint8 res = (source_[current_byte_] & (1 << current_bit_)) >> current_bit_;
  current_bit_--;
  if (current_bit_ < 0) {
    current_byte_++;
    current_bit_ = 7;
  }

  return res;
}

bool UncompressEVWhitelist(const std::string& compressed_whitelist,
                           std::set<std::string>* uncompressed_list) {
  BitStreamReader reader(compressed_whitelist.data(),
                         compressed_whitelist.size());
  std::set<std::string> result;

  VLOG(1) << "Uncompressing EV whitelist of size "
          << compressed_whitelist.size();
  uint64 curr_hash(0);
  if (!reader.ReadBits(kCertHashLengthBits, &curr_hash)) {
    VLOG(1) << "Failed reading first hash.";
    return false;
  }
  char hash_bytes[8];
  base::WriteBigEndian(hash_bytes, curr_hash);
  result.insert(std::string(hash_bytes, 8));
  static const uint64 M = static_cast<uint64>(1) << kGolombMParameterBits;

  while (reader.BitsLeft() > kGolombMParameterBits) {
    uint64 read_prefix = 0;
    if (!reader.ReadUnaryEncoding(&read_prefix)) {
      VLOG(1) << "Failed reading unary-encoded prefix.";
      return false;
    }

    uint64 r = 0;
    if (!reader.ReadBits(kGolombMParameterBits, &r)) {
      VLOG(1) << "Failed reading " << kGolombMParameterBits << " bits.";
      return false;
    }

    uint64 curr_diff = read_prefix * M + r;
    curr_hash += curr_diff;

    base::WriteBigEndian(hash_bytes, curr_hash);
    result.insert(std::string(hash_bytes, 8));
  }

  uncompressed_list->swap(result);
  return true;
}

void SetEVWhitelistData(std::set<std::string>& ev_whitelist) {
  g_current_ev_whitelist.Get().swap(ev_whitelist);
}

}  // namespace internal

void SetEVWhitelistFromFile(const base::FilePath& compressed_whitelist_file) {
  VLOG(1) << "Setting EV whitelist from file: "
          << compressed_whitelist_file.value();
  std::string compressed_list;
  if (!base::ReadFileToString(compressed_whitelist_file, &compressed_list)) {
    VLOG(1) << "Failed reading from " << compressed_whitelist_file.value();
    return;
  }

  std::set<std::string> uncompressed_list;
  if (!internal::UncompressEVWhitelist(compressed_list, &uncompressed_list)) {
    VLOG(1) << "Failed uncompressing.";
    return;
  }
  VLOG(1) << "Uncompressing succeeded, hashes: " << uncompressed_list.size();

  internal::SetEVWhitelistData(uncompressed_list);
}

bool IsCertificateHashInWhitelist(const std::string& certificate_hash) {
  const std::set<std::string>& current_ev_whitelist =
      g_current_ev_whitelist.Get();
  return current_ev_whitelist.find(certificate_hash) !=
         current_ev_whitelist.end();
}

bool HasValidEVWhitelist() {
  return !g_current_ev_whitelist.Get().empty();
}

}  // namespace ct

}  // namespace net
