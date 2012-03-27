// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/http_pipelining_compatibility_client.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "chrome/browser/io_thread.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/disk_cache/histogram_macros.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace chrome_browser_net {

HttpPipeliningCompatibilityClient::HttpPipeliningCompatibilityClient()
    : num_finished_(0),
      num_succeeded_(0) {
}

HttpPipeliningCompatibilityClient::~HttpPipeliningCompatibilityClient() {
}

void HttpPipeliningCompatibilityClient::Start(
    const std::string& base_url,
    std::vector<RequestInfo>& requests,
    bool collect_server_stats,
    const net::CompletionCallback& callback,
    net::URLRequestContext* url_request_context) {
  net::HttpNetworkSession* old_session =
      url_request_context->http_transaction_factory()->GetSession();
  net::HttpNetworkSession::Params params = old_session->params();
  params.force_http_pipelining = true;
  scoped_refptr<net::HttpNetworkSession> session =
      new net::HttpNetworkSession(params);
  http_transaction_factory_.reset(
      net::HttpNetworkLayer::CreateFactory(session.get()));

  url_request_context_ = new net::URLRequestContext;
  url_request_context_->CopyFrom(url_request_context);
  url_request_context_->set_http_transaction_factory(
      http_transaction_factory_.get());

  finished_callback_ = callback;
  for (size_t i = 0; i < requests.size(); ++i) {
    requests_.push_back(new Request(i, base_url, requests[i], this,
                                    url_request_context_.get()));
  }
  if (collect_server_stats) {
    RequestInfo info;
    info.filename = "stats.txt";
    // This is just to determine the expected length of the response.
    // StatsRequest doesn't expect this exact value, but it does expect this
    // exact length.
    info.expected_response =
        "were_all_requests_http_1_1:1,max_pipeline_depth:5";
    requests_.push_back(new StatsRequest(requests.size(), base_url, info, this,
                                         url_request_context_.get()));
  }
}

void HttpPipeliningCompatibilityClient::OnRequestFinished(int request_id,
                                                          Status status) {
  // The CACHE_HISTOGRAM_* macros are used, because they allow dynamic metric
  // names.
  CACHE_HISTOGRAM_ENUMERATION(GetMetricName(request_id, "Status"),
                              status, STATUS_MAX);
  ++num_finished_;
  if (status == SUCCESS) {
    ++num_succeeded_;
  }
  if (num_finished_ == requests_.size()) {
    UMA_HISTOGRAM_BOOLEAN("NetConnectivity.Pipeline.Success",
                          num_succeeded_ == requests_.size());
    finished_callback_.Run(0);
  }
}

void HttpPipeliningCompatibilityClient::ReportNetworkError(int request_id,
                                                           int error_code) {
  CACHE_HISTOGRAM_ENUMERATION(GetMetricName(request_id, "NetworkError"),
                              -error_code, 900);
}

void HttpPipeliningCompatibilityClient::ReportResponseCode(int request_id,
                                                           int response_code) {
  CACHE_HISTOGRAM_ENUMERATION(GetMetricName(request_id, "ResponseCode"),
                              response_code, 600);
}

std::string HttpPipeliningCompatibilityClient::GetMetricName(
    int request_id, const char* description) {
  return base::StringPrintf("NetConnectivity.Pipeline.%d.%s",
                            request_id, description);
}

HttpPipeliningCompatibilityClient::Request::Request(
    int request_id,
    const std::string& base_url,
    const RequestInfo& info,
    HttpPipeliningCompatibilityClient* client,
    net::URLRequestContext* url_request_context)
    : request_id_(request_id),
      request_(new net::URLRequest(GURL(base_url + info.filename), this)),
      info_(info),
      client_(client) {
  request_->set_context(url_request_context);
  request_->set_load_flags(net::LOAD_BYPASS_CACHE |
                           net::LOAD_DISABLE_CACHE |
                           net::LOAD_DO_NOT_SAVE_COOKIES |
                           net::LOAD_DO_NOT_SEND_COOKIES |
                           net::LOAD_DO_NOT_PROMPT_FOR_LOGIN |
                           net::LOAD_DO_NOT_SEND_AUTH_DATA);
  request_->Start();
}

