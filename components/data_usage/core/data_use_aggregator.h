// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USAGE_CORE_DATA_USE_AGGREGATOR_H_
#define COMPONENTS_DATA_USAGE_CORE_DATA_USE_AGGREGATOR_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/data_usage/core/data_use.h"

namespace net {
class URLRequest;
}

namespace data_usage {

// Class that collects and aggregates network usage, reporting the usage to
// observers. Should only be used on the IO thread.
class DataUseAggregator {
 public:
  class Observer {
   public:
    virtual ~Observer() {}
    // TODO(sclittle): Consider performing some pre-aggregation on the data use.
    virtual void OnDataUse(const std::vector<DataUse>& data_use_sequence) = 0;
  };

  DataUseAggregator();
  virtual ~DataUseAggregator();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Virtual for testing.
  virtual void ReportDataUse(const net::URLRequest& request,
                             int32_t tab_id,
                             int64_t tx_bytes,
                             int64_t rx_bytes);

  // Account for off-the-record data use. This usage is only kept track of here
  // so that it can be taken out of any amortized data usage calculations, and a
  // per-request breakdown of off-the-record data usage will never leave the
  // DataUseAggregator.
  // Virtual for testing.
  virtual void ReportOffTheRecordDataUse(int64_t tx_bytes, int64_t rx_bytes);

 private:
  void NotifyDataUse(const std::vector<DataUse>& data_use_sequence);

  base::ThreadChecker thread_checker_;
  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(DataUseAggregator);
};

}  // namespace data_usage

#endif  // COMPONENTS_DATA_USAGE_CORE_DATA_USE_AGGREGATOR_H_
