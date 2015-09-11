// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/content/data_use_measurement.h"

#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/network_change_notifier.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

namespace data_use_measurement {

namespace {

// Records the occurrence of |sample| in |name| histogram. Conventional UMA
// histograms are not used because the |name| is not static.
void RecordUMAHistogramCount(const std::string& name, int64_t sample) {
  base::HistogramBase* histogram_pointer = base::Histogram::FactoryGet(
      name,
      1,        // Minimum sample size in bytes.
      1000000,  // Maximum sample size in bytes. Should cover most of the
                // requests by services.
      50,       // Bucket count.
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->Add(sample);
}

// This function increases the value of |sample| bucket in |name| sparse
// histogram by |value|. Conventional UMA histograms are not used because |name|
// is not static.
void IncreaseSparseHistogramByValue(const std::string& name,
                                    int64_t sample,
                                    int64_t value) {
  base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
      name, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddCount(sample, value);
}

}  // namespace

DataUseMeasurement::DataUseMeasurement()
#if defined(OS_ANDROID)
    : app_state_(base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES),
      app_listener_(new base::android::ApplicationStatusListener(
          base::Bind(&DataUseMeasurement::OnApplicationStateChange,
                     base::Unretained(this))))
#endif
{
}

DataUseMeasurement::~DataUseMeasurement(){};

void DataUseMeasurement::ReportDataUseUMA(
    const net::URLRequest* request) const {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  // Having |info| is the sign of a request for a web content from user. For now
  // we could add a condition to check ProcessType in info is
  // content::PROCESS_TYPE_RENDERER, but it won't be compatible with upcoming
  // PlzNavigate architecture. So just existence of |info| is verified, and the
  // current check should be compatible with upcoming changes in PlzNavigate.
  bool is_user_traffic = info != nullptr;

  // Counts rely on URLRequest::GetTotalReceivedBytes() and
  // URLRequest::GetTotalSentBytes(), which does not include the send path,
  // network layer overhead, TLS overhead, and DNS.
  // TODO(amohammadkhan): Make these measured bytes more in line with number of
  // bytes in lower levels.
  int64_t total_upload_bytes = request->GetTotalSentBytes();
  int64_t total_received_bytes = request->GetTotalReceivedBytes();

  RecordUMAHistogramCount(
      GetHistogramName(is_user_traffic ? "DataUse.TrafficSize.User"
                                       : "DataUse.TrafficSize.System",
                       UPSTREAM),
      total_upload_bytes);
  RecordUMAHistogramCount(
      GetHistogramName(is_user_traffic ? "DataUse.TrafficSize.User"
                                       : "DataUse.TrafficSize.System",
                       DOWNSTREAM),
      total_received_bytes);

  DataUseUserData* attached_service_data = reinterpret_cast<DataUseUserData*>(
      request->GetUserData(DataUseUserData::kUserDataKey));

  if (!is_user_traffic) {
    DataUseUserData::ServiceName service_name =
        attached_service_data ? attached_service_data->service_name()
                              : DataUseUserData::NOT_TAGGED;
    ReportDataUsageServices(service_name, UPSTREAM, total_upload_bytes);
    ReportDataUsageServices(service_name, DOWNSTREAM, total_received_bytes);
  }
}

#if defined(OS_ANDROID)
void DataUseMeasurement::OnApplicationStateChangeForTesting(
    base::android::ApplicationState application_state) {
  app_state_ = application_state;
}
#endif

DataUseMeasurement::AppState DataUseMeasurement::CurrentAppState() const {
#if defined(OS_ANDROID)
  if (app_state_ != base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES)
    return BACKGROUND;
#endif
  // If the OS is not Android, all the requests are considered Foreground.
  return FOREGROUND;
}

std::string DataUseMeasurement::GetHistogramName(const char* prefix,
                                                 TrafficDirection dir) const {
  AppState app_state = CurrentAppState();
  bool is_conn_cellular = net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType());
  return base::StringPrintf(
      "%s.%s.%s.%s", prefix, dir == UPSTREAM ? "Upstream" : "Downstream",
      app_state == BACKGROUND ? "Background" : "Foreground",
      is_conn_cellular ? "Cellular" : "NotCellular");
}

#if defined(OS_ANDROID)
void DataUseMeasurement::OnApplicationStateChange(
    base::android::ApplicationState application_state) {
  app_state_ = application_state;
}
#endif

void DataUseMeasurement::ReportDataUsageServices(
    DataUseUserData::ServiceName service,
    TrafficDirection dir,
    int64_t message_size) const {
  RecordUMAHistogramCount(
      "DataUse.MessageSize." + DataUseUserData::GetServiceNameAsString(service),
      message_size);
  if (message_size > 0) {
    IncreaseSparseHistogramByValue(
        GetHistogramName("DataUse.MessageSize.AllServices", dir), service,
        message_size);
  }
}

}  // namespace data_use_measurement