HttpPipeliningCompatibilityClient::Request::~Request() {
}

void HttpPipeliningCompatibilityClient::Request::OnReceivedRedirect(
    net::URLRequest* request,
    const GURL& new_url,
    bool* defer_redirect) {
  *defer_redirect = true;
  request->Cancel();
  Finished(REDIRECTED);
}

void HttpPipeliningCompatibilityClient::Request::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  Finished(CERT_ERROR);
}

void HttpPipeliningCompatibilityClient::Request::OnResponseStarted(
    net::URLRequest* request) {
  int response_code = request->GetResponseCode();
  if (response_code > 0) {
    client_->ReportResponseCode(request_id_, response_code);
  }
  if (response_code == 200) {
    const net::HttpVersion required_version(1, 1);
    if (request->response_info().headers->GetParsedHttpVersion() <
        required_version) {
      Finished(BAD_HTTP_VERSION);
    } else {
      read_buffer_ = new net::IOBuffer(info_.expected_response.length());
      DoRead();
    }
  } else {
    Finished(BAD_RESPONSE_CODE);
  }
}

void HttpPipeliningCompatibilityClient::Request::OnReadCompleted(
    net::URLRequest* request,
    int bytes_read) {
  if (bytes_read == 0) {
    DoReadFinished();
  } else if (bytes_read < 0) {
    Finished(NETWORK_ERROR);
  } else {
    response_.append(read_buffer_->data(), bytes_read);
    if (response_.length() <= info_.expected_response.length()) {
      DoRead();
    } else if (response_.find(info_.expected_response) == 0) {
      Finished(TOO_LARGE);
    } else {
      Finished(CONTENT_MISMATCH);
    }
  }
}

void HttpPipeliningCompatibilityClient::Request::DoRead() {
  int bytes_read = 0;
  if (request_->Read(read_buffer_.get(), info_.expected_response.length(),
                     &bytes_read)) {
    OnReadCompleted(request_.get(), bytes_read);
  }
}

void HttpPipeliningCompatibilityClient::Request::DoReadFinished() {
  if (response_.length() != info_.expected_response.length()) {
    if (info_.expected_response.find(response_) == 0) {
      Finished(TOO_SMALL);
    } else {
      Finished(CONTENT_MISMATCH);
    }
  } else if (response_ == info_.expected_response) {
    Finished(SUCCESS);
  } else {
    Finished(CONTENT_MISMATCH);
  }
}

void HttpPipeliningCompatibilityClient::Request::Finished(Status result) {
  const net::URLRequestStatus status = request_->status();
  request_.reset();
  if (status.status() == net::URLRequestStatus::FAILED) {
    // Network errors trump all other status codes, because network errors can
    // be detected by the network stack even with real content. If we determine
    // that all pipelining errors can be detected by the network stack, then we
    // don't need to worry about broken proxies.
    client_->ReportNetworkError(request_id_, status.error());
    client_->OnRequestFinished(request_id_, NETWORK_ERROR);
  } else {
    client_->OnRequestFinished(request_id_, result);
  }
  // WARNING: We may be deleted at this point.
}

HttpPipeliningCompatibilityClient::StatsRequest::StatsRequest(
    int request_id,
    const std::string& base_url,
    const RequestInfo& info,
    HttpPipeliningCompatibilityClient* client,
    net::URLRequestContext* url_request_context)
    : Request(request_id, base_url, info, client, url_request_context) {
}

HttpPipeliningCompatibilityClient::StatsRequest::~StatsRequest() {
}

void HttpPipeliningCompatibilityClient::StatsRequest::DoReadFinished() {
  Status status = internal::ProcessStatsResponse(response());
  Finished(status);
}

