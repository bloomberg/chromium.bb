// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/entropy_provider.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/sha1.h"
#include "base/sys_byteorder.h"
#include "chrome/common/metrics/metrics_util.h"
#include "chrome/common/pref_names.h"

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

void PermuteMappingUsingRandomizationSeed(uint32 randomization_seed,
                                          std::vector<uint16>* mapping) {
  for (size_t i = 0; i < mapping->size(); ++i)
    (*mapping)[i] = static_cast<uint16>(i);

  SeededRandGenerator generator(randomization_seed);
  std::random_shuffle(mapping->begin(), mapping->end(), generator);
}

}  // namespace internal

SHA1EntropyProvider::SHA1EntropyProvider(const std::string& entropy_source)
    : entropy_source_(entropy_source) {
}

SHA1EntropyProvider::~SHA1EntropyProvider() {
}

double SHA1EntropyProvider::GetEntropyForTrial(
    const std::string& trial_name,
    uint32 randomization_seed) const {
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
    const std::string& trial_name,
    uint32 randomization_seed) const {
  if (randomization_seed == 0)
    randomization_seed = HashName(trial_name);

  std::vector<uint16> mapping(low_entropy_source_max_);
  internal::PermuteMappingUsingRandomizationSeed(randomization_seed, &mapping);

  return mapping[low_entropy_source_] /
         static_cast<double>(low_entropy_source_max_);
}

CachingPermutedEntropyProvider::CachingPermutedEntropyProvider(
    PrefService* local_state,
    uint16 low_entropy_source,
    size_t low_entropy_source_max)
    : local_state_(local_state),
      low_entropy_source_(low_entropy_source),
      low_entropy_source_max_(low_entropy_source_max) {
  DCHECK_LT(low_entropy_source, low_entropy_source_max);
  DCHECK_LE(low_entropy_source_max, std::numeric_limits<uint16>::max());
  ReadFromLocalState();
}

CachingPermutedEntropyProvider::~CachingPermutedEntropyProvider() {
}

// static
void CachingPermutedEntropyProvider::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kMetricsPermutedEntropyCache,
                               std::string());
}

// static
void CachingPermutedEntropyProvider::ClearCache(PrefService* local_state) {
  local_state->ClearPref(prefs::kMetricsPermutedEntropyCache);
}

double CachingPermutedEntropyProvider::GetEntropyForTrial(
    const std::string& trial_name,
    uint32 randomization_seed) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (randomization_seed == 0)
    randomization_seed = HashName(trial_name);

  uint16 value = 0;
  if (!FindValue(randomization_seed, &value)) {
    std::vector<uint16> mapping(low_entropy_source_max_);
    internal::PermuteMappingUsingRandomizationSeed(randomization_seed,
                                                   &mapping);
    value = mapping[low_entropy_source_];
    AddToCache(randomization_seed, value);
  }

  return value / static_cast<double>(low_entropy_source_max_);
}

void CachingPermutedEntropyProvider::ReadFromLocalState() const {
  const std::string base64_cache_data =
      local_state_->GetString(prefs::kMetricsPermutedEntropyCache);
  std::string cache_data;
  if (!base::Base64Decode(base64_cache_data, &cache_data) ||
      !cache_.ParseFromString(cache_data)) {
    local_state_->ClearPref(prefs::kMetricsPermutedEntropyCache);
    NOTREACHED();
  }
}

void CachingPermutedEntropyProvider::UpdateLocalState() const {
  std::string serialized;
  cache_.SerializeToString(&serialized);

  std::string base64_encoded;
  if (!base::Base64Encode(serialized, &base64_encoded)) {
    NOTREACHED();
    return;
  }
  local_state_->SetString(prefs::kMetricsPermutedEntropyCache, base64_encoded);
}

void CachingPermutedEntropyProvider::AddToCache(uint32 randomization_seed,
                                                uint16 value) const {
  PermutedEntropyCache::Entry* entry;
  const int kMaxSize = 25;
  if (cache_.entry_size() >= kMaxSize) {
    // If the cache is full, evict the first entry, swapping later entries in
    // to take its place. This effectively creates a FIFO cache, which is good
    // enough here because the expectation is that there shouldn't be more than
    // |kMaxSize| field trials at any given time, so eviction should happen very
    // rarely, only as new trials are introduced, evicting old expired trials.
    for (int i = 1; i < kMaxSize; ++i)
      cache_.mutable_entry()->SwapElements(i - 1, i);
    entry = cache_.mutable_entry(kMaxSize - 1);
  } else {
    entry = cache_.add_entry();
  }

  entry->set_randomization_seed(randomization_seed);
  entry->set_value(value);

  UpdateLocalState();
}

bool CachingPermutedEntropyProvider::FindValue(uint32 randomization_seed,
                                               uint16* value) const {
  for (int i = 0; i < cache_.entry_size(); ++i) {
    if (cache_.entry(i).randomization_seed() == randomization_seed) {
      *value = cache_.entry(i).value();
      return true;
    }
  }
  return false;
}

}  // namespace metrics
