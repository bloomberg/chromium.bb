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

// Class for performing a background test of users' Internet connections.
// Fetches a collection of resources on a test server and verifies all were
// received correctly. This will be used to determine whether or not proxies are
// interfering with a user's ability to use HTTP pipelining. Results are
// recorded with UMA.
//
// TODO(simonjam): Connect this to something. We should start with a field trial
// that affects a subset of canary channel users. But first, we need a test
// server.
class HttpPipeliningCompatibilityClient {
 public:
  struct RequestInfo {
    std::string filename;  // The path relative to the test server's base_url.
    std::string expected_response;  // The expected body of the response.
  };

  enum Status {
    SUCCESS,
    REDIRECTED,         // Response was redirected. We won't follow.
    CERT_ERROR,         // Any certificate problem.
    BAD_RESPONSE_CODE,  // Any non-200 response.
    NETWORK_ERROR,      // Any socket error reported by the network layer.
    TOO_LARGE,          // The response matched, but had extra data on the end.
    TOO_SMALL,          // The response was shorter than expected, but what we
                        // got matched.
    CONTENT_MISMATCH,   // The response didn't match the expected value.
    BAD_HTTP_VERSION,   // Any version older than HTTP/1.1.
    CORRUPT_STATS,      // The stats.txt response was corrupt.
    STATUS_MAX,
  };

  HttpPipeliningCompatibilityClient();
  ~HttpPipeliningCompatibilityClient();

  // Launches the asynchronous URLRequests to fetch the URLs specified by
  // |requests| combined with |base_url|. |base_url| should match the pattern
  // "http://host/". |callback| is invoked once all the requests have completed.
  // URLRequests are initiated in |url_request_context|. Results are recorded to
  // UMA as they are received. If |collect_server_stats| is true, also collects
  // pipelining information recorded by the server.
  void Start(const std::string& base_url,
             std::vector<RequestInfo>& requests,
             bool collect_server_stats,
             const net::CompletionCallback& callback,
             net::URLRequestContext* url_request_context);

 private:
  // There is one Request per RequestInfo passed in to Start() above.
  class Request : public net::URLRequest::Delegate {
   public:
    Request(int request_id,
            const std::string& base_url,
            const RequestInfo& info,
            HttpPipeliningCompatibilityClient* client,
            net::URLRequestContext* url_request_context);
    virtual ~Request();

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

   protected:
    // Called when this request has determined its result. Returns the result to
    // the |client_|.
    void Finished(Status result);

    const std::string& response() const { return response_; }

   private:
    // Called when a response can be read. Reads bytes into |response_| until it
    // consumes the entire response or it encounters an error.
    void DoRead();

    // Called when all bytes have been received. Compares the |response_| to
    // |info_|'s expected response.
    virtual void DoReadFinished();

    const int request_id_;
    scoped_ptr<net::URLRequest> request_;
    const RequestInfo info_;
    HttpPipeliningCompatibilityClient* client_;
    scoped_refptr<net::IOBuffer> read_buffer_;
    std::string response_;
  };

  // A special request that parses a /stats.txt response from the test server.
  class StatsRequest : public Request {
   public:
    // Note that |info.expected_response| is only used to determine the correct
    // length of the response. The exact string content isn't used.
    StatsRequest(int request_id,
                 const std::string& base_url,
                 const RequestInfo& info,
                 HttpPipeliningCompatibilityClient* client,
                 net::URLRequestContext* url_request_context);
    virtual ~StatsRequest();

   private:
    virtual void DoReadFinished() OVERRIDE;
  };

  // Called when a Request determines its result. Reports to UMA.
  void OnRequestFinished(int request_id, Status status);

  // Called when a Request encounters a network error. Reports to UMA.
  void ReportNetworkError(int request_id, int error_code);

  // Called when a Request determines its HTTP response code. Reports to UMA.
  void ReportResponseCode(int request_id, int response_code);

  // Returns the full UMA metric name based on |request_id| and |description|.
  std::string GetMetricName(int request_id, const char* description);

  ScopedVector<Request> requests_;
  net::CompletionCallback finished_callback_;
  size_t num_finished_;
  size_t num_succeeded_;
  scoped_ptr<net::HttpTransactionFactory> http_transaction_factory_;
  scoped_refptr<net::URLRequestContext> url_request_context_;
};

namespace internal {

HttpPipeliningCompatibilityClient::Status ProcessStatsResponse(
    const std::string& response);

}  // namespace internal

void CollectPipeliningCapabilityStatsOnUIThread(
    const std::string& pipeline_test_server, IOThread* io_thread);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_HTTP_PIPELINING_COMPATIBILITY_CLIENT_H_
