// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_experiments_stats.h"

#include "base/logging.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_retrieval_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"

namespace data_reduction_proxy {

DataReductionProxyExperimentsStats::DataReductionProxyExperimentsStats(
    const Int64ValueSetter& value_setter)
    : value_setter_(value_setter), initialized_(false) {
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyExperimentsStats::~DataReductionProxyExperimentsStats() {
}

void DataReductionProxyExperimentsStats::InitializeOnUIThread(scoped_ptr<
    DataReductionProxyConfigRetrievalParams> config_retrieval_params) {
  DCHECK(!initialized_);
  config_retrieval_params_ = config_retrieval_params.Pass();

  // This method may be called from the UI thread, but should be checked on the
  // IO thread.
  thread_checker_.DetachFromThread();
}

void DataReductionProxyExperimentsStats::InitializeOnIOThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!initialized_);
  initialized_ = true;
  if (config_retrieval_params_) {
    if (config_retrieval_params_->loaded_expired_config())
      UpdateSimulatedConfig();
    config_refresh_time_.Start(
        FROM_HERE, config_retrieval_params_->refresh_interval(), this,
        &DataReductionProxyExperimentsStats::UpdateSimulatedConfig);
  }
}

void DataReductionProxyExperimentsStats::RecordBytes(
    const base::Time& request_time,
    DataReductionProxyRequestType request_type,
    int64 received_content_length,
    int64 original_content_length) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (config_retrieval_params_) {
    // Only measure requests which flowed through the Data Reduction Proxy.
    if (request_type == VIA_DATA_REDUCTION_PROXY) {
      config_retrieval_params_->RecordStats(
          request_time, received_content_length, original_content_length);
    }
  }
}

void DataReductionProxyExperimentsStats::UpdateSimulatedConfig() {
  DCHECK(config_retrieval_params_);
  value_setter_.Run(prefs::kSimulatedConfigRetrieveTime,
                    base::Time::Now().ToInternalValue());
}

}  // namespace data_reduction_proxy
