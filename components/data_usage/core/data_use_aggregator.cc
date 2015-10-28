// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_usage/core/data_use_aggregator.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "components/data_usage/core/data_use.h"
#include "net/base/load_timing_info.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif  // OS_ANDROID

namespace data_usage {

DataUseAggregator::DataUseAggregator()
    : connection_type_(net::NetworkChangeNotifier::GetConnectionType()),
      off_the_record_tx_bytes_since_last_flush_(0),
      off_the_record_rx_bytes_since_last_flush_(0),
      is_flush_pending_(false),
      weak_ptr_factory_(this) {
#if defined(OS_ANDROID)
  mcc_mnc_ = net::android::GetTelephonySimOperator();
#endif  // OS_ANDROID
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

DataUseAggregator::~DataUseAggregator() {
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

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
  net::LoadTimingInfo load_timing_info;
  request.GetLoadTimingInfo(&load_timing_info);

  scoped_ptr<DataUse> data_use(
      new DataUse(request.url(), load_timing_info.request_start,
                  request.first_party_for_cookies(), tab_id, connection_type_,
                  mcc_mnc_, tx_bytes, rx_bytes));

  // As an optimization, attempt to combine the newly reported data use with the
  // most recent buffered data use, if the annotations on the data use are the
  // same.
  if (!buffered_data_use_.empty() &&
      buffered_data_use_.back()->CanCombineWith(*data_use)) {
    buffered_data_use_.back()->tx_bytes += tx_bytes;
    buffered_data_use_.back()->rx_bytes += rx_bytes;
  } else {
    buffered_data_use_.push_back(data_use.Pass());
  }

  if (!is_flush_pending_) {
    // Post a flush operation to happen in the future, so that the
    // DataUseAggregator has a chance to batch together some data use before
    // notifying observers.
    base::MessageLoop::current()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&DataUseAggregator::FlushBufferedDataUse, GetWeakPtr()));
    is_flush_pending_ = true;
  }
}

void DataUseAggregator::ReportOffTheRecordDataUse(int64_t tx_bytes,
                                                  int64_t rx_bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  off_the_record_tx_bytes_since_last_flush_ += tx_bytes;
  off_the_record_rx_bytes_since_last_flush_ += rx_bytes;
}

base::WeakPtr<DataUseAggregator> DataUseAggregator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DataUseAggregator::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  connection_type_ = type;
#if defined(OS_ANDROID)
  mcc_mnc_ = net::android::GetTelephonySimOperator();
#endif  // OS_ANDROID
}

void DataUseAggregator::SetMccMncForTests(const std::string& mcc_mnc) {
  DCHECK(thread_checker_.CalledOnValidThread());
  mcc_mnc_ = mcc_mnc;
}

void DataUseAggregator::FlushBufferedDataUse() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(sclittle): Amortize data use on supported platforms before notifying
  // observers.

  // Pass Observers a sequence of const DataUse pointers instead of using the
  // buffer directly in order to prevent Observers from modifying the DataUse
  // objects.
  std::vector<const DataUse*> const_sequence(buffered_data_use_.begin(),
                                             buffered_data_use_.end());
  DCHECK(!ContainsValue(const_sequence, nullptr));
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDataUse(const_sequence));

  buffered_data_use_.clear();
  off_the_record_tx_bytes_since_last_flush_ = 0;
  off_the_record_rx_bytes_since_last_flush_ = 0;
  is_flush_pending_ = false;
}

}  // namespace data_usage
