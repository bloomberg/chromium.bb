// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USAGE_CORE_DATA_USE_AGGREGATOR_H_
#define COMPONENTS_DATA_USAGE_CORE_DATA_USE_AGGREGATOR_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
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

  base::WeakPtr<DataUseAggregator> GetWeakPtr();

 private:
  // Flush any buffered data use and notify observers.
  void FlushBufferedDataUse();

  base::ThreadChecker thread_checker_;
  base::ObserverList<Observer> observer_list_;

  // Buffer of unreported data use.
  std::vector<DataUse> buffered_data_use_;

  // The total amount of off-the-record data usage that has happened since the
  // last time the buffer was flushed.
  int64_t off_the_record_tx_bytes_since_last_flush_;
  int64_t off_the_record_rx_bytes_since_last_flush_;

  // Indicates if a FlushBufferedDataUse() callback has been posted to run later
  // on the IO thread.
  bool is_flush_pending_;

  base::WeakPtrFactory<DataUseAggregator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataUseAggregator);
};

}  // namespace data_usage

#endif  // COMPONENTS_DATA_USAGE_CORE_DATA_USE_AGGREGATOR_H_
