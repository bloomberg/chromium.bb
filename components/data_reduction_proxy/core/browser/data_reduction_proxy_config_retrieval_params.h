// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_RETRIEVAL_PARAMS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_RETRIEVAL_PARAMS_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"

class PrefService;

namespace base {
class HistogramBase;
}

namespace data_reduction_proxy {

// The number of config fetch variations supported. Must match the number of
// suffixes in histograms.xml.
extern const int kConfigFetchGroups;

// The name of the trial group for measuring lost bytes during fetching the
// configuration.
extern const char kConfigFetchTrialGroup[];

// The config retrieval round trip time is calculated as:
// (previous base) * (multiplier) + increment.
// Multipler is >= 1. Increment is >= 0.
// The name of the param prefix for determining the base round trip time in
// milliseconds for simulating a configuration fetch.
extern const char kConfigRoundtripMillisecondsBaseParam[];

// The name of the param prefix for determining the base round trip time in
// milliseconds for simulating a configuration fetch.
extern const char kConfigRoundtripMultiplierParam[];

// The name of the param prefix for determining the base round trip time in
// milliseconds for simulating a configuration fetch.
extern const char kConfigRoundtripMillisecondsIncrementParam[];

// The name of the param for determining the expiration interval of the
// configuration in seconds.
extern const char kConfigExpirationSecondsParam[];

// The name of the param for determining if the config should be considered
// stale on startup (regardless of whether it is stale or not).
extern const char kConfigAlwaysStaleParam[];

// The number of seconds by which the expiration interval exceeds the refresh
// interval so that we maintain a valid configuration.
extern const int kConfigFetchBufferSeconds;

// Parameters for setting up an experiment to measure potentially uncompressed
// bytes during the retrieval of the Data Reduction Proxy configuration.
class DataReductionProxyConfigRetrievalParams {
 public:
  enum ConfigState {
    VALID = 0,
    RETRIEVING = 1,
    EXPIRED = 2,
  };

  // A variation of the simulated configuration retrieval time, permitting the
  // experiment to measure multiple values.
  class Variation {
   public:
    Variation(int index, const base::Time& simulated_config_retrieved);

    // Returns the current state of the Data Reduction Proxy configuration.
    // Virtual for testing.
    virtual ConfigState GetState(const base::Time& request_time,
                                 const base::Time& config_expiration) const;

    // Records content length statistics if potentially uncompressed bytes have
    // been detected for this variation.
    void RecordStats(int64 received_content_length,
                     int64 original_content_length) const;

    // Visible for testing.
    const base::Time& simulated_config_retrieved() const {
      return simulated_config_retrieved_;
    }

   private:
    // The time at which the simulated Data Reduction Proxy configuration is
    // considered to have been received.
    base::Time simulated_config_retrieved_;

    // Histograms for recording stats.
    base::HistogramBase* lost_bytes_ocl_;
    base::HistogramBase* lost_bytes_rcl_;
    base::HistogramBase* lost_bytes_diff_;
  };

  static scoped_ptr<DataReductionProxyConfigRetrievalParams> Create(
      PrefService* pref_service);

  virtual ~DataReductionProxyConfigRetrievalParams();

  // Records content length statistics against any variations for which
  // potentially uncompressed bytes have been detected.
  void RecordStats(const base::Time& request_time,
                   int64 received_content_length,
                   int64 original_content_length) const;

  // Simulates retrieving a new configuration.
  void RefreshConfig();

  bool loaded_expired_config() const { return loaded_expired_config_; }

  // Returns the expiration time of the current configuration.
  const base::Time& config_expiration() const { return config_expiration_; }

  // Returns how often the configuration should be retrieved.
  const base::TimeDelta& refresh_interval() const {
    return config_refresh_interval_;
  }

  // Visible for testing.
  const std::vector<Variation>& variations() const { return variations_; }

 private:
  DataReductionProxyConfigRetrievalParams(
      bool loaded_expired_config,
      const std::vector<Variation>& variations,
      const base::Time& config_expiration,
      const base::TimeDelta& config_refresh_interval);

  // Indicates that the Data Reduction Proxy configuration when loaded has
  // expired.
  bool loaded_expired_config_;

  // The time at which the simulated Data Reduction Proxy configuration
  // expires.
  base::Time config_expiration_;

  // The duration for which a Data Reduction Proxy configuration is considered
  // valid.
  base::TimeDelta config_expiration_interval_;

  // The duration after which a simulated retrieval of a new Data Reduction
  // Proxy configuration should be performed.
  base::TimeDelta config_refresh_interval_;

  // The different variations on roundtrip time that can take place.
  std::vector<Variation> variations_;
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_RETRIEVAL_PARAMS_H_
