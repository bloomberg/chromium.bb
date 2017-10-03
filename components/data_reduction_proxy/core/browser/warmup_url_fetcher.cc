// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/warmup_url_fetcher.h"

#include "base/metrics/histogram_macros.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace data_reduction_proxy {

WarmupURLFetcher::WarmupURLFetcher(
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter)
    : url_request_context_getter_(url_request_context_getter) {
  DCHECK(url_request_context_getter_);
}

WarmupURLFetcher::~WarmupURLFetcher() {}

void WarmupURLFetcher::FetchWarmupURL() {
  UMA_HISTOGRAM_EXACT_LINEAR("DataReductionProxy.WarmupURL.FetchInitiated", 1,
                             2);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("data_reduction_proxy_warmup", R"(
          semantics {
            sender: "Data Reduction Proxy"
            description:
              "Sends a request to the Data Reduction Proxy server to warm up "
              "the connection to the proxy."
            trigger:
              "A network change while the data reduction proxy is enabled will "
              "trigger this request."
            data: "A specific URL, not related to user data."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can control Data Saver on Android via the 'Data Saver' "
              "setting. Data Saver is not available on iOS, and on desktop it "
              "is enabled by installing the Data Saver extension."
            policy_exception_justification: "Not implemented."
          })");
  fetcher_ = net::URLFetcher::Create(
      params::GetWarmupURL(), net::URLFetcher::GET, this, traffic_annotation);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher_.get(),
      data_use_measurement::DataUseUserData::DATA_REDUCTION_PROXY);
  // Do not disable cookies. This allows the warmup connection to be reused
  // for fetching user initiated requests.
  fetcher_->SetLoadFlags(net::LOAD_BYPASS_CACHE);
  fetcher_->SetRequestContext(url_request_context_getter_.get());
  // |fetcher| should not retry on 5xx errors. |fetcher_| should retry on
  // network changes since the network stack may receive the connection change
  // event later than |this|.
  static const int kMaxRetries = 5;
  fetcher_->SetAutomaticallyRetryOn5xx(false);
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(kMaxRetries);
  fetcher_->Start();
}

void WarmupURLFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(source, fetcher_.get());
  UMA_HISTOGRAM_BOOLEAN(
      "DataReductionProxy.WarmupURL.FetchSuccessful",
      source->GetStatus().status() == net::URLRequestStatus::SUCCESS);
}

}  // namespace data_reduction_proxy