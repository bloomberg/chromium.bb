// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/bloom_filter.h"

#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

namespace {

// The Jenkins 96 bit mix function:
// http://www.concentric.net/~Ttwang/tech/inthash.htm
uint32 HashMix(BloomFilter::HashKey hash_key, uint32 c) {
  uint32 a = static_cast<uint32>(hash_key)       & 0xFFFFFFFF;
  uint32 b = static_cast<uint32>(hash_key >> 32) & 0xFFFFFFFF;

  a -= (b + c);  a ^= (c >> 13);
  b -= (c + a);  b ^= (a << 8);
  c -= (a + b);  c ^= (b >> 13);
  a -= (b + c);  a ^= (c >> 12);
  b -= (c + a);  b ^= (a << 16);
  c -= (a + b);  c ^= (b >> 5);
  a -= (b + c);  a ^= (c >> 3);
  b -= (c + a);  b ^= (a << 10);
  c -= (a + b);  c ^= (b >> 15);

  return c;
}

}  // namespace

// static
int BloomFilter::FilterSizeForKeyCount(int key_count) {
  const int default_min = BloomFilter::kBloomFilterMinSize;
  const int number_of_keys = std::max(key_count, default_min);
  return std::min(number_of_keys * BloomFilter::kBloomFilterSizeRatio,
                  BloomFilter::kBloomFilterMaxSize * 8);
}

// static
void BloomFilter::RecordFailure(FailureType failure_type) {
  UMA_HISTOGRAM_ENUMERATION("SB2.BloomFailure", failure_type,
                            FAILURE_FILTER_MAX);
}

BloomFilter::BloomFilter(int bit_size) {
  for (int i = 0; i < kNumHashKeys; ++i)
    hash_keys_.push_back(base::RandUint64());

  // Round up to the next boundary which fits bit_size.
  byte_size_ = (bit_size + 7) / 8;
  bit_size_ = byte_size_ * 8;
  DCHECK_LE(bit_size, bit_size_);  // strictly more bits.
  data_.reset(new char[byte_size_]);
  memset(data_.get(), 0, byte_size_);
}

BloomFilter::BloomFilter(char* data, int size, const HashKeys& keys)
    : hash_keys_(keys) {
  byte_size_ = size;
  bit_size_ = byte_size_ * 8;
  data_.reset(data);
}

BloomFilter::~BloomFilter() {
}

void BloomFilter::Insert(SBPrefix hash) {
  uint32 hash_uint32 = static_cast<uint32>(hash);
  for (size_t i = 0; i < hash_keys_.size(); ++i) {
    uint32 index = HashMix(hash_keys_[i], hash_uint32) % bit_size_;
    data_[index / 8] |= 1 << (index % 8);
  }
}

bool BloomFilter::Exists(SBPrefix hash) const {
  uint32 hash_uint32 = static_cast<uint32>(hash);
  for (size_t i = 0; i < hash_keys_.size(); ++i) {
    uint32 index = HashMix(hash_keys_[i], hash_uint32) % bit_size_;
    if (!(data_[index / 8] & (1 << (index % 8))))
      return false;
  }
  return true;
}

// static.
BloomFilter* BloomFilter::LoadFile(const FilePath& filter_name) {
  net::FileStream filter;

  if (filter.Open(filter_name,
                  base::PLATFORM_FILE_OPEN |
                  base::PLATFORM_FILE_READ) != net::OK) {
    RecordFailure(FAILURE_FILTER_READ_OPEN);
    return NULL;
  }

  // Make sure we have a file version that we can understand.
  int file_version;
  int bytes_read = filter.Read(reinterpret_cast<char*>(&file_version),
                               sizeof(file_version), NULL);
  if (bytes_read != sizeof(file_version) || file_version != kFileVersion) {
    RecordFailure(FAILURE_FILTER_READ_VERSION);
    return NULL;
  }

  // Get all the random hash keys.
  int num_keys;
  bytes_read = filter.Read(reinterpret_cast<char*>(&num_keys),
                           sizeof(num_keys), NULL);
  if (bytes_read != sizeof(num_keys) ||
      num_keys < 1 || num_keys > kNumHashKeys) {
    RecordFailure(FAILURE_FILTER_READ_NUM_KEYS);
    return NULL;
  }

  HashKeys hash_keys;
  for (int i = 0; i < num_keys; ++i) {
    HashKey key;
    bytes_read = filter.Read(reinterpret_cast<char*>(&key), sizeof(key), NULL);
    if (bytes_read != sizeof(key)) {
      RecordFailure(FAILURE_FILTER_READ_KEY);
      return NULL;
    }
    hash_keys.push_back(key);
  }

  // Read in the filter data, with sanity checks on min and max sizes.
  int64 remaining64 = filter.Available();
  if (remaining64 < kBloomFilterMinSize) {
    RecordFailure(FAILURE_FILTER_READ_DATA_MINSIZE);
    return NULL;
  } else if (remaining64 > kBloomFilterMaxSize) {
    RecordFailure(FAILURE_FILTER_READ_DATA_MAXSIZE);
    return NULL;
  }

  int byte_size = static_cast<int>(remaining64);
  scoped_array<char> data(new char[byte_size]);
  bytes_read = filter.Read(data.get(), byte_size, NULL);
  if (bytes_read < byte_size) {
    RecordFailure(FAILURE_FILTER_READ_DATA_SHORT);
    return NULL;
  } else if (bytes_read != byte_size) {
    RecordFailure(FAILURE_FILTER_READ_DATA);
    return NULL;
  }

  // We've read everything okay, commit the data.
  return new BloomFilter(data.release(), byte_size, hash_keys);
}

bool BloomFilter::WriteFile(const FilePath& filter_name) const {
  net::FileStream filter;

  if (filter.Open(filter_name,
                  base::PLATFORM_FILE_WRITE |
                  base::PLATFORM_FILE_CREATE_ALWAYS) != net::OK)
    return false;

  // Write the version information.
  int version = kFileVersion;
  int bytes_written = filter.Write(reinterpret_cast<char*>(&version),
                                   sizeof(version), NULL);
  if (bytes_written != sizeof(version))
    return false;

  // Write the number of random hash keys.
  int num_keys = static_cast<int>(hash_keys_.size());
  bytes_written = filter.Write(reinterpret_cast<char*>(&num_keys),
                               sizeof(num_keys), NULL);
  if (bytes_written != sizeof(num_keys))
    return false;

  for (int i = 0; i < num_keys; ++i) {
    bytes_written = filter.Write(reinterpret_cast<const char*>(&hash_keys_[i]),
                                 sizeof(hash_keys_[i]), NULL);
    if (bytes_written != sizeof(hash_keys_[i]))
      return false;
  }

  // Write the filter data.
  bytes_written = filter.Write(data_.get(), byte_size_, NULL);
  if (bytes_written != byte_size_)
    return false;

  return true;
}