namespace internal {

HttpPipeliningCompatibilityClient::Status ProcessStatsResponse(
    const std::string& response) {
  bool were_all_requests_http_1_1 = false;
  int max_pipeline_depth = 0;

  std::vector<std::pair<std::string, std::string> > kv_pairs;
  base::SplitStringIntoKeyValuePairs(response, ':', ',', &kv_pairs);

  if (kv_pairs.size() != 2) {
    return HttpPipeliningCompatibilityClient::CORRUPT_STATS;
  }

  for (size_t i = 0; i < kv_pairs.size(); ++i) {
    const std::string& key = kv_pairs[i].first;
    int value;
    if (!base::StringToInt(kv_pairs[i].second, &value)) {
      return HttpPipeliningCompatibilityClient::CORRUPT_STATS;
    }

    if (key == "were_all_requests_http_1_1") {
      were_all_requests_http_1_1 = (value == 1);
    } else if (key == "max_pipeline_depth") {
      max_pipeline_depth = value;
    } else {
      return HttpPipeliningCompatibilityClient::CORRUPT_STATS;
    }
  }

  UMA_HISTOGRAM_BOOLEAN("NetConnectivity.Pipeline.AllHTTP11",
                        were_all_requests_http_1_1);
  UMA_HISTOGRAM_ENUMERATION("NetConnectivity.Pipeline.Depth",
                            max_pipeline_depth, 6);

  return HttpPipeliningCompatibilityClient::SUCCESS;
}

}  // namespace internal

namespace {

void DeleteClient(HttpPipeliningCompatibilityClient* client, int rv) {
  delete client;
}

void CollectPipeliningCapabilityStatsOnIOThread(
    const std::string& pipeline_test_server,
    net::URLRequestContextGetter* url_request_context_getter) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  net::URLRequestContext* url_request_context =
      url_request_context_getter->GetURLRequestContext();
  if (!url_request_context->proxy_service()->config().proxy_rules().empty()) {
    // Pipelining with explicitly configured proxies is disabled for now.
    return;
  }

  const base::FieldTrial::Probability kDivisor = 100;
  base::FieldTrial::Probability probability_to_run_test = 0;

  const char* kTrialName = "HttpPipeliningCompatibility";
  base::FieldTrial* trial = base::FieldTrialList::Find(kTrialName);
  if (trial) {
    return;
  }
  // After April 1, 2012, the trial will disable itself.
  trial = new base::FieldTrial(kTrialName, kDivisor,
                               "disable_test", 2012, 4, 1);

  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_CANARY) {
    probability_to_run_test = 20;
  }

  int collect_stats_group = trial->AppendGroup("enable_test",
                                               probability_to_run_test);
  if (trial->group() != collect_stats_group) {
    return;
  }

  std::vector<HttpPipeliningCompatibilityClient::RequestInfo> requests;

  HttpPipeliningCompatibilityClient::RequestInfo info1;
  info1.filename = "alphabet.txt";
  info1.expected_response = "abcdefghijklmnopqrstuvwxyz";
  requests.push_back(info1);

  HttpPipeliningCompatibilityClient::RequestInfo info2;
  info2.filename = "reverse.txt";
  info2.expected_response = "zyxwvutsrqponmlkjihgfedcba";
  requests.push_back(info2);

  HttpPipeliningCompatibilityClient::RequestInfo info3;
  info3.filename = "cached.txt";
  info3.expected_response = "azbycxdwevfugthsirjqkplomn";
  requests.push_back(info3);

  HttpPipeliningCompatibilityClient::RequestInfo info4;
  info4.filename = "chunked.txt";
  info4.expected_response = "chunkedencodingisfun";
  requests.push_back(info4);

  HttpPipeliningCompatibilityClient::RequestInfo info5;
  info5.filename = "cached.txt";
  info5.expected_response = "azbycxdwevfugthsirjqkplomn";
  requests.push_back(info5);

  HttpPipeliningCompatibilityClient* client =
      new HttpPipeliningCompatibilityClient;
  client->Start(pipeline_test_server, requests, true,
                base::Bind(&DeleteClient, client),
                url_request_context_getter->GetURLRequestContext());
}

}  // anonymous namespace

void CollectPipeliningCapabilityStatsOnUIThread(
    const std::string& pipeline_test_server, IOThread* io_thread) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (pipeline_test_server.empty())
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CollectPipeliningCapabilityStatsOnIOThread,
                 pipeline_test_server,
                 make_scoped_refptr(
                     io_thread->system_url_request_context_getter())));
}

}  // namespace chrome_browser_net
