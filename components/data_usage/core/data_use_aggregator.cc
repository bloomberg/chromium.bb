// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_usage/core/data_use_aggregator.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "components/data_usage/core/data_use.h"
#include "components/data_usage/core/data_use_amortizer.h"
#include "components/data_usage/core/data_use_annotator.h"
#include "net/base/load_timing_info.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif  // OS_ANDROID

namespace data_usage {

DataUseAggregator::DataUseAggregator(scoped_ptr<DataUseAnnotator> annotator,
                                     scoped_ptr<DataUseAmortizer> amortizer)
    : annotator_(annotator.Pass()),
      amortizer_(amortizer.Pass()),
      connection_type_(net::NetworkChangeNotifier::GetConnectionType()),
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

void DataUseAggregator::ReportDataUse(net::URLRequest* request,
                                      int64_t tx_bytes,
                                      int64_t rx_bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());

  net::LoadTimingInfo load_timing_info;
  request->GetLoadTimingInfo(&load_timing_info);

  scoped_ptr<DataUse> data_use(
      new DataUse(request->url(), load_timing_info.request_start,
                  request->first_party_for_cookies(), -1 /* tab_id */,
                  connection_type_, mcc_mnc_, tx_bytes, rx_bytes));

  if (!annotator_) {
    PassDataUseToAmortizer(data_use.Pass());
    return;
  }

  // TODO(sclittle): Instead of binding a new callback every time, re-use the
  // same callback every time.
  annotator_->Annotate(
      request, data_use.Pass(),
      base::Bind(&DataUseAggregator::PassDataUseToAmortizer, GetWeakPtr()));
}

void DataUseAggregator::ReportOffTheRecordDataUse(int64_t tx_bytes,
                                                  int64_t rx_bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!amortizer_)
    return;

  amortizer_->OnExtraBytes(tx_bytes, rx_bytes);
}

base::WeakPtr<DataUseAggregator> DataUseAggregator::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
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

void DataUseAggregator::PassDataUseToAmortizer(scoped_ptr<DataUse> data_use) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_use);

  if (!amortizer_) {
    OnAmortizationComplete(data_use.Pass());
    return;
  }

  // TODO(sclittle): Instead of binding a new callback every time, re-use the
  // same callback every time.
  amortizer_->AmortizeDataUse(
      data_use.Pass(),
      base::Bind(&DataUseAggregator::OnAmortizationComplete, GetWeakPtr()));
}

void DataUseAggregator::OnAmortizationComplete(
    scoped_ptr<DataUse> amortized_data_use) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Pass Observers a sequence of const DataUse pointers instead of using the
  // buffer directly in order to prevent Observers from modifying the DataUse
  // objects.
  // TODO(sclittle): Change the observer interface to take in a const DataUse&.
  std::vector<const DataUse*> const_sequence(1, amortized_data_use.get());
  DCHECK(!ContainsValue(const_sequence, nullptr));
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDataUse(const_sequence));
}

}  // namespace data_usage
