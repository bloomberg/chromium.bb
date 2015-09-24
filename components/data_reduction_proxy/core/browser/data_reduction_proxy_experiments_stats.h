// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_EXPERIMENTS_STATS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_EXPERIMENTS_STATS_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"

namespace base {
class Time;
}

namespace data_reduction_proxy {

class DataReductionProxyConfigRetrievalParams;

typedef base::Callback<void(const std::string&, int64)> Int64ValueSetter;

// Collects statistics specific to experiments of which a client may be part.
// Any calls into this class should be as lightweight as possible such that if
// no experiments are being performed, there should be minimal code being
// executed.
// Currently supported experiments are:
// - Measuring potentially uncompressed bytes during simulated config retrieval.
class DataReductionProxyExperimentsStats {
 public:
  DataReductionProxyExperimentsStats(const Int64ValueSetter& value_setter);
  virtual ~DataReductionProxyExperimentsStats();

  // Initializes |this| on the UI thread. If called, must be called prior to
  // InitializeOnIOThread.
  void InitializeOnUIThread(scoped_ptr<DataReductionProxyConfigRetrievalParams>
                                config_retrieval_params);

  // Initializes |this| on the IO thread.
  void InitializeOnIOThread();

  // Collect received bytes for the experiment.
  void RecordBytes(const base::Time& request_time,
                   DataReductionProxyRequestType request_type,
                   int64 received_content_length,
                   int64 original_content_length);

 private:
  // Updates the simulated expiration of the Data Reduction Proxy configuration
  // if |config_retrieval_| is set.
  void UpdateSimulatedConfig();

  // Allows an experiment to persist data to preferences.
  Int64ValueSetter value_setter_;

  // Enables measuring of potentially uncompressed bytes during a simulated Data
  // Reduction Proxy configuration retrieval.
  scoped_ptr<DataReductionProxyConfigRetrievalParams> config_retrieval_params_;

  // If set, periodically updates the simulated expiration of the Data Reduction
  // Proxy configuration.
  base::RepeatingTimer config_refresh_time_;

  // Enforce initialization order.
  bool initialized_;

  // Enforce usage on the IO thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyExperimentsStats);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_EXPERIMENTS_STATS_H_
