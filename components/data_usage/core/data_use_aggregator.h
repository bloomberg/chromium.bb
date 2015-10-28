// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USAGE_CORE_DATA_USE_AGGREGATOR_H_
#define COMPONENTS_DATA_USAGE_CORE_DATA_USE_AGGREGATOR_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "net/base/network_change_notifier.h"

namespace net {
class URLRequest;
}

namespace data_usage {
struct DataUse;

// Class that collects and aggregates network usage, reporting the usage to
// observers. Should only be used on the IO thread.
class DataUseAggregator
    : public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  class Observer {
   public:
    virtual ~Observer() {}
    // Each of the elements of |data_use_sequence| are guaranteed to be
    // non-NULL.
    virtual void OnDataUse(
        const std::vector<const DataUse*>& data_use_sequence) = 0;
  };

  DataUseAggregator();
  ~DataUseAggregator() override;

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

 protected:
  // net::NetworkChangeNotifier::ConnectionTypeObserver implementation.
  // Protected for testing.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Protected for testing.
  void SetMccMncForTests(const std::string& mcc_mnc);

 private:
  // Flush any buffered data use and notify observers.
  void FlushBufferedDataUse();

  base::ThreadChecker thread_checker_;
  base::ObserverList<Observer> observer_list_;

  // Buffer of unreported data use.
  ScopedVector<DataUse> buffered_data_use_;

  // Current connection type as notified by NetworkChangeNotifier.
  net::NetworkChangeNotifier::ConnectionType connection_type_;

  // MCC+MNC (mobile country code + mobile network code) of the current SIM
  // provider.  Set to empty string if SIM is not present. |mcc_mnc_| is set
  // even if the current active network is not a cellular network.
  std::string mcc_mnc_;

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
