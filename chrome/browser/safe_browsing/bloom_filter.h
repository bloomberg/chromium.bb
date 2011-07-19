// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple bloom filter. It uses a large number (20) of hashes to reduce the
// possibility of false positives. The bloom filter's hashing uses random keys
// in order to minimize the chance that a false positive for one user is a false
// positive for all.
//
// The bloom filter manages it serialization to disk with the following file
// format:
//         4 byte version number
//         4 byte number of hash keys (n)
//     n * 8 bytes of hash keys
// Remaining bytes are the filter data.

#ifndef CHROME_BROWSER_SAFE_BROWSING_BLOOM_FILTER_H_
#define CHROME_BROWSER_SAFE_BROWSING_BLOOM_FILTER_H_
#pragma once

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"

class FilePath;

class BloomFilter : public base::RefCountedThreadSafe<BloomFilter> {
 public:
  typedef uint64 HashKey;
  typedef std::vector<HashKey> HashKeys;

  // Constructs an empty filter with the given size.
  explicit BloomFilter(int bit_size);

  // Constructs a filter from serialized data. This object owns the memory and
  // will delete it on destruction.
  BloomFilter(char* data, int size, const HashKeys& keys);

  void Insert(SBPrefix hash);
  bool Exists(SBPrefix hash) const;

  const char* data() const { return data_.get(); }
  int size() const { return byte_size_; }

  // Loading and storing the filter from / to disk.
  static BloomFilter* LoadFile(const FilePath& filter_name);
  bool WriteFile(const FilePath& filter_name) const;

  // How many bits to use per item. See the design doc for more information.
  static const int kBloomFilterSizeRatio = 25;

  // Force a minimum size on the bloom filter to prevent a high false positive
  // hash request rate (in bytes).
  static const int kBloomFilterMinSize = 250000;

  // Force a maximum size on the bloom filter to avoid using too much memory
  // (in bytes).
  static const int kBloomFilterMaxSize = 3 * 1024 * 1024;

  // Use the above constants to calculate an appropriate size to pass
  // to the BloomFilter constructor based on the intended |key_count|.
  // TODO(shess): This is very clunky.  It would be cleaner to have
  // the constructor manage this, but at this time the unit and perf
  // tests wish to make their own calculations.
  static int FilterSizeForKeyCount(int key_count);

 private:
  friend class base::RefCountedThreadSafe<BloomFilter>;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBloomFilter, BloomFilterUse);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBloomFilter, BloomFilterFile);

  static const int kNumHashKeys = 20;
  static const int kFileVersion = 1;

  // Enumerate failures for histogramming purposes.  DO NOT CHANGE THE
  // ORDERING OF THESE VALUES.
  enum FailureType {
    FAILURE_FILTER_READ_OPEN,
    FAILURE_FILTER_READ_VERSION,
    FAILURE_FILTER_READ_NUM_KEYS,
    FAILURE_FILTER_READ_KEY,
    FAILURE_FILTER_READ_DATA_MINSIZE,
    FAILURE_FILTER_READ_DATA_MAXSIZE,
    FAILURE_FILTER_READ_DATA_SHORT,
    FAILURE_FILTER_READ_DATA,

    // Memory space for histograms is determined by the max.  ALWAYS
    // ADD NEW VALUES BEFORE THIS ONE.
    FAILURE_FILTER_MAX
  };

  static void RecordFailure(FailureType failure_type);

  ~BloomFilter();

  int byte_size_;  // size in bytes
  int bit_size_;   // size in bits
  scoped_array<char> data_;

  // Random keys used for hashing.
  HashKeys hash_keys_;

  DISALLOW_COPY_AND_ASSIGN(BloomFilter);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_BLOOM_FILTER_H_
