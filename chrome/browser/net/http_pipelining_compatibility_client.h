// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_HTTP_PIPELINING_COMPATIBILITY_CLIENT_H_
#define CHROME_BROWSER_NET_HTTP_PIPELINING_COMPATIBILITY_CLIENT_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

class IOThread;

namespace chrome_browser_net {

struct RequestInfo {
  std::string filename;  // The path relative to the test server's base_url.
  std::string expected_response;  // The expected body of the response.
};

namespace internal {

class PipelineTestRequest {
 public:
  enum Status {
    STATUS_SUCCESS,
    STATUS_REDIRECTED,         // Response was redirected. We won't follow.
    STATUS_CERT_ERROR,         // Any certificate problem.
    STATUS_BAD_RESPONSE_CODE,  // Any non-200 response.
    STATUS_NETWORK_ERROR,      // Any socket error reported by the network
                               // layer.
    STATUS_TOO_LARGE,          // The response matched, but had extra data on
                               // the end.
    STATUS_TOO_SMALL,          // The response was shorter than expected, but
                               // what we got matched.
    STATUS_CONTENT_MISMATCH,   // The response didn't match the expected value.
    STATUS_BAD_HTTP_VERSION,   // Any version older than HTTP/1.1.
    STATUS_CORRUPT_STATS,      // The stats.txt response was corrupt.
    STATUS_MAX,
  };

  enum Type {
    TYPE_PIPELINED,  // Pipelined test requests.
    TYPE_CANARY,     // A non-pipelined request to verify basic HTTP
                     // connectivity.
    TYPE_STATS,      // Collects stats from the test server.
  };

  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when the canary request completes.
    virtual void OnCanaryFinished(Status status) = 0;

    // Called when a Request determines its result. Reports to UMA.
    virtual void OnRequestFinished(int request_id, Status status) = 0;

    // Called when a Request encounters a network error. Reports to UMA.
    virtual void ReportNetworkError(int request_id, int error_code) = 0;

    // Called when a Request determines its HTTP response code. Reports to UMA.
    virtual void ReportResponseCode(int request_id, int response_code) = 0;
  };

  class Factory {
   public:
    virtual ~Factory() {}

    virtual PipelineTestRequest* NewRequest(
        int request_id,
        const std::string& base_url,
        const RequestInfo& info,
        Delegate* delegate,
        net::URLRequestContext* url_request_context,
        Type type) = 0;
  };

  virtual ~PipelineTestRequest() {}

  virtual void Start() = 0;
};

PipelineTestRequest::Status ProcessStatsResponse(
    const std::string& response);

}  // namespace internal

// Class for performing a background test of users' Internet connections.
// Fetches a collection of resources on a test server and verifies all were
// received correctly. This will be used to determine whether or not proxies are
// interfering with a user's ability to use HTTP pipelining. Results are
// recorded with UMA.
class HttpPipeliningCompatibilityClient
    : public internal::PipelineTestRequest::Delegate {
 public:
  enum Options {
    PIPE_TEST_DEFAULTS,              // Only run the |requests| passed to Start.
    PIPE_TEST_RUN_CANARY_REQUEST,    // Also perform a canary request before
                                     // testing pipelining.
    PIPE_TEST_COLLECT_SERVER_STATS,  // Also request stats from the server after
                                     // the pipeline test completes.
    PIPE_TEST_CANARY_AND_STATS,      // Both of the above.
  };

  HttpPipeliningCompatibilityClient(
      internal::PipelineTestRequest::Factory* factory);
  virtual ~HttpPipeliningCompatibilityClient();

  // Launches the asynchronous URLRequests to fetch the URLs specified by
  // |requests| combined with |base_url|. |base_url| should match the pattern
  // "http://host/". |callback| is invoked once all the requests have completed.
  // URLRequests are initiated in |url_request_context|. Results are recorded to
  // UMA as they are received. If |collect_server_stats| is true, also collects
  // pipelining information recorded by the server.
  void Start(const std::string& base_url,
             std::vector<RequestInfo>& requests,
             Options options,
             const net::CompletionCallback& callback,
             net::URLRequestContext* url_request_context);

 private:
  // Sends the pipelining test requests.
  void StartTestRequests();

  // Returns the full UMA metric name based on |request_id| and |description|.
  std::string GetMetricName(int request_id, const char* description);

  // PipelineTestRequest::Delegate interface.
  virtual void OnCanaryFinished(
      internal::PipelineTestRequest::Status status) OVERRIDE;
  virtual void OnRequestFinished(
      int request_id,
      internal::PipelineTestRequest::Status status) OVERRIDE;
  virtual void ReportNetworkError(int request_id, int error_code) OVERRIDE;
  virtual void ReportResponseCode(int request_id, int response_code) OVERRIDE;

  scoped_ptr<net::HttpTransactionFactory> http_transaction_factory_;
  scoped_ptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<internal::PipelineTestRequest::Factory> factory_;
  ScopedVector<internal::PipelineTestRequest> requests_;
  scoped_ptr<internal::PipelineTestRequest> canary_request_;
  net::CompletionCallback finished_callback_;
  size_t num_finished_;
  size_t num_succeeded_;
};

void CollectPipeliningCapabilityStatsOnUIThread(
    const std::string& pipeline_test_server, IOThread* io_thread);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_HTTP_PIPELINING_COMPATIBILITY_CLIENT_H_
