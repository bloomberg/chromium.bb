// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/observation_buffer.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/nqe/network_quality_observation.h"
#include "net/nqe/network_quality_observation_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace nqe {

namespace internal {

namespace {

// Verify that the buffer size is never exceeded.
TEST(NetworkQualityObservationBufferTest, BoundedBuffer) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  ObservationBuffer observation_buffer(&params, &tick_clock, 1.0, 1.0);
  const base::TimeTicks now =
      base::TimeTicks() + base::TimeDelta::FromSeconds(1);
  for (int i = 1; i <= 1000; ++i) {
    observation_buffer.AddObservation(
        Observation(i, now, INT32_MIN, NETWORK_QUALITY_OBSERVATION_SOURCE_TCP));
    // The number of entries should be at most the maximum buffer size.
    EXPECT_GE(300u, observation_buffer.Size());
  }
}

// Test disabled on OS_WIN to avoid linking errors when calling
// SetTickClockForTesting.
// TODO(tbansal): crbug.com/651963. Pass the clock through NQE's constructor.
#if !defined(OS_WIN)
// Verify that the percentiles are monotonically non-decreasing when a weight is
// applied.
TEST(NetworkQualityObservationBufferTest, GetPercentileWithWeights) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));

  ObservationBuffer observation_buffer(&params, &tick_clock, 0.98, 1.0);
  const base::TimeTicks now = tick_clock.NowTicks();
  for (int i = 1; i <= 100; ++i) {
    tick_clock.Advance(base::TimeDelta::FromSeconds(1));
    observation_buffer.AddObservation(
        Observation(i, tick_clock.NowTicks(), INT32_MIN,
                    NETWORK_QUALITY_OBSERVATION_SOURCE_TCP));
  }
  EXPECT_EQ(100U, observation_buffer.Size());

  int32_t result_lowest = INT32_MAX;
  int32_t result_highest = INT32_MIN;

  for (int i = 1; i <= 100; ++i) {
    size_t observations_count = 0;
    // Verify that i'th percentile is more than i-1'th percentile.
    base::Optional<int32_t> result_i = observation_buffer.GetPercentile(
        now, INT32_MIN, i, &observations_count);
    EXPECT_EQ(100u, observations_count);
    ASSERT_TRUE(result_i.has_value());
    result_lowest = std::min(result_lowest, result_i.value());

    result_highest = std::max(result_highest, result_i.value());

    base::Optional<int32_t> result_i_1 = observation_buffer.GetPercentile(
        now, INT32_MIN, i - 1, &observations_count);
    EXPECT_EQ(100u, observations_count);
    ASSERT_TRUE(result_i_1.has_value());

    EXPECT_LE(result_i_1.value(), result_i.value());
  }
  EXPECT_LT(result_lowest, result_highest);
}
#endif

