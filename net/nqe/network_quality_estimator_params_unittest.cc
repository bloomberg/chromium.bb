// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_quality_estimator_params.h"

#include <map>
#include <string>

#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace nqe {

namespace internal {

namespace {

// Tests if |weight_multiplier_per_second()| returns correct value for various
// values of half life parameter.
TEST(NetworkQualityEstimatorParamsTest, HalfLifeParam) {
  std::map<std::string, std::string> variation_params;

  const struct {
    std::string description;
    std::string variation_params_value;
    double expected_weight_multiplier;
  } tests[] = {
      {"Half life parameter is not set, default value should be used",
       std::string(), 0.988},
      {"Half life parameter is set to negative, default value should be used",
       "-100", 0.988},
      {"Half life parameter is set to zero, default value should be used", "0",
       0.988},
      {"Half life parameter is set correctly", "10", 0.933},
  };

  for (const auto& test : tests) {
    variation_params["HalfLifeSeconds"] = test.variation_params_value;
    NetworkQualityEstimatorParams params(variation_params);
    EXPECT_NEAR(test.expected_weight_multiplier,
                params.weight_multiplier_per_second(), 0.001)
        << test.description;
  }
}

// Test that the typical network qualities are set correctly.
TEST(NetworkQualityEstimatorParamsTest, TypicalNetworkQualities) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);

  // Typical network quality should not be set for Unknown and Offline.
  for (size_t i = EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
       i <= EFFECTIVE_CONNECTION_TYPE_OFFLINE; ++i) {
    EffectiveConnectionType ect = static_cast<EffectiveConnectionType>(i);
    EXPECT_EQ(nqe::internal::InvalidRTT(),
              params.TypicalNetworkQuality(ect).http_rtt());

    EXPECT_EQ(nqe::internal::InvalidRTT(),
              params.TypicalNetworkQuality(ect).transport_rtt());
  }

  // Typical network quality should be set for other effective connection
  // types.
  for (size_t i = EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
       i <= EFFECTIVE_CONNECTION_TYPE_3G; ++i) {
    EffectiveConnectionType ect = static_cast<EffectiveConnectionType>(i);
    // The typical RTT for an effective connection type should be at least as
    // much as the threshold RTT.
    EXPECT_NE(nqe::internal::InvalidRTT(),
              params.TypicalNetworkQuality(ect).http_rtt());
    EXPECT_GT(params.TypicalNetworkQuality(ect).http_rtt(),
              params.ConnectionThreshold(ect).http_rtt());

    EXPECT_NE(nqe::internal::InvalidRTT(),
              params.TypicalNetworkQuality(ect).transport_rtt());
    EXPECT_GT(params.TypicalNetworkQuality(ect).transport_rtt(),
              params.ConnectionThreshold(ect).transport_rtt());

    // The typical throughput for an effective connection type should not be
    // more than the threshold throughput.
    if (params.ConnectionThreshold(ect).downstream_throughput_kbps() !=
        nqe::internal::INVALID_RTT_THROUGHPUT) {
      EXPECT_LT(params.TypicalNetworkQuality(ect).downstream_throughput_kbps(),
                params.ConnectionThreshold(ect).downstream_throughput_kbps());
    }
  }

  // The typical network quality of 4G connection should be at least as fast
  // as the threshold for 3G connection.
  EXPECT_LT(
      params.TypicalNetworkQuality(EFFECTIVE_CONNECTION_TYPE_4G).http_rtt(),
      params.ConnectionThreshold(EFFECTIVE_CONNECTION_TYPE_3G).http_rtt());
  EXPECT_LT(
      params.TypicalNetworkQuality(EFFECTIVE_CONNECTION_TYPE_4G)
          .transport_rtt(),
      params.ConnectionThreshold(EFFECTIVE_CONNECTION_TYPE_3G).transport_rtt());
  if (params.ConnectionThreshold(EFFECTIVE_CONNECTION_TYPE_3G)
          .downstream_throughput_kbps() !=
      nqe::internal::INVALID_RTT_THROUGHPUT) {
    EXPECT_GT(params.TypicalNetworkQuality(EFFECTIVE_CONNECTION_TYPE_4G)
                  .downstream_throughput_kbps(),
              params.ConnectionThreshold(EFFECTIVE_CONNECTION_TYPE_3G)
                  .downstream_throughput_kbps());
  }
}

