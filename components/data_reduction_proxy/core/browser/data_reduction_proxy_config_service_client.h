// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"

namespace base {
class Time;
class TimeDelta;
}

namespace data_reduction_proxy {

class DataReductionProxyConfig;
class DataReductionProxyMutableConfigValues;
class DataReductionProxyParams;
class DataReductionProxyRequestOptions;

// Retrieves the default net::BackoffEntry::Policy for the Data Reduction Proxy
// configuration service client.
const net::BackoffEntry::Policy& GetBackoffPolicy();

// Retrieves the Data Reduction Proxy configuration from a remote service. This
// object lives on the IO thread.
class DataReductionProxyConfigServiceClient {
 public:
  // The caller must ensure that all parameters remain alive for the lifetime of
  // the |DataReductionProxyConfigClient|, with the exception of |params|
  // which this instance will own.
  DataReductionProxyConfigServiceClient(
      scoped_ptr<DataReductionProxyParams> params,
      const net::BackoffEntry::Policy& backoff_policy,
      DataReductionProxyRequestOptions* request_options,
      DataReductionProxyMutableConfigValues* config_values,
      DataReductionProxyConfig* config);

  virtual ~DataReductionProxyConfigServiceClient();

  // Request the retrieval of the Data Reduction Proxy configuration.
  void RetrieveConfig();

 protected:
  // Retrieves the backoff entry object being used to throttle request failures.
  // Virtual for testing.
  virtual net::BackoffEntry* GetBackoffEntry();

  // Sets a timer to determine when to next refresh the Data Reduction Proxy
  // configuration.
  // Virtual for testing.
  virtual void SetConfigRefreshTimer(const base::TimeDelta& delay);

  // Returns the current time.
  // Virtual for testing.
  virtual base::Time Now();

  // Constructs a synthetic response based on |params_|.
  // Virtual for testing.
  virtual std::string ConstructStaticResponse() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigServiceClientTest,
                           TestConstructStaticResponse);
  friend class TestDataReductionProxyConfigServiceClient;

  // Contains the static configuration data to use.
  scoped_ptr<DataReductionProxyParams> params_;

  // The caller must ensure that the |request_options_| outlives this instance.
  DataReductionProxyRequestOptions* request_options_;

  // The caller must ensure that the |config_values_| outlives this instance.
  DataReductionProxyMutableConfigValues* config_values_;

  // The caller must ensure that the |config_| outlives this instance.
  DataReductionProxyConfig* config_;

  // Used to calculate the backoff time on request failures.
  net::BackoffEntry backoff_entry_;

  // An event that fires when it is time to refresh the Data Reduction Proxy
  // configuration.
  base::OneShotTimer<DataReductionProxyConfigServiceClient>
      config_refresh_timer_;

  // Enforce usage on the IO thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfigServiceClient);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_H_