// Verifies that the percentiles are correctly computed. All observations have
// the same timestamp.
TEST(NetworkQualityObservationBufferTest, PercentileSameTimestamps) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  ObservationBuffer buffer(&params, &tick_clock, 0.5, 1.0);
  ASSERT_EQ(0u, buffer.Size());
  ASSERT_LT(0u, buffer.Capacity());

  const base::TimeTicks now = tick_clock.NowTicks();

  size_t observations_count = 0;
  // Percentiles should be unavailable when no observations are available.
  EXPECT_FALSE(
      buffer
          .GetPercentile(base::TimeTicks(), INT32_MIN, 50,
                         &observations_count)
          .has_value());
  EXPECT_EQ(0u, observations_count);

  // Insert samples from {1,2,3,..., 100}. First insert odd samples, then even
  // samples. This helps in verifying that the order of samples does not matter.
  for (int i = 1; i <= 99; i += 2) {
    buffer.AddObservation(Observation(i, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
    EXPECT_TRUE(buffer.GetPercentile(base::TimeTicks(), INT32_MIN, 50, nullptr)
                    .has_value());
    ASSERT_EQ(static_cast<size_t>(i / 2 + 1), buffer.Size());
  }

  for (int i = 2; i <= 100; i += 2) {
    buffer.AddObservation(Observation(i, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
    EXPECT_TRUE(buffer.GetPercentile(base::TimeTicks(), INT32_MIN, 50, nullptr)
                    .has_value());
    ASSERT_EQ(static_cast<size_t>(i / 2 + 50), buffer.Size());
  }

  ASSERT_EQ(100u, buffer.Size());

  for (int i = 0; i <= 100; ++i) {
    // Checks if the difference between actual result and the computed result is
    // less than 1. This is required because computed percentiles may be
    // slightly different from what is expected due to floating point
    // computation errors and integer rounding off errors.
    base::Optional<int32_t> result = buffer.GetPercentile(
        base::TimeTicks(), INT32_MIN, i, &observations_count);
    EXPECT_EQ(100u, observations_count);
    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), i, 1.0);
  }

  EXPECT_FALSE(
      buffer
          .GetPercentile(now + base::TimeDelta::FromSeconds(1), INT32_MIN, 50,
                         &observations_count)
          .has_value());
  EXPECT_EQ(0u, observations_count);

  // Percentiles should be unavailable when no observations are available.
  buffer.Clear();
  EXPECT_FALSE(
      buffer
          .GetPercentile(base::TimeTicks(), INT32_MIN, 50,
                         &observations_count)
          .has_value());
  EXPECT_EQ(0u, observations_count);
}

// Verifies that the percentiles are correctly computed. Observations have
// different timestamps with half the observations being very old and the rest
// of them being very recent. Percentiles should factor in recent observations
// much more heavily than older samples.
TEST(NetworkQualityObservationBufferTest, PercentileDifferentTimestamps) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  ObservationBuffer buffer(&params, &tick_clock, 0.5, 1.0);
  const base::TimeTicks now = tick_clock.NowTicks();
  const base::TimeTicks very_old = now - base::TimeDelta::FromDays(7);

  size_t observations_count;

  // Network quality should be unavailable when no observations are available.
  EXPECT_FALSE(
      buffer
          .GetPercentile(base::TimeTicks(), INT32_MIN, 50,
                         &observations_count)
          .has_value());
  EXPECT_EQ(0u, observations_count);

  // First 50 samples have very old timestamps.
  for (int i = 1; i <= 50; ++i) {
    buffer.AddObservation(Observation(i, very_old, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
  }

  // Next 50 (i.e., from 51 to 100) have recent timestamps.
  for (int i = 51; i <= 100; ++i) {
    buffer.AddObservation(Observation(i, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
  }

  // Older samples have very little weight. So, all percentiles are >= 51
  // (lowest value among recent observations).
  for (int i = 1; i < 100; ++i) {
    // Checks if the difference between the two integers is less than 1. This is
    // required because computed percentiles may be slightly different from
    // what is expected due to floating point computation errors and integer
    // rounding off errors.
    base::Optional<int32_t> result =
        buffer.GetPercentile(very_old, INT32_MIN, i, &observations_count);
    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 51 + 0.49 * i, 1);
    EXPECT_EQ(100u, observations_count);
  }

  EXPECT_FALSE(buffer.GetPercentile(now + base::TimeDelta::FromSeconds(1),
                                    INT32_MIN, 50, &observations_count));
  EXPECT_EQ(0u, observations_count);
}

// Verifies that the percentiles are correctly computed. All observations have
// same timestamp with half the observations taken at low RSSI, and half the
// observations with high RSSI. Percentiles should be computed based on the
// current RSSI and the RSSI of the observations.
TEST(NetworkQualityObservationBufferTest, PercentileDifferentRSSI) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  ObservationBuffer buffer(&params, &tick_clock, 1.0, 0.5);
  const base::TimeTicks now = tick_clock.NowTicks();
  int32_t high_rssi = 0;
  int32_t low_rssi = -100;

  // Network quality should be unavailable when no observations are available.
  EXPECT_FALSE(buffer.GetPercentile(base::TimeTicks(), INT32_MIN, 50, nullptr)
                   .has_value());

  // First 50 samples have very low RSSI.
  for (int i = 1; i <= 50; ++i) {
    buffer.AddObservation(
        Observation(i, now, low_rssi, NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
  }

  // Next 50 (i.e., from 51 to 100) have high RSSI.
  for (int i = 51; i <= 100; ++i) {
    buffer.AddObservation(Observation(i, now, high_rssi,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
  }

  // When the current RSSI is |high_rssi|, higher weight should be assigned
  // to observations that were taken at |high_rssi|.
  for (int i = 1; i < 100; ++i) {
    base::Optional<int32_t> result =
        buffer.GetPercentile(now, high_rssi, i, nullptr);
    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), 51 + 0.49 * i, 1);
  }

  // When the current RSSI is |low_rssi|, higher weight should be assigned
  // to observations that were taken at |low_rssi|.
  for (int i = 1; i < 100; ++i) {
    base::Optional<int32_t> result =
        buffer.GetPercentile(now, low_rssi, i, nullptr);
    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), i / 2, 1);
  }
}

// Verifies that the percentiles are correctly computed when some of the
// observation sources are disallowed. All observations have the same timestamp.
TEST(NetworkQualityObservationBufferTest, RemoveObservations) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));

  ObservationBuffer buffer(&params, &tick_clock, 0.5, 1.0);
  const base::TimeTicks now = tick_clock.NowTicks();

  // Insert samples from {1,2,3,..., 100}. First insert odd samples, then even
  // samples. This helps in verifying that the order of samples does not matter.
  for (int i = 1; i <= 99; i += 2) {
    buffer.AddObservation(Observation(i, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
  }
  EXPECT_EQ(50u, buffer.Size());

  // Add samples for TCP and QUIC observations which should not be taken into
  // account when computing the percentile.
  for (int i = 1; i <= 99; i += 2) {
    buffer.AddObservation(Observation(10000, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_TCP));
    buffer.AddObservation(Observation(10000, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC));
  }
  EXPECT_EQ(150u, buffer.Size());

  for (int i = 2; i <= 100; i += 2) {
    buffer.AddObservation(Observation(i, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));
  }
  EXPECT_EQ(200u, buffer.Size());

  bool deleted_observation_sources[NETWORK_QUALITY_OBSERVATION_SOURCE_MAX] = {
      false};

  // Since all entries in |deleted_observation_sources| are set to false, no
  // observations should be deleted.
  buffer.RemoveObservationsWithSource(deleted_observation_sources);
  EXPECT_EQ(200u, buffer.Size());

  // 50 TCP and 50 QUIC observations should be deleted.
  deleted_observation_sources[NETWORK_QUALITY_OBSERVATION_SOURCE_TCP] = true;
  deleted_observation_sources[NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC] = true;
  buffer.RemoveObservationsWithSource(deleted_observation_sources);
  EXPECT_EQ(100u, buffer.Size());

  for (int i = 0; i <= 100; ++i) {
    // Checks if the difference between the two integers is less than 1. This is
    // required because computed percentiles may be slightly different from
    // what is expected due to floating point computation errors and integer
    // rounding off errors.
    base::Optional<int32_t> result =
        buffer.GetPercentile(base::TimeTicks(), INT32_MIN, i,
                             nullptr);
    EXPECT_TRUE(result.has_value());
    EXPECT_NEAR(result.value(), i, 1);
  }

  deleted_observation_sources[NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP] = true;
  buffer.RemoveObservationsWithSource(deleted_observation_sources);
  EXPECT_EQ(0u, buffer.Size());
}

TEST(NetworkQualityObservationBufferTest, TestGetMedianRTTSince) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  ObservationBuffer buffer(&params, &tick_clock, 0.5, 1.0);
  base::TimeTicks now = tick_clock.NowTicks();
  base::TimeTicks old = now - base::TimeDelta::FromMilliseconds(1);
  ASSERT_NE(old, now);

  // First sample has very old timestamp.
  buffer.AddObservation(
      Observation(1, old, INT32_MIN, NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));

  buffer.AddObservation(Observation(100, now, INT32_MIN,
                                    NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP));

  const struct {
    base::TimeTicks start_timestamp;
    bool expect_network_quality_available;
    base::TimeDelta expected_url_request_rtt;
  } tests[] = {
      {now + base::TimeDelta::FromSeconds(10), false,
       base::TimeDelta::FromMilliseconds(0)},
      {now, true, base::TimeDelta::FromMilliseconds(100)},
      {now - base::TimeDelta::FromMicroseconds(500), true,
       base::TimeDelta::FromMilliseconds(100)},

  };

  for (const auto& test : tests) {
    base::Optional<int32_t> url_request_rtt =
        buffer.GetPercentile(test.start_timestamp, INT32_MIN, 50, nullptr);
    EXPECT_EQ(test.expect_network_quality_available,
              url_request_rtt.has_value());

    if (test.expect_network_quality_available) {
      EXPECT_EQ(test.expected_url_request_rtt.InMillisecondsF(),
                url_request_rtt.value());
    }
  }
}

