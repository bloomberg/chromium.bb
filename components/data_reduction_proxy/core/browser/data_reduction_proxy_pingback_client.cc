// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_pingback_client.h"

#include "base/metrics/histogram.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_page_load_timing.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

static const char kHistogramSucceeded[] =
    "DataReductionProxy.Pingback.Succeeded";
static const char kHistogramAttempted[] =
    "DataReductionProxy.Pingback.Attempted";

// Adds the relevant information to |request| for this page load based on page
// timing and data reduction proxy state.
void AddDataToPageloadMetrics(const DataReductionProxyData& request_data,
                              const DataReductionProxyPageLoadTiming& timing,
                              PageloadMetrics* request) {
  request->set_session_key(request_data.session_key());
  // For the timing events, any of them could be zero. Fill the message as a
  // best effort.
  request->set_allocated_first_request_time(
      protobuf_parser::CreateTimestampFromTime(timing.navigation_start)
          .release());
  if (request_data.original_request_url().is_valid())
    request->set_first_request_url(request_data.original_request_url().spec());
  if (timing.first_contentful_paint) {
    request->set_allocated_time_to_first_contentful_paint(
        protobuf_parser::CreateDurationFromTimeDelta(
            timing.first_contentful_paint.value())
            .release());
  }
  if (timing.first_image_paint) {
    request->set_allocated_time_to_first_image_paint(
        protobuf_parser::CreateDurationFromTimeDelta(
            timing.first_image_paint.value())
            .release());
  }
  if (timing.response_start) {
    request->set_allocated_time_to_first_byte(
        protobuf_parser::CreateDurationFromTimeDelta(
            timing.response_start.value())
            .release());
  }
  if (timing.load_event_start) {
    request->set_allocated_page_load_time(
        protobuf_parser::CreateDurationFromTimeDelta(
            timing.load_event_start.value())
            .release());
  }
}

// Adds |current_time| as the metrics sent time to |request_data|, and returns
// the serialized request.
std::string AddTimeAndSerializeRequest(
    RecordPageloadMetricsRequest* request_data,
    base::Time current_time) {
  request_data->set_allocated_metrics_sent_time(
      protobuf_parser::CreateTimestampFromTime(current_time).release());
  std::string serialized_request;
  request_data->SerializeToString(&serialized_request);
  return serialized_request;
}

}  // namespace

DataReductionProxyPingbackClient::DataReductionProxyPingbackClient(
    net::URLRequestContextGetter* url_request_context)
    : url_request_context_(url_request_context),
      pingback_url_(util::AddApiKeyToUrl(params::GetPingbackURL())),
      pingback_reporting_fraction_(0.0) {}

DataReductionProxyPingbackClient::~DataReductionProxyPingbackClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DataReductionProxyPingbackClient::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(source == current_fetcher_.get());
  UMA_HISTOGRAM_BOOLEAN(kHistogramSucceeded, source->GetStatus().is_success());
  current_fetcher_.reset();
  if (metrics_request_.pageloads_size() > 0) {
    CreateFetcherForDataAndStart();
  }
}

void DataReductionProxyPingbackClient::SendPingback(
    const DataReductionProxyData& request_data,
    const DataReductionProxyPageLoadTiming& timing) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool send_pingback = ShouldSendPingback();
  UMA_HISTOGRAM_BOOLEAN(kHistogramAttempted, send_pingback);
  if (!send_pingback)
    return;
  PageloadMetrics* pageload_metrics = metrics_request_.add_pageloads();
  AddDataToPageloadMetrics(request_data, timing, pageload_metrics);
  if (current_fetcher_.get())
    return;
  DCHECK_EQ(1, metrics_request_.pageloads_size());
  CreateFetcherForDataAndStart();
}

void DataReductionProxyPingbackClient::CreateFetcherForDataAndStart() {
  DCHECK(!current_fetcher_);
  DCHECK_GE(metrics_request_.pageloads_size(), 1);
  std::string serialized_request =
      AddTimeAndSerializeRequest(&metrics_request_, CurrentTime());
  metrics_request_.Clear();
  current_fetcher_ =
      net::URLFetcher::Create(pingback_url_, net::URLFetcher::POST, this);
  current_fetcher_->SetLoadFlags(net::LOAD_BYPASS_PROXY);
  current_fetcher_->SetUploadData("application/x-protobuf", serialized_request);
  current_fetcher_->SetRequestContext(url_request_context_);
  // Configure max retries to be at most kMaxRetries times for 5xx errors.
  static const int kMaxRetries = 5;
  current_fetcher_->SetMaxRetriesOn5xx(kMaxRetries);
  current_fetcher_->SetAutomaticallyRetryOnNetworkChanges(kMaxRetries);
  current_fetcher_->Start();
}

bool DataReductionProxyPingbackClient::ShouldSendPingback() const {
  return params::IsForcePingbackEnabledViaFlags() ||
         GenerateRandomFloat() < pingback_reporting_fraction_;
}

base::Time DataReductionProxyPingbackClient::CurrentTime() const {
  return base::Time::Now();
}

float DataReductionProxyPingbackClient::GenerateRandomFloat() const {
  return static_cast<float>(base::RandDouble());
}

void DataReductionProxyPingbackClient::SetPingbackReportingFraction(
    float pingback_reporting_fraction) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(0.0f, pingback_reporting_fraction);
  DCHECK_GE(1.0f, pingback_reporting_fraction);
  pingback_reporting_fraction_ = pingback_reporting_fraction;
}

}  // namespace data_reduction_proxy
