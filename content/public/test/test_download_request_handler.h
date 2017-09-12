// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_DOWNLOAD_REQUEST_HANDLER_H_
#define CONTENT_PUBLIC_TEST_TEST_DOWNLOAD_REQUEST_HANDLER_H_

#include <stdint.h>

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_job.h"
#include "url/gurl.h"

namespace content {

// A request handler that can be used to mock the behavior of a URLRequestJob
// for a download.
//
// Testing of download interruption scenarios typically involve simulating
// errors that occur:
// 1. On the client, prior to the request being sent out,
// 2. On the network, between the client and the server,
// 3. On the server,
// 4. Back on the client, while writing the response to disk,
// 5. On the client, after the response has been written to disk.
//
// This test class is meant to help test failures in #2 and #3 above. The test
// implementation depends on content::BrowserThread and assumes that the
// thread identified by BrowserThread::IO is the network task runner thread.
//
// TestDownloadRequestHandler can be used on any thread as long as it is used
// and destroyed on the same thread it was constructed on.
//
// To use the test request handler:
//
//  // Define the request handler. Note that initialization of the
//  // TestDownloadRequestHandler object immediately registers it as well and is
//  // a blocking operation.
//  TestDownloadRequestHandler request_handler;
//
//  // Set up parameters for the partial request handler.
//  TestDownloadRequestHandler::Parameters parameters;
//
//  // Inject an error at offset 100.
//  parameters.injected_errors.push(TestDownloadRequestHandler::InjectedError(
//      100, net::ERR_CONNECTION_RESET));
//
//  // Start serving.
//  request_handler.StartServing(parameters);
//
// At this point, you can initiate a URLRequest for request_handler.url(). The
// request will fail when offset 100 is reached with the error specified above.
class TestDownloadRequestHandler {
 public:
  // OnStartHandler can be used to intercept the Start() event of a new
  // URLRequest. Set it as the |on_start_handler| member of Parameters below.
  //
  // The callback is invoked on the thread on which TestDownloadRequestHandler
  // was created. Once the callback has a response ready, it can invoke the
  // OnStartResponseCallback object. The latter can be invoked on any thread and
  // will post back to the IO thread to continue with processing the Start()
  // event.
  //
  // The parameters to the OnStartResponseCallback are:
  //
  // * a |const std::string&| containing the headers to be sent in response to
  //   the request. The headers should be formatted according to the
  //   requirements of net::HttpUtil::AssembleRawHeaders(). The headers are only
  //   used if the |net::Error| parameters is net::OK.
  //
  // * a |net::Error| indicating the result of the operation. If this parameters
  //   is not net::OK, then that error value is set as the result of the Start()
  //   operation. The headers are ignored in this case.
  //
  //   If the error is net::OK, and the headers are empty, then the request is
  //   handled based on the remaining parameters in |Parameters|.
  using OnStartResponseCallback =
      base::Callback<void(const std::string&, net::Error)>;

  using OnStartHandler = base::Callback<void(const net::HttpRequestHeaders&,
                                             const OnStartResponseCallback&)>;

  // An injected error.
  struct InjectedError {
    InjectedError(int64_t offset, net::Error error);

    int64_t offset;
    net::Error error;
  };

  // Parameters used by StartServing().
  struct Parameters {
    // Constructs a Parameters structure using the default constructor, but with
    // the addition of a net::ERR_CONNECTION_RESET which will be triggered at
    // byte offset (filesize / 2).
    static Parameters WithSingleInterruption();

    // The default constructor initializes the parameters for serving a 100 KB
    // resource with no interruptions. The response contains an ETag and a
    // Last-Modified header and the server supports byte range requests.
    Parameters();

    // Parameters is expected to be copyable and moveable.
    Parameters(Parameters&&);
    Parameters(const Parameters&);
    Parameters& operator=(Parameters&&);
    Parameters& operator=(const Parameters&);
    ~Parameters();

    // Clears the errors in injected_errors.
    void ClearInjectedErrors();

    // Contents of the ETag header field of the response.  No Etag header is
    // sent if this field is empty.
    std::string etag;

    // Contents of the Last-Modified header field of the response. No
    // Last-Modified header is sent if this field is empty.
    std::string last_modified;

    // The Content-Type of the response. No Content-Type header is sent if this
    // field is empty.
    std::string content_type;

    // The total size of the entity. If the entire entity is requested, then
    // this would be the same as the value returned in the Content-Length
    // header.
    int64_t size;

    // Seed for the pseudo-random sequence that defines the response body
    // contents. The seed is with GetPatternBytes() to generate the body of the
    // response.
    int pattern_generator_seed;

    // Whether the server can handle partial request.
    // If true, contains a 'Accept-Ranges: bytes' header for HTTP 200
    // response, or contains 'Content-Range' header for HTTP 206 response.
    bool support_byte_ranges;

    // The connection type in the response.
    net::HttpResponseInfo::ConnectionInfo connection_type;

    // If on_start_handler is valid, it will be invoked when a new request is
    // received. See details about the OnStartHandler above.
    OnStartHandler on_start_handler;

