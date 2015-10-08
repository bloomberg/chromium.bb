// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_usage/core/data_use_aggregator.h"

#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request.h"

namespace data_usage {

DataUseAggregator::DataUseAggregator() {}

DataUseAggregator::~DataUseAggregator() {}

void DataUseAggregator::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void DataUseAggregator::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void DataUseAggregator::ReportDataUse(const net::URLRequest& request,
                                      int32_t tab_id,
                                      int64_t tx_bytes,
                                      int64_t rx_bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(sclittle): Once actual buffering/aggregation is being done, consider
  // combining consecutive data use entries from the same request.
  DataUse data_use(request.url(), request.request_time(),
                   request.first_party_for_cookies(), tab_id,
                   net::NetworkChangeNotifier::GetConnectionType(), tx_bytes,
                   rx_bytes);

  // TODO(sclittle): Buffer and amortize data use on supported platforms.
  NotifyDataUse(std::vector<DataUse>(1, data_use));
}

void DataUseAggregator::ReportOffTheRecordDataUse(int64_t tx_bytes,
                                                  int64_t rx_bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(sclittle): Once data usage amortization is implemented, keep track of
  // the off-the-record bytes so that they can be taken out of the amortized
  // data usage calculations if applicable.
}

void DataUseAggregator::NotifyDataUse(
    const std::vector<DataUse>& data_use_sequence) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDataUse(data_use_sequence));
}

}  // namespace data_usage