TEST(NetworkQualityEstimatorParamsTest, ObtainAlgorithmToUseFromParams) {
  const struct {
    bool set_variation_param;
    std::string algorithm;
    NetworkQualityEstimatorParams::EffectiveConnectionTypeAlgorithm
        expected_algorithm;
  } tests[] = {
      {false, "",
       NetworkQualityEstimatorParams::EffectiveConnectionTypeAlgorithm::
           HTTP_RTT_AND_DOWNSTREAM_THROUGHOUT},
      {true, "",
       NetworkQualityEstimatorParams::EffectiveConnectionTypeAlgorithm::
           HTTP_RTT_AND_DOWNSTREAM_THROUGHOUT},
      {true, "HttpRTTAndDownstreamThroughput",
       NetworkQualityEstimatorParams::EffectiveConnectionTypeAlgorithm::
           HTTP_RTT_AND_DOWNSTREAM_THROUGHOUT},
      {true, "TransportRTTOrDownstreamThroughput",
       NetworkQualityEstimatorParams::EffectiveConnectionTypeAlgorithm::
           TRANSPORT_RTT_OR_DOWNSTREAM_THROUGHOUT},
  };

  for (const auto& test : tests) {
    std::map<std::string, std::string> variation_params;
    if (test.set_variation_param)
      variation_params["effective_connection_type_algorithm"] = test.algorithm;

    NetworkQualityEstimatorParams params(variation_params);
    EXPECT_EQ(test.expected_algorithm,
              params.GetEffectiveConnectionTypeAlgorithm())
        << test.algorithm;
  }
}

// Verify that for a given connection type, the default network quality values
// lie in the same range of ECT as the value returned by GetDefaultECT().
TEST(NetworkQualityEstimatorParamsTest, GetDefaultECT) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimatorParams params(variation_params);

  for (size_t i = 0; i < NetworkChangeNotifier::ConnectionType::CONNECTION_LAST;
       ++i) {
    NetworkChangeNotifier::ConnectionType connection_type =
        static_cast<NetworkChangeNotifier::ConnectionType>(i);
    EffectiveConnectionType ect = params.GetDefaultECT(connection_type);
    EXPECT_LE(EFFECTIVE_CONNECTION_TYPE_2G, ect);

    const nqe::internal::NetworkQuality& default_nq =
        params.DefaultObservation(connection_type);

    // Now verify that |default_nq| corresponds to |ect|.
    if (ect == EFFECTIVE_CONNECTION_TYPE_4G) {
      // If the expected effective connection type is 4G, then RTT values in
      // |default_nq| should be lower than the threshold for ECT of 3G.
      const nqe::internal::NetworkQuality& threshold_3g =
          params.ConnectionThreshold(EFFECTIVE_CONNECTION_TYPE_3G);

      EXPECT_LT(default_nq.http_rtt(), threshold_3g.http_rtt());
      EXPECT_LT(default_nq.transport_rtt(), threshold_3g.transport_rtt());
      EXPECT_TRUE(default_nq.downstream_throughput_kbps() >
                      threshold_3g.downstream_throughput_kbps() ||
                  threshold_3g.downstream_throughput_kbps() < 0);
    } else {
      // If the expected effective connection type is |ect|, then RTT values in
      // |default_nq| should be (i) higher than the threshold for ECT of |ect|
      // (ii) Lower than the threshold for |slower_ect|.
      const nqe::internal::NetworkQuality& threshold_ect =
          params.ConnectionThreshold(ect);
      EffectiveConnectionType slower_ect =
          static_cast<EffectiveConnectionType>(static_cast<int>(ect) - 1);
      const nqe::internal::NetworkQuality& threshold_slower_ect =
          params.ConnectionThreshold(slower_ect);

      EXPECT_GT(default_nq.http_rtt(), threshold_ect.http_rtt());
      EXPECT_GT(default_nq.transport_rtt(), threshold_ect.transport_rtt());
      EXPECT_TRUE(default_nq.downstream_throughput_kbps() <
                      threshold_ect.downstream_throughput_kbps() ||
                  threshold_ect.downstream_throughput_kbps() < 0);

      EXPECT_LT(default_nq.http_rtt(), threshold_slower_ect.http_rtt());
      EXPECT_LT(default_nq.transport_rtt(),
                threshold_slower_ect.transport_rtt());
      EXPECT_TRUE(default_nq.downstream_throughput_kbps() >
                      threshold_slower_ect.downstream_throughput_kbps() ||
                  threshold_slower_ect.downstream_throughput_kbps() < 0);
    }
  }
}

}  // namespace

}  // namespace internal

}  // namespace nqe

}  // namespace net