    // Errors to be injected. Each injected error is defined by an offset and an
    // error. Request handler will successfully fulfil requests to read up to
    // |offset|. An attempt to read the byte at |offset| will result in the
    // error defined by the InjectErrors object.
    //
    // If a read spans the range containing |offset|, then the portion of the
    // request preceding |offset| will succeed. The next read would start at
    // |offset| and hence would result in an error.
    //
    // E.g.: injected_errors.push(InjectedError(100, ERR_CONNECTION_RESET));
    //
    //    A network read for 1024 bytes at offset 0 would result in successfully
    //    reading 100 bytes (bytes with offset 0-99). The next read would,
    //    therefore, start at offset 100 and would result in
    //    ERR_CONNECTION_RESET.
    //
    // Injected errors are processed in the order in which they appear in
    // |injected_errors|. When handling a network request for the range [S,E]
    // (inclusive), all events in |injected_errors| where |offset| is less than
    // S will be ignored. The first event remaining will trigger an error once
    // the sequence of reads proceeds to a point where its |offset| is included
    // in [S,E].
    //
    // This implies that |injected_errors| must be specified in increasing order
    // of |offset|. I.e. |injected_errors| must be sorted by |offset|.
    //
    // Errors at relative offset 0 are ignored for a partial request. I.e. If
    // the request is for the byte range 100-200, then an error at offset 100
    // will not trigger. This is done so that non-overlapping continuation
    // attempts don't require resetting parameters to succeed.
    //
    // E.g.: If the caller injects an error at offset 100, then a request for
    // the entire entity will fail after reading 100 bytes (offsets 0 through
    // 99). A subsequent request for byte range "100-" (offsets 100 through EOF)
    // will succeed since the error at offset 100 is ignored.
    //
    // Notes:
    //
    // * Distinctions about which read requests signal the error is often only
    //   important at the //net layer. From //content, it would appear that 100
    //   bytes were read and then request failed with ERR_CONNECTION_RESET.
    base::queue<InjectedError> injected_errors;
  };

  // Details about completed requests returned by GetCompletedRequestInfo().
  struct CompletedRequest {
    CompletedRequest();
    CompletedRequest(CompletedRequest&&);
    ~CompletedRequest();

    // Count of bytes read by the client of the URLRequestJob. This counts the
    // number of bytes of the entity that was transferred *after* content
    // decoding is complete.
    int64_t transferred_byte_count = -1;

    net::HttpRequestHeaders request_headers;

    std::string referrer;
    net::URLRequest::ReferrerPolicy referrer_policy =
        net::URLRequest::NEVER_CLEAR_REFERRER;

    GURL site_for_cookies;
    net::URLRequest::FirstPartyURLPolicy first_party_url_policy =
        net::URLRequest::NEVER_CHANGE_FIRST_PARTY_URL;

    url::Origin initiator;

   private:
    DISALLOW_COPY_AND_ASSIGN(CompletedRequest);
  };

  using CompletedRequests = std::vector<std::unique_ptr<CompletedRequest>>;

  // Registers a request handler at the default URL. Call url() to determine the
  // URL.
  //
  // Notes:
  // * This constructor is only meant to be used for convenience when the caller
  //   is not interested in the URL used for interception. The URL used is
  //   generated at run time and should not be assumed to be the same across
  //   different runs of the same test.
  //
  // * Initialization of the handler synchronously runs a task on the
  //   BrowserThread::IO thread using a nested run loop. Only construct an
  //   instance of this object after browser threads have been initialized.
  TestDownloadRequestHandler();

  // Similar to the default constructor, but registers the handler at |url|.
  //
  // Notes:
  // * The behavior is undefined if more than one TestDownloadRequestHandler is
  //   registered for the same URL.
  TestDownloadRequestHandler(const GURL& url);

  // Destroys and posts a task to the IO thread to dismantle the registered URL
  // request interceptor. Does not wait for the task to return.
  ~TestDownloadRequestHandler();

  // Returns the URL that this instance is intercepting URLRequests for.
  const GURL& url() const { return url_; }

  // Start responding to URLRequests for url() with responses based on
  // |parameters|.
  //
  // This method invocation posts a task to the IO thread to update the
  // URLRequestInterceptor with the new parameters and returns immediately. URL
  // interception won't be updated until the posted task executes. The method
  // returns without waiting for the posted task to complete.
  //
  // Calling this method does not affect URLRequests that have already started.
  // The new parameters will only be used to respond to new URLRequests that are
  // starting.
  //
  // StartServing() can be called multiple times to change the operating
  // parameters of the current URL interceptor.
  void StartServing(const Parameters& parameters);

  // Start responding to URLRequests for url() with a static response
  // containing the headers in |headers|.
  //
  // The format of |headers| should comply with the requirements for
  // net::HttpUtil::AssembleRawHeaders().
  void StartServingStaticResponse(const base::StringPiece& headers);

  // Get the list of requests that have already completed.
  //
  // This method posts a task to the IO thread to collect the list of completed
  // requests and waits for the task to complete.
  //
  // Requests that are currently in progress will not be reflected in
  // |requests|.
  void GetCompletedRequestInfo(CompletedRequests* requests);

  // Generate a pseudo random pattern.
  //
  // |seed| is the seed for the pseudo random sequence.  |offset| is the byte
  // offset into the sequence.  |length| is a count of bytes to generate.
  // |data| receives the generated bytes and should be able to store |length|
  // bytes.
  //
  // The pattern has the following properties:
  //
  // * For a given |seed|, the entire sequence of bytes is fixed. Any
  //   subsequence can be generated by specifying the |offset| and |length|.
  //
  // * The sequence is aperiodic (at least for the first 1M bytes).
  //
  // * |seed| is chaotic. Different seeds produce "very different" data. This
  //   means that there's no trivial mapping between sequences generated using
  //   two distinct seeds.
  //
  // These properties make the generated bytes useful for testing partial
  // requests where the response may need to be built using a sequence of
  // partial requests.
  //
  // Note: Don't use this function to generate a cryptographically secure
  // pseudo-random sequence.
  static void GetPatternBytes(int seed, int64_t offset, int length, char* data);

 private:
  class Interceptor;
  class PartialResponseJob;

  GURL url_;
  base::WeakPtr<Interceptor> interceptor_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(TestDownloadRequestHandler);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_DOWNLOAD_REQUEST_HANDLER_H_
