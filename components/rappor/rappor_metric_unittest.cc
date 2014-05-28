// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/rappor_metric.h"

#include <stdlib.h>

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace rappor {

const RapporParameters kTestRapporParameters = {
    1 /* Num cohorts */,
    16 /* Bloom filter size bytes */,
    4 /* Bloom filter hash count */,
    PROBABILITY_75 /* Fake data probability */,
    PROBABILITY_50 /* Fake one probability */,
    PROBABILITY_75 /* One coin probability */,
    PROBABILITY_50 /* Zero coin probability */};

const RapporParameters kTestStatsRapporParameters = {
    1 /* Num cohorts */,
    50 /* Bloom filter size bytes */,
    4 /* Bloom filter hash count */,
    PROBABILITY_75 /* Fake data probability */,
    PROBABILITY_50 /* Fake one probability */,
    PROBABILITY_75 /* One coin probability */,
    PROBABILITY_50 /* Zero coin probability */};

// Check for basic syntax and use.
TEST(RapporMetricTest, BasicMetric) {
  RapporMetric testMetric("MyRappor", kTestRapporParameters, 0);
  testMetric.AddSample("Foo");
  testMetric.AddSample("Bar");
  EXPECT_EQ(0x80, testMetric.bytes()[1]);
}

TEST(RapporMetricTest, GetReport) {
  RapporMetric metric("MyRappor", kTestRapporParameters, 0);

  const ByteVector report = metric.GetReport(
      HmacByteVectorGenerator::GenerateEntropyInput());
  EXPECT_EQ(16u, report.size());
}

TEST(RapporMetricTest, GetReportStatistics) {
  RapporMetric metric("MyStatsRappor", kTestStatsRapporParameters, 0);

  for (char i = 0; i < 50; i++) {
    metric.AddSample(base::StringPrintf("%d", i));
  }
  const ByteVector real_bits = metric.bytes();
  const int real_bit_count = CountBits(real_bits);
  EXPECT_EQ(real_bit_count, 152);

  const std::string secret = HmacByteVectorGenerator::GenerateEntropyInput();
  const ByteVector report = metric.GetReport(secret);

  // For the bits we actually set in the Bloom filter, get a count of how
  // many of them reported true.
  ByteVector from_true_reports = report;
  // Real bits AND report bits.
  ByteVectorMerge(real_bits, real_bits, &from_true_reports);
  const int true_from_true_count = CountBits(from_true_reports);

  // For the bits we didn't set in the Bloom filter, get a count of how
  // many of them reported true.
  ByteVector from_false_reports = report;
  ByteVectorOr(real_bits, &from_false_reports);
  const int true_from_false_count =
      CountBits(from_false_reports) - real_bit_count;

  // The probability of a true bit being true after redaction =
  //   [fake_prob]*[fake_true_prob] + (1-[fake_prob]) =
  //   .75 * .5 + (1-.75) = .625
  // The probablity of a false bit being true after redaction =
  //   [fake_prob]*[fake_true_prob] = .375
  // The probability of a bit reporting true =
  //   [redacted_prob] * [one_coin_prob:.75] +
  //      (1-[redacted_prob]) * [zero_coin_prob:.5] =
  //   0.65625 for true bits
  //   0.59375 for false bits

  // stats.binom(152, 0.65625).ppf(0.000005) = 73
  EXPECT_GT(true_from_true_count, 73);
  // stats.binom(152, 0.65625).ppf(0.999995) = 124
  EXPECT_LE(true_from_true_count, 124);

  // stats.binom(248, 0.59375).ppf(.000005) = 113
  EXPECT_GT(true_from_false_count, 113);
  // stats.binom(248, 0.59375).ppf(.999995) = 181
  EXPECT_LE(true_from_false_count, 181);
}

}  // namespace rappor
