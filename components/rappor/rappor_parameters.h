// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RAPPOR_RAPPOR_PARAMETERS_H_
#define COMPONENTS_RAPPOR_RAPPOR_PARAMETERS_H_

#include <string>

namespace rappor {

// Levels of noise added to a sample.
enum NoiseLevel {
  NORMAL_NOISE = 0,
  NUM_NOISE_LEVELS,
};

// The type of data stored in a metric.
enum RapporType {
  // Generic metrics from UMA opt-in users.
  UMA_RAPPOR_TYPE = 0,
  // Generic metrics for SafeBrowsing users.
  SAFEBROWSING_RAPPOR_TYPE,
  // Deprecated: Use UMA_RAPPOR_TYPE for new metrics
  ETLD_PLUS_ONE_RAPPOR_TYPE,
  NUM_RAPPOR_TYPES,
  COARSE_RAPPOR_TYPE = SAFEBROWSING_RAPPOR_TYPE,
};

enum Probability {
  PROBABILITY_75,    // 75%
  PROBABILITY_50,    // 50%
  PROBABILITY_25,    // 25%
};


// A metric is reported when its reporting group is in the set of groups
// passed in to RapporService::Start()
enum RecordingGroup {
  // Metrics for UMA users.
  UMA_RAPPOR_GROUP = 1 << 0,
  // Metrics related to SafeBrowsing, for SafeBrowsing users.
  SAFEBROWSING_RAPPOR_GROUP = 1 << 1,
};


// An object describing noise probabilities for a noise level
struct NoiseParameters {
  // The probability that a bit will be redacted with fake data.
  Probability fake_prob;
  // The probability that a fake bit will be a one.
  Probability fake_one_prob;
  // The probability that a one bit in the redacted data reports as one.
  Probability one_coin_prob;
  // The probability that a zero bit in the redacted data reports as one.
  Probability zero_coin_prob;
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
  // This should divide kMaxCohorts evenly so that each cohort has an equal
  // probability of being assigned users.
  int num_cohorts;

  // The number of bytes stored in the Bloom filter.
  int bloom_filter_size_bytes;
  // The number of hash functions used in the Bloom filter.
  int bloom_filter_hash_function_count;

  // The level of noise to use.
  NoiseLevel noise_level;

  // The reporting level this metric is reported at.
  RecordingGroup recording_group;
};

namespace internal {

const NoiseParameters kNoiseParametersForLevel[NUM_NOISE_LEVELS] = {
    // NORMAL_NOISE
    {
     rappor::PROBABILITY_50 /* Fake data probability */,
     rappor::PROBABILITY_50 /* Fake one probability */,
     rappor::PROBABILITY_75 /* One coin probability */,
     rappor::PROBABILITY_25 /* Zero coin probability */,
    },
};

const RapporParameters kRapporParametersForType[NUM_RAPPOR_TYPES] = {
    // UMA_RAPPOR_TYPE
    {128 /* Num cohorts */,
     4 /* Bloom filter size bytes */,
     2 /* Bloom filter hash count */,
     rappor::NORMAL_NOISE /* Noise level */,
     UMA_RAPPOR_GROUP /* Recording group */},
    // SAFEBROWSING_RAPPOR_TYPE
    {128 /* Num cohorts */,
     1 /* Bloom filter size bytes */,
     2 /* Bloom filter hash count */,
     rappor::NORMAL_NOISE /* Noise level */,
     SAFEBROWSING_RAPPOR_GROUP /* Recording group */},
    // ETLD_PLUS_ONE_RAPPOR_TYPE
    {128 /* Num cohorts */,
     16 /* Bloom filter size bytes */,
     2 /* Bloom filter hash count */,
     rappor::NORMAL_NOISE /* Noise level */,
     UMA_RAPPOR_GROUP /* Recording group */},
};

}  // namespace internal

}  // namespace rappor

#endif  // COMPONENTS_RAPPOR_RAPPOR_PARAMETERS_H_
