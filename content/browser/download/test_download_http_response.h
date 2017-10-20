// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_TEST_DOWNLOAD_HTTP_RESPONSE_H_
#define CONTENT_BROWSER_DOWNLOAD_TEST_DOWNLOAD_HTTP_RESPONSE_H_

#include <set>
#include <string>

#include "base/containers/queue.h"
#include "net/http/http_response_info.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace content {

/*
 * Class for configuring and returning http responses for download requests.
 * TODO(qinmin): remove TestDownloadRequestHandler and port all tests to use
 * this class. http://crbug.com/776973.
 */
class TestDownloadHttpResponse : public net::test_server::HttpResponse {
 public:
  static const char kTestDownloadHostName[];
  static GURL GetNextURLForDownload();

  // OnPauseHandler can be used to pause the response until the enclosed
  // callback is called.
  using OnPauseHandler = base::Callback<void(const base::Closure&)>;

  // Called when an injected error triggers.
  using InjectErrorCallback = base::Callback<void(int64_t, int64_t)>;

  struct Parameters {
    // Constructs a Parameters structure using the default constructor, but with
    // the injection of an error which will be triggered at byte offset
    // (filesize / 2). |inject_error_cb| will be called once the response is
    // sent.
    static Parameters WithSingleInterruption(
        const InjectErrorCallback& inject_error_cb);

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

    // If specified, return this as the http response to the client.
    std::string static_response;

    // Error offsets to be injected. The response will successfully fulfill
    // requests to read up to offset. An attempt to read the byte at offset
    // cause |inject_error_cb| to run.
    //
    // If a read spans the range containing an offset, then the portion of the
    // request preceding the offset will succeed. The next read would start at
    // offset and hence would result in an error.
    //
    // E.g.: injected_errors.push(100);
    //
    //    A network read for 1024 bytes at offset 0 would result in successfully
    //    reading 100 bytes (bytes with offset 0-99). The next read would,
    //    therefore, start at offset 100 and would result in |inject_error_cb|
    //    to get called.
    //
    // Injected errors are processed in the order in which they appear in
    // |injected_errors|. When handling a network request for the range [S,E]
    // (inclusive), all events in |injected_errors| whose offset is less than
    // S will be ignored. The first event remaining will trigger an error once
    // the sequence of reads proceeds to a point where its offset is included
    // in [S,E].
    //
    // This implies that |injected_errors| must be specified in increasing order
    // of offset. I.e. |injected_errors| must be sorted by offset.
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
    // An injected error with -1 offset will immediately abort the response
    // without sending anything to the client, including the response header.
    base::queue<int64_t> injected_errors;

    // Callback to run when an injected error triggers.
    InjectErrorCallback inject_error_cb;

    // If on_pause_handler is valid, it will be invoked when the
    // |pause_condition| is reached.
    OnPauseHandler on_pause_handler;

    // Offset of body to pause the response sending. A -1 offset will pause
    // the response before header is sent.
    int64_t pause_offset;
  };

  // Information about completed requests.
  struct CompletedRequest {
    CompletedRequest(const net::test_server::HttpRequest& request);
    ~CompletedRequest();

    // Count of bytes returned in the response body.
    int64_t transferred_byte_count = 0;

    // The request received from the client.
    net::test_server::HttpRequest http_request;
  };

  // Called when response are sent to the client.
  using OnResponseSentCallback =
      base::Callback<void(std::unique_ptr<CompletedRequest>)>;

  // Create an object of this class and return the pointer to the caller.
  static std::unique_ptr<net::test_server::HttpResponse> Create(
      const net::test_server::HttpRequest& request,
      const OnResponseSentCallback& on_response_sent_callback);

  // Generate a pseudo random pattern.
  //
  // |seed| is the seed for the pseudo random sequence.  |offset| is the byte
  // offset into the sequence.  |length| is a count of bytes to generate.
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
  static std::string GetPatternBytes(int seed, int64_t offset, int length);

  // Start responding to http request for |url| with responses based on
  // |parameters|.
  //
  // This method registers the |url| and |parameters| mapping until another
  // call to StartServing() or StartServingStaticResponse() changes it. It
  // can be called on any thread.
  static void StartServing(
      const TestDownloadHttpResponse::Parameters& parameters,
      const GURL& url);

  // Start responding to URLRequests for |url| with a static response
  // containing the headers in |headers|.
  static void StartServingStaticResponse(const std::string& headers,
                                         const GURL& url);

  TestDownloadHttpResponse(
      const net::test_server::HttpRequest& request,
      const Parameters& parameters,
      const OnResponseSentCallback& on_response_sent_callback);
  ~TestDownloadHttpResponse() override;

  // net::test_server::HttpResponse implementations.
  void SendResponse(
      const net::test_server::SendBytesCallback& send,
      const net::test_server::SendCompleteCallback& done) override;

 private:
  // Called to append the range headers to |response| when validator matches.
  // Returns true if the range headers can be added, or false if the request
  // is invalid.
  bool HandleRangeAssumingValidatorMatch(std::string& response);

  // Adds headers that describe the entity (Content-Type, ETag, Last-Modified).
  std::string GetCommonEntityHeaders();

  // Gets the response headers to send.
  std::string GetResponseHeaders();

  // Gets the response body to send.
  std::string GetResponseBody();

  // The parsed beginning and end range offset from the |request_|.
  int64_t requested_range_begin_;
  int64_t requested_range_end_;

  // Parameters associated with this response.
  Parameters parameters_;

  // Request received from the client
  net::test_server::HttpRequest request_;

  // Callback to run when the response is sent.
  OnResponseSentCallback on_response_sent_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestDownloadHttpResponse);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_TEST_DOWNLOAD_HTTP_RESPONSE_H_
