// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_ENTROPY_PROVIDER_H_
#define CHROME_COMMON_METRICS_ENTROPY_PROVIDER_H_

#include <functional>
#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/metrics/field_trial.h"
#include "third_party/mt19937ar/mt19937ar.h"

namespace metrics {

// Internals of entropy_provider.cc exposed for testing.
namespace internal {

// A functor that generates random numbers based on a seed, using the Mersenne
// Twister algorithm. Suitable for use with std::random_shuffle().
struct SeededRandGenerator : std::unary_function<uint32, uint32> {
  explicit SeededRandGenerator(uint32 seed);
  ~SeededRandGenerator();

  // Returns a random number in range [0, range).
  uint32 operator()(uint32 range);

  MersenneTwister mersenne_twister_;
};

// Fills |mapping| to create a bijection of values in the range of
// [0, |mapping.size()|), permuted based on |trial_name|.
void PermuteMappingUsingTrialName(const std::string& trial_name,
                                  std::vector<uint16>* mapping);

}  // namespace internal

// SHA1EntropyProvider is an entropy provider suitable for high entropy
// sources. It works by taking the first 64 bits of the SHA1 hash of the
// entropy source concatenated with the trial name and using that for the
// final entropy value.
class SHA1EntropyProvider : public base::FieldTrial::EntropyProvider {
 public:
  // Creates a SHA1EntropyProvider with the given |entropy_source|, which
  // should contain a large amount of entropy - for example, a textual
  // representation of a persistent randomly-generated 128-bit value.
  explicit SHA1EntropyProvider(const std::string& entropy_source);
  virtual ~SHA1EntropyProvider();

  // base::FieldTrial::EntropyProvider implementation:
  virtual double GetEntropyForTrial(const std::string& trial_name) const
      OVERRIDE;

 private:
  std::string entropy_source_;

  DISALLOW_COPY_AND_ASSIGN(SHA1EntropyProvider);
};

// PermutedEntropyProvider is an entropy provider suitable for low entropy
// sources (below 16 bits). It uses the field trial name to generate a
// permutation of a mapping array from an initial entropy value to a new value.
// Note: This provider's performance is O(2^n), where n is the number of bits
// in the entropy source.
class PermutedEntropyProvider : public base::FieldTrial::EntropyProvider {
 public:
  // Creates a PermutedEntropyProvider with the given |low_entropy_source|,
  // which should have a value in the range of [0, low_entropy_source_max).
  PermutedEntropyProvider(uint16 low_entropy_source,
                          size_t low_entropy_source_max);
  virtual ~PermutedEntropyProvider();

  // base::FieldTrial::EntropyProvider implementation:
  virtual double GetEntropyForTrial(const std::string& trial_name) const
      OVERRIDE;

 private:
  uint16 low_entropy_source_;
  size_t low_entropy_source_max_;

  DISALLOW_COPY_AND_ASSIGN(PermutedEntropyProvider);
};

}  // namespace metrics

#endif  // CHROME_COMMON_METRICS_ENTROPY_PROVIDER_H_
