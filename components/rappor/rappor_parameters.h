// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RAPPOR_RAPPOR_PARAMETERS_H_
#define COMPONENTS_RAPPOR_RAPPOR_PARAMETERS_H_

#include <string>

namespace rappor {

enum Probability {
  PROBABILITY_75,    // 75%
  PROBABILITY_50,    // 50%
  PROBABILITY_25,    // 25%
};

// An object describing a rappor metric and the parameters used to generate it.
//
// For a full description of the rappor metrics, see
// http://www.chromium.org/developers/design-documents/rappor
struct RapporParameters {
  // Get a string representing the parameters, for DCHECK_EQ.
  std::string ToString() const;

  // The maximum number of cohorts we divide clients into.
  static const int kMaxCohorts;

  // The number of cohorts to divide the reports for this metric into.
  int num_cohorts;

  // The number of bytes stored in the Bloom filter.
  int bloom_filter_size_bytes;
  // The number of hash functions used in the Bloom filter.
  int bloom_filter_hash_function_count;

  // The probability that a bit will be redacted with fake data.
  Probability fake_prob;
  // The probability that a fake bit will be a one.
  Probability fake_one_prob;

  // The probability that a one bit in the redacted data reports as one.
  Probability one_coin_prob;
  // The probability that a zero bit in the redacted data reports as one.
  Probability zero_coin_prob;
};

}  // namespace rappor

#endif  // COMPONENTS_RAPPOR_RAPPOR_PARAMETERS_H_
