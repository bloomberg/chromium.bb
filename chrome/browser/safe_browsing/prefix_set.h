// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A read-only set implementation for |SBPrefix| items.  Prefixes are
// sorted and stored as 16-bit deltas from the previous prefix.  An
// index structure provides quick random access, and also handles
// cases where 16 bits cannot encode a delta.
//
// For example, the sequence {20, 25, 41, 65432, 150000, 160000} would
// be stored as:
//  A pair {20, 0} in |index_|.
//  5, 16, 65391 in |deltas_|.
//  A pair {150000, 3} in |index_|.
//  10000 in |deltas_|.
// |index_.size()| will be 2, |deltas_.size()| will be 4.
//
// This structure is intended for storage of sparse uniform sets of
// prefixes of a certain size.  As of this writing, my safe-browsing
// database contains:
//   653132 add prefixes
//   6446 are duplicates (from different chunks)
//   24301 w/in 2^8 of the prior prefix
//   622337 w/in 2^16 of the prior prefix
//   47 further than 2^16 from the prior prefix
// For this input, the memory usage is approximately 2 bytes per
// prefix, a bit over 1.2M.  The bloom filter used 25 bits per prefix,
// a bit over 1.9M on this data.
//
// Experimenting with random selections of the above data, storage
// size drops almost linearly as prefix count drops, until the index
// overhead starts to become a problem a bit under 200k prefixes.  The
// memory footprint gets worse than storing the raw prefix data around
// 75k prefixes.  Fortunately, the actual memory footprint also falls.
// If the prefix count increases the memory footprint should increase
// approximately linearly.  The worst-case would be 2^16 items all
// 2^16 apart, which would need 512k (versus 256k to store the raw
// data).
//
// TODO(shess): Write serialization code.  Something like this should
// work:
//         4 byte magic number
//         4 byte version number
//         4 byte |index_.size()|
//         4 byte |deltas_.size()|
//     n * 8 byte |&index_[0]..&index_[n]|
//     m * 2 byte |&deltas_[0]..&deltas_[m]|
//        16 byte digest

#ifndef CHROME_BROWSER_SAFE_BROWSING_PREFIX_SET_H_
#define CHROME_BROWSER_SAFE_BROWSING_PREFIX_SET_H_
#pragma once

#include <vector>

#include "chrome/browser/safe_browsing/safe_browsing_util.h"

namespace safe_browsing {

class PrefixSet {
 public:
  explicit PrefixSet(const std::vector<SBPrefix>& sorted_prefixes);
  ~PrefixSet();

  // |true| if |prefix| was in |prefixes| passed to the constructor.
  bool Exists(SBPrefix prefix) const;

  // Regenerate the vector of prefixes passed to the constructor into
  // |prefixes|.  Prefixes will be added in sorted order.
  void GetPrefixes(std::vector<SBPrefix>* prefixes);

 private:
  // Maximum delta that can be encoded in a 16-bit unsigned.
  static const unsigned kMaxDelta = 256 * 256;

  // Maximum number of consecutive deltas to encode before generating
  // a new index entry.  This helps keep the worst-case performance
  // for |Exists()| under control.
  static const size_t kMaxRun = 100;

  // Top-level index of prefix to offset in |deltas_|.  Each pair
  // indicates a base prefix and where the deltas from that prefix
  // begin in |deltas_|.  The deltas for a pair end at the next pair's
  // index into |deltas_|.
  std::vector<std::pair<SBPrefix,size_t> > index_;

  // Deltas which are added to the prefix in |index_| to generate
  // prefixes.  Deltas are only valid between consecutive items from
  // |index_|, or the end of |deltas_| for the last |index_| pair.
  std::vector<uint16> deltas_;

  DISALLOW_COPY_AND_ASSIGN(PrefixSet);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_PREFIX_SET_H_
