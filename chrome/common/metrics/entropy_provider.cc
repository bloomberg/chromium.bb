// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/entropy_provider.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/sha1.h"
#include "base/sys_byteorder.h"
#include "chrome/common/metrics/metrics_util.h"

namespace metrics {

namespace internal {

SeededRandGenerator::SeededRandGenerator(uint32 seed) {
  mersenne_twister_.init_genrand(seed);
}

SeededRandGenerator::~SeededRandGenerator() {
}

uint32 SeededRandGenerator::operator()(uint32 range) {
  // Based on base::RandGenerator().
  DCHECK_GT(range, 0u);

  // We must discard random results above this number, as they would
  // make the random generator non-uniform (consider e.g. if
  // MAX_UINT64 was 7 and |range| was 5, then a result of 1 would be twice
  // as likely as a result of 3 or 4).
  uint32 max_acceptable_value =
      (std::numeric_limits<uint32>::max() / range) * range - 1;

  uint32 value;
  do {
    value = mersenne_twister_.genrand_int32();
  } while (value > max_acceptable_value);

  return value % range;
}

void PermuteMappingUsingTrialName(const std::string& trial_name,
                                  std::vector<uint16>* mapping) {
  for (size_t i = 0; i < mapping->size(); ++i)
    (*mapping)[i] = static_cast<uint16>(i);

  SeededRandGenerator generator(HashName(trial_name));
  std::random_shuffle(mapping->begin(), mapping->end(), generator);
}

}  // namespace internal

SHA1EntropyProvider::SHA1EntropyProvider(const std::string& entropy_source)
    : entropy_source_(entropy_source) {
}

SHA1EntropyProvider::~SHA1EntropyProvider() {
}

double SHA1EntropyProvider::GetEntropyForTrial(
    const std::string& trial_name) const {
  // Given enough input entropy, SHA-1 will produce a uniformly random spread
  // in its output space. In this case, the input entropy that is used is the
  // combination of the original |entropy_source_| and the |trial_name|.
  //
  // Note: If |entropy_source_| has very low entropy, such as 13 bits or less,
  // it has been observed that this method does not result in a uniform
  // distribution given the same |trial_name|. When using such a low entropy
  // source, PermutedEntropyProvider should be used instead.
  std::string input(entropy_source_ + trial_name);
  unsigned char sha1_hash[base::kSHA1Length];
  base::SHA1HashBytes(reinterpret_cast<const unsigned char*>(input.c_str()),
                      input.size(),
                      sha1_hash);

  uint64 bits;
  COMPILE_ASSERT(sizeof(bits) < sizeof(sha1_hash), need_more_data);
  memcpy(&bits, sha1_hash, sizeof(bits));
  bits = base::ByteSwapToLE64(bits);

  return base::BitsToOpenEndedUnitInterval(bits);
}

PermutedEntropyProvider::PermutedEntropyProvider(
    uint16 low_entropy_source,
    size_t low_entropy_source_max)
    : low_entropy_source_(low_entropy_source),
      low_entropy_source_max_(low_entropy_source_max) {
  DCHECK_LT(low_entropy_source, low_entropy_source_max);
  DCHECK_LE(low_entropy_source_max, std::numeric_limits<uint16>::max());
}

PermutedEntropyProvider::~PermutedEntropyProvider() {
}

double PermutedEntropyProvider::GetEntropyForTrial(
    const std::string& trial_name) const {
  std::vector<uint16> mapping(low_entropy_source_max_);
  internal::PermuteMappingUsingTrialName(trial_name, &mapping);

  return mapping[low_entropy_source_] /
         static_cast<double>(low_entropy_source_max_);
}

}  // namespace metrics