// Test that time filtering works and the remote hosts are split correctly.
TEST(NetworkQualityObservationBufferTest,
     RestGetPercentileForEachRemoteHostSinceTimeStamp) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  const uint64_t new_host = 0x101010UL;
  const int32_t new_host_observation = 1000;
  const size_t new_host_num_obs = 10;
  const uint64_t old_host = 0x202020UL;
  const int32_t old_host_observation = 2000;
  const size_t old_host_num_obs = 20;
  ObservationBuffer buffer(&params, &tick_clock, 0.5, 1.0);
  base::TimeTicks now = tick_clock.NowTicks();
  for (unsigned int i = 0; i < old_host_num_obs; ++i) {
    buffer.AddObservation(Observation(
        old_host_observation, now - base::TimeDelta::FromSeconds(100),
        INT32_MIN, NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP, old_host));
  }

  for (unsigned int i = 0; i < new_host_num_obs; ++i) {
    buffer.AddObservation(Observation(new_host_observation, now, INT32_MIN,
                                      NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP,
                                      new_host));
  }

  std::map<uint64_t, int32_t> host_keyed_percentiles;
  std::map<uint64_t, size_t> host_keyed_counts;
  buffer.GetPercentileForEachHostWithCounts(
      now - base::TimeDelta::FromSeconds(50), 50, base::nullopt,
      &host_keyed_percentiles, &host_keyed_counts);
  EXPECT_EQ(1u, host_keyed_percentiles.size());
  EXPECT_EQ(1u, host_keyed_counts.size());
  EXPECT_EQ(new_host_observation, host_keyed_percentiles[new_host]);
  EXPECT_EQ(new_host_num_obs, host_keyed_counts[new_host]);

  host_keyed_percentiles.clear();
  host_keyed_counts.clear();

  buffer.GetPercentileForEachHostWithCounts(
      now - base::TimeDelta::FromSeconds(150), 50, base::nullopt,
      &host_keyed_percentiles, &host_keyed_counts);
  EXPECT_EQ(2u, host_keyed_percentiles.size());
  EXPECT_EQ(2u, host_keyed_counts.size());
  EXPECT_EQ(new_host_observation, host_keyed_percentiles[new_host]);
  EXPECT_EQ(new_host_num_obs, host_keyed_counts[new_host]);
  EXPECT_EQ(old_host_observation, host_keyed_percentiles[old_host]);
  EXPECT_EQ(old_host_num_obs, host_keyed_counts[old_host]);
}

