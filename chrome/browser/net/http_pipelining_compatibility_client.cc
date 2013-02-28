// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/http_pipelining_compatibility_client.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
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
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace chrome_browser_net {

static const int kCanaryRequestId = 999;

namespace {

// There is one Request per RequestInfo passed in to Start() above.
class Request : public internal::PipelineTestRequest,
                public net::URLRequest::Delegate {
 public:
  Request(int request_id,
          const std::string& base_url,
          const RequestInfo& info,
          internal::PipelineTestRequest::Delegate* delegate,
          net::URLRequestContext* url_request_context);

  virtual ~Request() {}

  virtual void Start() OVERRIDE;

 protected:
  // Called when this request has determined its result. Returns the result to
  // the |client_|.
  virtual void Finished(internal::PipelineTestRequest::Status result);

  const std::string& response() const { return response_; }

  internal::PipelineTestRequest::Delegate* delegate() { return delegate_; }

 private:
  // Called when a response can be read. Reads bytes into |response_| until it
  // consumes the entire response or it encounters an error.
  void DoRead();

  // Called when all bytes have been received. Compares the |response_| to
  // |info_|'s expected response.
  virtual void DoReadFinished();

  // net::URLRequest::Delegate interface
  virtual void OnReceivedRedirect(net::URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE;
  virtual void OnSSLCertificateError(net::URLRequest* request,
                                     const net::SSLInfo& ssl_info,
                                     bool fatal) OVERRIDE;
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE;

  internal::PipelineTestRequest::Delegate* delegate_;
  const int request_id_;
  scoped_ptr<net::URLRequest> url_request_;
  const RequestInfo info_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  std::string response_;
  int response_code_;
};

Request::Request(int request_id,
                 const std::string& base_url,
                 const RequestInfo& info,
                 internal::PipelineTestRequest::Delegate* delegate,
                 net::URLRequestContext* url_request_context)
    : delegate_(delegate),
      request_id_(request_id),
      url_request_(url_request_context->CreateRequest(
          GURL(base_url + info.filename), this)),
      info_(info),
      response_code_(0) {
  url_request_->set_load_flags(net::LOAD_BYPASS_CACHE |
                               net::LOAD_DISABLE_CACHE |
                               net::LOAD_DO_NOT_SAVE_COOKIES |
                               net::LOAD_DO_NOT_SEND_COOKIES |
                               net::LOAD_DO_NOT_PROMPT_FOR_LOGIN |
                               net::LOAD_DO_NOT_SEND_AUTH_DATA);
}

void Request::Start() {
  url_request_->Start();
}

void Request::OnReceivedRedirect(
    net::URLRequest* request,
    const GURL& new_url,
    bool* defer_redirect) {
  *defer_redirect = true;
  request->Cancel();
  Finished(STATUS_REDIRECTED);
}

void Request::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  Finished(STATUS_CERT_ERROR);
}

void Request::OnResponseStarted(net::URLRequest* request) {
  response_code_ = request->GetResponseCode();
  if (response_code_ != 200) {
    Finished(STATUS_BAD_RESPONSE_CODE);
    return;
  }
  const net::HttpVersion required_version(1, 1);
  if (request->response_info().headers->GetParsedHttpVersion() <
      required_version) {
    Finished(STATUS_BAD_HTTP_VERSION);
    return;
  }
  read_buffer_ = new net::IOBuffer(info_.expected_response.length());
  DoRead();
}

void Request::OnReadCompleted(net::URLRequest* request, int bytes_read) {
  if (bytes_read == 0) {
    DoReadFinished();
  } else if (bytes_read < 0) {
    Finished(STATUS_NETWORK_ERROR);
  } else {
    response_.append(read_buffer_->data(), bytes_read);
    if (response_.length() <= info_.expected_response.length()) {
      DoRead();
    } else if (response_.find(info_.expected_response) == 0) {
      Finished(STATUS_TOO_LARGE);
    } else {
      Finished(STATUS_CONTENT_MISMATCH);
    }
  }
}

void Request::DoRead() {
  int bytes_read = 0;
  if (url_request_->Read(read_buffer_.get(), info_.expected_response.length(),
                         &bytes_read)) {
    OnReadCompleted(url_request_.get(), bytes_read);
  }
}

void Request::DoReadFinished() {
  if (response_.length() != info_.expected_response.length()) {
    if (info_.expected_response.find(response_) == 0) {
      Finished(STATUS_TOO_SMALL);
    } else {
      Finished(STATUS_CONTENT_MISMATCH);
    }
  } else if (response_ == info_.expected_response) {
    Finished(STATUS_SUCCESS);
  } else {
    Finished(STATUS_CONTENT_MISMATCH);
  }
}

void Request::Finished(internal::PipelineTestRequest::Status result) {
  const net::URLRequestStatus status = url_request_->status();
  url_request_.reset();
  if (response_code_ > 0) {
    delegate()->ReportResponseCode(request_id_, response_code_);
  }
  if (status.status() == net::URLRequestStatus::FAILED) {
    // Network errors trump all other status codes, because network errors can
    // be detected by the network stack even with real content. If we determine
    // that all pipelining errors can be detected by the network stack, then we
    // don't need to worry about broken proxies.
    delegate()->ReportNetworkError(request_id_, status.error());
    delegate()->OnRequestFinished(request_id_, STATUS_NETWORK_ERROR);
  } else {
    delegate()->OnRequestFinished(request_id_, result);
  }
  // WARNING: We may be deleted at this point.
}

// A special non-pipelined request sent before pipelining begins to test basic
// HTTP connectivity.
class CanaryRequest : public Request {
 public:
  CanaryRequest(int request_id,
               const std::string& base_url,
               const RequestInfo& info,
               internal::PipelineTestRequest::Delegate* delegate,
               net::URLRequestContext* url_request_context)
      : Request(request_id, base_url, info, delegate, url_request_context) {
  }

  virtual ~CanaryRequest() {}

 private:
  virtual void Finished(
      internal::PipelineTestRequest::Status result) OVERRIDE {
    delegate()->OnCanaryFinished(result);
  }
};

// A special request that parses a /stats.txt response from the test server.
class StatsRequest : public Request {
 public:
  // Note that |info.expected_response| is only used to determine the correct
  // length of the response. The exact string content isn't used.
  StatsRequest(int request_id,
               const std::string& base_url,
               const RequestInfo& info,
               internal::PipelineTestRequest::Delegate* delegate,
               net::URLRequestContext* url_request_context)
      : Request(request_id, base_url, info, delegate, url_request_context) {
  }

  virtual ~StatsRequest() {}

 private:
  virtual void DoReadFinished() OVERRIDE {
    internal::PipelineTestRequest::Status status =
        internal::ProcessStatsResponse(response());
    Finished(status);
  }
};

class RequestFactory : public internal::PipelineTestRequest::Factory {
 public:
  virtual internal::PipelineTestRequest* NewRequest(
      int request_id,
      const std::string& base_url,
      const RequestInfo& info,
      internal::PipelineTestRequest::Delegate* delegate,
      net::URLRequestContext* url_request_context,
      internal::PipelineTestRequest::Type request_type) OVERRIDE {
    switch (request_type) {
      case internal::PipelineTestRequest::TYPE_PIPELINED:
        return new Request(request_id, base_url, info, delegate,
                           url_request_context);

      case internal::PipelineTestRequest::TYPE_CANARY:
        return new CanaryRequest(request_id, base_url, info, delegate,
                                 url_request_context);

      case internal::PipelineTestRequest::TYPE_STATS:
        return new StatsRequest(request_id, base_url, info, delegate,
                                url_request_context);

      default:
        NOTREACHED();
        return NULL;
    }
  }
};

}  // anonymous namespace

HttpPipeliningCompatibilityClient::HttpPipeliningCompatibilityClient(
    internal::PipelineTestRequest::Factory* factory)
    : factory_(factory),
      num_finished_(0),
      num_succeeded_(0) {
  if (!factory_.get()) {
    factory_.reset(new RequestFactory);
  }
}

HttpPipeliningCompatibilityClient::~HttpPipeliningCompatibilityClient() {
}

void HttpPipeliningCompatibilityClient::Start(
    const std::string& base_url,
    std::vector<RequestInfo>& requests,
    Options options,
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

  url_request_context_.reset(new net::URLRequestContext);
  url_request_context_->CopyFrom(url_request_context);
  url_request_context_->set_http_transaction_factory(
      http_transaction_factory_.get());

  finished_callback_ = callback;
  for (size_t i = 0; i < requests.size(); ++i) {
    requests_.push_back(factory_->NewRequest(
        i, base_url, requests[i], this, url_request_context_.get(),
        internal::PipelineTestRequest::TYPE_PIPELINED));
  }
  if (options == PIPE_TEST_COLLECT_SERVER_STATS ||
      options == PIPE_TEST_CANARY_AND_STATS) {
    RequestInfo info;
    info.filename = "stats.txt";
    // This is just to determine the expected length of the response.
    // StatsRequest doesn't expect this exact value, but it does expect this
    // exact length.
    info.expected_response =
        "were_all_requests_http_1_1:1,max_pipeline_depth:5";
    requests_.push_back(factory_->NewRequest(
        requests.size(), base_url, info, this, url_request_context_.get(),
        internal::PipelineTestRequest::TYPE_STATS));
  }
  if (options == PIPE_TEST_RUN_CANARY_REQUEST ||
      options == PIPE_TEST_CANARY_AND_STATS) {
    RequestInfo info;
    info.filename = "index.html";
    info.expected_response =
        "\nThis is a test server operated by Google. It's used by Google "
        "Chrome to test\nproxies for compatibility with HTTP pipelining. More "
        "information can be found\nhere:\n\nhttp://dev.chromium.org/developers/"
        "design-documents/network-stack/http-pipelining\n\nSource code can be "
        "found here:\n\nhttp://code.google.com/p/http-pipelining-test/\n";
    canary_request_.reset(factory_->NewRequest(
        kCanaryRequestId, base_url, info, this, url_request_context,
        internal::PipelineTestRequest::TYPE_CANARY));
    canary_request_->Start();
  } else {
    StartTestRequests();
  }
}

void HttpPipeliningCompatibilityClient::StartTestRequests() {
  for (size_t i = 0; i < requests_.size(); ++i) {
    requests_[i]->Start();
  }
}

void HttpPipeliningCompatibilityClient::OnCanaryFinished(
    internal::PipelineTestRequest::Status status) {
  canary_request_.reset();
  bool success = (status == internal::PipelineTestRequest::STATUS_SUCCESS);
  UMA_HISTOGRAM_BOOLEAN("NetConnectivity.Pipeline.CanarySuccess", success);
  if (success) {
    StartTestRequests();
  } else {
    finished_callback_.Run(0);
  }
}

void HttpPipeliningCompatibilityClient::OnRequestFinished(
    int request_id, internal::PipelineTestRequest::Status status) {
  // The CACHE_HISTOGRAM_* macros are used, because they allow dynamic metric
  // names.
  CACHE_HISTOGRAM_ENUMERATION(GetMetricName(request_id, "Status"),
                              status,
                              internal::PipelineTestRequest::STATUS_MAX);

  ++num_finished_;
  if (status == internal::PipelineTestRequest::STATUS_SUCCESS) {
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

namespace internal {

internal::PipelineTestRequest::Status ProcessStatsResponse(
    const std::string& response) {
  bool were_all_requests_http_1_1 = false;
  int max_pipeline_depth = 0;

  std::vector<std::pair<std::string, std::string> > kv_pairs;
  base::SplitStringIntoKeyValuePairs(response, ':', ',', &kv_pairs);

  if (kv_pairs.size() != 2) {
    return internal::PipelineTestRequest::STATUS_CORRUPT_STATS;
  }

  for (size_t i = 0; i < kv_pairs.size(); ++i) {
    const std::string& key = kv_pairs[i].first;
    int value;
    if (!base::StringToInt(kv_pairs[i].second, &value)) {
      return internal::PipelineTestRequest::STATUS_CORRUPT_STATS;
    }

    if (key == "were_all_requests_http_1_1") {
      were_all_requests_http_1_1 = (value == 1);
    } else if (key == "max_pipeline_depth") {
      max_pipeline_depth = value;
    } else {
      return internal::PipelineTestRequest::STATUS_CORRUPT_STATS;
    }
  }

  UMA_HISTOGRAM_BOOLEAN("NetConnectivity.Pipeline.AllHTTP11",
                        were_all_requests_http_1_1);
  UMA_HISTOGRAM_ENUMERATION("NetConnectivity.Pipeline.Depth",
                            max_pipeline_depth, 6);

  return internal::PipelineTestRequest::STATUS_SUCCESS;
}

}  // namespace internal

namespace {

void DeleteClient(IOThread* io_thread, int /* rv */) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  io_thread->globals()->http_pipelining_compatibility_client.reset();
}

void CollectPipeliningCapabilityStatsOnIOThread(
    const std::string& pipeline_test_server,
    IOThread* io_thread) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  net::URLRequestContext* url_request_context =
      io_thread->globals()->system_request_context.get();
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
  // After May 4, 2012, the trial will disable itself.
  trial = base::FieldTrialList::FactoryGetFieldTrial(
      kTrialName, kDivisor, "disable_test", 2012, 5, 4, NULL);

  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_CANARY) {
    probability_to_run_test = 100;
  } else if (channel == chrome::VersionInfo::CHANNEL_DEV) {
    probability_to_run_test = 100;
  }

  int collect_stats_group = trial->AppendGroup("enable_test",
                                               probability_to_run_test);
  if (trial->group() != collect_stats_group) {
    return;
  }

  std::vector<RequestInfo> requests;

  RequestInfo info0;
  info0.filename = "alphabet.txt";
  info0.expected_response = "abcdefghijklmnopqrstuvwxyz";
  requests.push_back(info0);

  RequestInfo info1;
  info1.filename = "cached.txt";
  info1.expected_response = "azbycxdwevfugthsirjqkplomn";
  requests.push_back(info1);

  RequestInfo info2;
  info2.filename = "reverse.txt";
  info2.expected_response = "zyxwvutsrqponmlkjihgfedcba";
  requests.push_back(info2);

  RequestInfo info3;
  info3.filename = "chunked.txt";
  info3.expected_response = "chunkedencodingisfun";
  requests.push_back(info3);

  RequestInfo info4;
  info4.filename = "cached.txt";
  info4.expected_response = "azbycxdwevfugthsirjqkplomn";
  requests.push_back(info4);

  HttpPipeliningCompatibilityClient* client =
      new HttpPipeliningCompatibilityClient(NULL);
  client->Start(pipeline_test_server, requests,
                HttpPipeliningCompatibilityClient::PIPE_TEST_CANARY_AND_STATS,
                base::Bind(&DeleteClient, io_thread),
                url_request_context);
  io_thread->globals()->http_pipelining_compatibility_client.reset(client);
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
                 io_thread));
}

}  // namespace chrome_browser_net
