// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/prefix_set.h"

#include <algorithm>
#include <math.h>

#include "base/logging.h"
#include "base/metrics/histogram.h"

namespace {

// For |std::upper_bound()| to find a prefix w/in a vector of pairs.
bool PrefixLess(const std::pair<SBPrefix,size_t>& a,
                const std::pair<SBPrefix,size_t>& b) {
  return a.first < b.first;
}

}  // namespace

namespace safe_browsing {

PrefixSet::PrefixSet(const std::vector<SBPrefix>& prefixes) {
  if (prefixes.size()) {
    std::vector<SBPrefix> sorted(prefixes.begin(), prefixes.end());
    std::sort(sorted.begin(), sorted.end());

    // Lead with the first prefix.
    SBPrefix prev_prefix = sorted[0];
    size_t run_length = 0;
    index_.push_back(std::make_pair(prev_prefix, deltas_.size()));

    for (size_t i = 1; i < sorted.size(); ++i) {
      // Don't encode zero deltas.
      if (sorted[i] == prev_prefix)
        continue;

      // Calculate the delta.  |unsigned| is mandatory, because the
      // prefixes could be more than INT_MAX apart.
      unsigned delta = sorted[i] - prev_prefix;

      // New index ref if the delta is too wide, or if too many
      // consecutive deltas have been encoded.  Note that
      // |kMaxDelta| cannot itself be encoded.
      if (delta >= kMaxDelta || run_length >= kMaxRun) {
        index_.push_back(std::make_pair(sorted[i], deltas_.size()));
        run_length = 0;
      } else {
        // Continue the run of deltas.
        deltas_.push_back(static_cast<uint16>(delta));
        ++run_length;
      }

      prev_prefix = sorted[i];
    }

    // Send up some memory-usage stats.  Bits because fractional bytes
    // are weird.
    const size_t bits_used = index_.size() * sizeof(index_[0]) * CHAR_BIT +
        deltas_.size() * sizeof(deltas_[0]) * CHAR_BIT;
    const size_t unique_prefixes = index_.size() + deltas_.size();
    static const size_t kMaxBitsPerPrefix = sizeof(SBPrefix) * CHAR_BIT;
    UMA_HISTOGRAM_ENUMERATION("SB2.PrefixSetBitsPerPrefix",
                              bits_used / unique_prefixes,
                              kMaxBitsPerPrefix);
  }
}

PrefixSet::~PrefixSet() {}

bool PrefixSet::Exists(SBPrefix prefix) const {
  if (index_.empty())
    return false;

  // Find the first position after |prefix| in |index_|.
  std::vector<std::pair<SBPrefix,size_t> >::const_iterator
      iter = std::upper_bound(index_.begin(), index_.end(),
                              std::pair<SBPrefix,size_t>(prefix, 0),
                              PrefixLess);

  // |prefix| comes before anything that's in the set.
  if (iter == index_.begin())
    return false;

  // Capture the upper bound of our target entry's deltas.
  const size_t bound = (iter == index_.end() ? deltas_.size() : iter->second);

  // Back up to the entry our target is in.
  --iter;

  // All prefixes in |index_| are in the set.
  SBPrefix current = iter->first;
  if (current == prefix)
    return true;

  // Scan forward accumulating deltas while a match is possible.
  for (size_t di = iter->second; di < bound && current < prefix; ++di) {
    current += deltas_[di];
  }

  return current == prefix;
}

}  // namespace safe_browsing