// Test that the result is split correctly for multiple remote hosts and that
// the count for each host is correct.
TEST(NetworkQualityObservationBufferTest,
     RestGetPercentileForEachRemoteHostCounts) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  ObservationBuffer buffer(&params, &tick_clock, 0.5, 1.0);
  base::TimeTicks now = tick_clock.NowTicks();
  const size_t num_remote_hosts = 5;

  // Add |2*i| observations having value |4*i| for host |i|.
  for (unsigned int host_index = 1; host_index <= num_remote_hosts;
       ++host_index) {
    for (unsigned int count = 1; count <= 2 * host_index; ++count) {
      buffer.AddObservation(Observation(4 * host_index, now, INT32_MIN,
                                        NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP,
                                        static_cast<uint64_t>(host_index)));
    }
  }

  std::map<uint64_t, int32_t> host_keyed_percentiles;
  std::map<uint64_t, size_t> host_keyed_counts;
  buffer.GetPercentileForEachHostWithCounts(
      base::TimeTicks(), 50, base::nullopt, &host_keyed_percentiles,
      &host_keyed_counts);
  EXPECT_EQ(num_remote_hosts, host_keyed_percentiles.size());
  EXPECT_EQ(num_remote_hosts, host_keyed_counts.size());

  for (unsigned int host_index = 1; host_index <= num_remote_hosts;
       ++host_index) {
    EXPECT_EQ(2u * host_index,
              host_keyed_counts[static_cast<uint64_t>(host_index)]);
    EXPECT_EQ(static_cast<int32_t>(4 * host_index),
              host_keyed_percentiles[static_cast<uint64_t>(host_index)]);
  }
}

