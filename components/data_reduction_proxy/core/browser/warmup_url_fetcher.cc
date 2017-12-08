// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/warmup_url_fetcher.h"

#include "base/guid.h"
#include "base/metrics/histogram_macros.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_util.h"
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

  GURL warmup_url_with_query_params;
  GetWarmupURLWithQueryParam(&warmup_url_with_query_params);

  fetcher_ =
      net::URLFetcher::Create(warmup_url_with_query_params,
                              net::URLFetcher::GET, this, traffic_annotation);
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

void WarmupURLFetcher::GetWarmupURLWithQueryParam(
    GURL* warmup_url_with_query_params) const {
  // Set the query param to a random string to prevent intermediate middleboxes
  // from returning cached content.
  const std::string query = "q=" + base::GenerateGUID();
  GURL::Replacements replacements;
  replacements.SetQuery(query.c_str(), url::Component(0, query.length()));

  *warmup_url_with_query_params =
      params::GetWarmupURL().ReplaceComponents(replacements);

  DCHECK(warmup_url_with_query_params->is_valid() &&
         warmup_url_with_query_params->has_query());
}

void WarmupURLFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(source, fetcher_.get());
  UMA_HISTOGRAM_BOOLEAN(
      "DataReductionProxy.WarmupURL.FetchSuccessful",
      source->GetStatus().status() == net::URLRequestStatus::SUCCESS);

  UMA_HISTOGRAM_SPARSE_SLOWLY("DataReductionProxy.WarmupURL.NetError",
                              std::abs(source->GetStatus().error()));

  UMA_HISTOGRAM_SPARSE_SLOWLY("DataReductionProxy.WarmupURL.HttpResponseCode",
                              std::abs(source->GetResponseCode()));

  if (source->GetResponseHeaders()) {
    UMA_HISTOGRAM_BOOLEAN(
        "DataReductionProxy.WarmupURL.HasViaHeader",
        HasDataReductionProxyViaHeader(*source->GetResponseHeaders(),
                                       nullptr /* has_intermediary */));

    UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.WarmupURL.ProxySchemeUsed",
                              util::ConvertNetProxySchemeToProxyScheme(
                                  source->ProxyServerUsed().scheme()),
                              PROXY_SCHEME_MAX);
  }
}

}  // namespace data_reduction_proxy