// Test that the percentiles are computed correctly for different remote hosts.
TEST(NetworkQualityObservationBufferTest,
     RestGetPercentileForEachRemoteHostComputation) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromMinutes(1));
  ObservationBuffer buffer(&params, &tick_clock, 0.5, 1.0);
  base::TimeTicks now = tick_clock.NowTicks();
  const size_t num_hosts = 3;

  // For three different remote hosts, add observations such that the 50
  // percentiles are different.
  for (unsigned int host_index = 1; host_index <= num_hosts; host_index++) {
    // Add |20 * host_index + 1| observations for host |host_index|.
    for (unsigned int observation_value = 90 * host_index;
         observation_value <= 110 * host_index; observation_value++) {
      buffer.AddObservation(Observation(observation_value, now, INT32_MIN,
                                        NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP,
                                        static_cast<uint64_t>(host_index)));
    }
  }
  std::map<uint64_t, int32_t> host_keyed_percentiles;
  std::map<uint64_t, size_t> host_keyed_counts;

  // Test the computation of the median.
  buffer.GetPercentileForEachHostWithCounts(
      base::TimeTicks(), 50, base::nullopt, &host_keyed_percentiles,
      &host_keyed_counts);

  EXPECT_EQ(num_hosts, host_keyed_percentiles.size());
  EXPECT_EQ(num_hosts, host_keyed_counts.size());

  // The median must be equal to |100 * i| and the count must be equal to
  // |20 * i + 1| for host |i|.
  for (unsigned int host_index = 1; host_index <= num_hosts; host_index++) {
    EXPECT_EQ(100u * host_index,
              static_cast<uint32_t>(
                  host_keyed_percentiles[static_cast<uint64_t>(host_index)]));
    EXPECT_EQ(static_cast<size_t>(20 * host_index + 1),
              host_keyed_counts[static_cast<uint64_t>(host_index)]);
  }

  // Test the computation of 0th percentile.
  buffer.GetPercentileForEachHostWithCounts(base::TimeTicks(), 0, base::nullopt,
                                            &host_keyed_percentiles,
                                            &host_keyed_counts);

  EXPECT_EQ(num_hosts, host_keyed_percentiles.size());
  EXPECT_EQ(num_hosts, host_keyed_counts.size());

  // The 0 percentile must be equal to |90 * i| and the count must be equal to
  // |20 * i| for host |i|.
  for (unsigned int host_index = 1; host_index <= num_hosts; host_index++) {
    EXPECT_EQ(90u * host_index,
              static_cast<uint32_t>(
                  host_keyed_percentiles[static_cast<uint64_t>(host_index)]));
    EXPECT_EQ(static_cast<size_t>(20 * host_index + 1),
              host_keyed_counts[static_cast<uint64_t>(host_index)]);
  }

  // Test the computation of 100th percentile.
  buffer.GetPercentileForEachHostWithCounts(
      base::TimeTicks(), 100, base::nullopt, &host_keyed_percentiles,
      &host_keyed_counts);

  EXPECT_EQ(num_hosts, host_keyed_percentiles.size());
  EXPECT_EQ(num_hosts, host_keyed_counts.size());

  // The 0 percentile must be equal to |90 * i| and the count must be equal to
  // |20 * i| for host |i|.
  for (int host_index = 1; host_index <= 3; host_index++) {
    EXPECT_EQ(110 * host_index,
              host_keyed_percentiles[static_cast<uint64_t>(host_index)]);
    EXPECT_EQ(static_cast<size_t>(20 * host_index + 1),
              host_keyed_counts[static_cast<uint64_t>(host_index)]);
  }
}

}  // namespace

}  // namespace internal

}  // namespace nqe

}  // namespace net
