// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/test_download_http_response.h"

#include <inttypes.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"

namespace content {

namespace {

// Lock object for protecting |g_parameters_map|.
base::LazyInstance<base::Lock>::Leaky g_lock = LAZY_INSTANCE_INITIALIZER;

using ParametersMap = std::map<GURL, TestDownloadHttpResponse::Parameters>;
// Maps url to Parameters so that requests for the same URL will get the same
// parameters.
base::LazyInstance<ParametersMap>::Leaky g_parameters_map =
    LAZY_INSTANCE_INITIALIZER;

const char* kTestDownloadPath = "/download/";

// Xorshift* PRNG from https://en.wikipedia.org/wiki/Xorshift
uint64_t XorShift64StarWithIndex(uint64_t seed, uint64_t index) {
  const uint64_t kMultiplier = UINT64_C(2685821657736338717);
  uint64_t x = seed * kMultiplier + index;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  return x * kMultiplier;
}

// Called when the response is sent to the client.
void OnResponseBodySent(const base::Closure& request_completed_cb,
                        const base::Closure& inject_error_cb,
                        const net::test_server::SendCompleteCallback& done) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, request_completed_cb);
  if (!inject_error_cb.is_null())
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, inject_error_cb);
  done.Run();
}

// Called to resume the response.
void OnResume(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
              const base::Closure& resume_callback) {
  task_runner->PostTask(FROM_HERE, resume_callback);
}

// Forward declaration.
void SendResponses(const std::string& header,
                   const std::string& body,
                   int64_t starting_body_offset,
                   const TestDownloadHttpResponse::Parameters& parameters,
                   const base::Closure& completion_callback,
                   const net::test_server::SendBytesCallback& send);

void PauseResponsesAndWaitForResumption(
    const std::string& header,
    const std::string& body,
    int64_t starting_body_offset,
    const TestDownloadHttpResponse::Parameters& parameters,
    const base::Closure& completion_callback,
    const net::test_server::SendBytesCallback& send) {
  TestDownloadHttpResponse::Parameters params = std::move(parameters);
  // Clean up the on_pause_handler so response will not be paused again.
  params.on_pause_handler.Reset();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(parameters.on_pause_handler,
                 base::Bind(&OnResume, base::ThreadTaskRunnerHandle::Get(),
                            base::Bind(&SendResponses, header, body,
                                       starting_body_offset, std::move(params),
                                       completion_callback, send))));
}

void OnResponseSentOnServerIOThread(
    const TestDownloadHttpResponse::OnResponseSentCallback& callback,
    std::unique_ptr<TestDownloadHttpResponse::CompletedRequest> request) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, base::Passed(&request)));
}

// Send all responses to the client.
void SendResponses(const std::string& header,
                   const std::string& body,
                   int64_t starting_body_offset,
                   const TestDownloadHttpResponse::Parameters& parameters,
                   const base::Closure& completion_callback,
                   const net::test_server::SendBytesCallback& send) {
  if (!parameters.on_pause_handler.is_null() && parameters.pause_offset == -1) {
    PauseResponsesAndWaitForResumption(header, body, starting_body_offset,
                                       std::move(parameters),
                                       completion_callback, send);
    return;
  }

  if (!header.empty()) {
    send.Run(header, base::Bind(&SendResponses, std::string(), body,
                                starting_body_offset, std::move(parameters),
                                completion_callback, send));
    return;
  }

  DCHECK(header.empty());
  if (!parameters.on_pause_handler.is_null() && parameters.pause_offset >= 0) {
    // Check if we need to pause while sending the full body.
    if (starting_body_offset != parameters.pause_offset &&
        parameters.pause_offset >= starting_body_offset &&
        parameters.pause_offset < static_cast<int64_t>(body.size())) {
      DCHECK_EQ(starting_body_offset, 0);
      // Send the body response before the pause_offset.
      send.Run(
          body.substr(0, parameters.pause_offset),
          base::Bind(&SendResponses, header,
                     body.substr(parameters.pause_offset,
                                 body.size() - parameters.pause_offset - 1),
                     parameters.pause_offset, std::move(parameters),
                     completion_callback, send));
      return;
    } else if (starting_body_offset == parameters.pause_offset) {
      // Pause the body response after the pause_offset.
      PauseResponsesAndWaitForResumption(header, body, parameters.pause_offset,
                                         std::move(parameters),
                                         completion_callback, send);
      return;
    }
  }
  send.Run(body, completion_callback);
}

}  // namespace

const char TestDownloadHttpResponse::kTestDownloadHostName[] =
    "*.default.example.com";

// static
GURL TestDownloadHttpResponse::GetNextURLForDownload() {
  static int index = 0;
  std::string url_string = base::StringPrintf("http://%d.default.example.com%s",
                                              ++index, kTestDownloadPath);
  return GURL(url_string);
}

// static
TestDownloadHttpResponse::Parameters
TestDownloadHttpResponse::Parameters::WithSingleInterruption(
    const TestDownloadHttpResponse::InjectErrorCallback& inject_error_cb) {
  Parameters parameters;
  parameters.injected_errors.push(parameters.size / 2);
  parameters.inject_error_cb = inject_error_cb;
  return parameters;
}

TestDownloadHttpResponse::Parameters::Parameters()
    : etag("abcd"),
      last_modified("Tue, 15 Nov 1994 12:45:26 GMT"),
      content_type("application/octet-stream"),
      size(102400),
      pattern_generator_seed(1),
      support_byte_ranges(true),
      connection_type(
          net::HttpResponseInfo::ConnectionInfo::CONNECTION_INFO_UNKNOWN),
      pause_offset(0) {}

// Copy and move constructors / assignment operators are all defaults.
TestDownloadHttpResponse::Parameters::Parameters(const Parameters&) = default;
TestDownloadHttpResponse::Parameters& TestDownloadHttpResponse::Parameters::
operator=(const Parameters&) = default;

TestDownloadHttpResponse::Parameters::Parameters(Parameters&& that)
    : etag(std::move(that.etag)),
      last_modified(std::move(that.last_modified)),
      content_type(std::move(that.content_type)),
      size(that.size),
      pattern_generator_seed(that.pattern_generator_seed),
      support_byte_ranges(that.support_byte_ranges),
      connection_type(that.connection_type),
      static_response(std::move(that.static_response)),
      injected_errors(std::move(that.injected_errors)),
      inject_error_cb(that.inject_error_cb),
      on_pause_handler(that.on_pause_handler),
      pause_offset(that.pause_offset) {}

TestDownloadHttpResponse::Parameters& TestDownloadHttpResponse::Parameters::
operator=(Parameters&& that) {
  etag = std::move(that.etag);
  last_modified = std::move(that.etag);
  content_type = std::move(that.content_type);
  size = that.size;
  pattern_generator_seed = that.pattern_generator_seed;
  support_byte_ranges = that.support_byte_ranges;
  static_response = std::move(that.static_response);
  injected_errors = std::move(that.injected_errors);
  inject_error_cb = that.inject_error_cb;
  on_pause_handler = that.on_pause_handler;
  pause_offset = that.pause_offset;
  return *this;
}

TestDownloadHttpResponse::Parameters::~Parameters() = default;

void TestDownloadHttpResponse::Parameters::ClearInjectedErrors() {
  base::queue<int64_t> empty_error_list;
  injected_errors.swap(empty_error_list);
}

TestDownloadHttpResponse::CompletedRequest::CompletedRequest(
    const net::test_server::HttpRequest& request)
    : http_request(request) {}

TestDownloadHttpResponse::CompletedRequest::~CompletedRequest() = default;

// static
void TestDownloadHttpResponse::StartServing(
    const TestDownloadHttpResponse::Parameters& parameters,
    const GURL& url) {
  base::AutoLock lock(*g_lock.Pointer());
  auto iter = g_parameters_map.Get().find(url);
  if (iter != g_parameters_map.Get().end())
    g_parameters_map.Get().erase(iter);
  g_parameters_map.Get().emplace(url, std::move(parameters));
}

// static
void TestDownloadHttpResponse::StartServingStaticResponse(
    const std::string& response,
    const GURL& url) {
  TestDownloadHttpResponse::Parameters parameters;
  parameters.static_response = response;
  StartServing(std::move(parameters), url);
}

// static
std::unique_ptr<net::test_server::HttpResponse>
TestDownloadHttpResponse::Create(
    const net::test_server::HttpRequest& request,
    const OnResponseSentCallback& on_response_sent_callback) {
  if (request.headers.find(net::HttpRequestHeaders::kHost) ==
      request.headers.end()) {
    return nullptr;
  }

  base::AutoLock lock(*g_lock.Pointer());
  GURL url(base::StringPrintf(
      "http://%s%s", request.headers.at(net::HttpRequestHeaders::kHost).c_str(),
      request.relative_url.c_str()));
  auto iter = g_parameters_map.Get().find(url);
  if (iter != g_parameters_map.Get().end()) {
    return base::MakeUnique<TestDownloadHttpResponse>(
        request, std::move(iter->second), on_response_sent_callback);
  }
  return nullptr;
}

// static
std::string TestDownloadHttpResponse::GetPatternBytes(int seed,
                                                      int64_t starting_offset,
                                                      int length) {
  int64_t seed_offset = starting_offset / sizeof(int64_t);
  int64_t first_byte_position = starting_offset % sizeof(int64_t);
  std::string output;
  while (length > 0) {
    uint64_t data = XorShift64StarWithIndex(seed, seed_offset);
    int length_to_copy =
        std::min(length, static_cast<int>(sizeof(data) - first_byte_position));
    char* start_pos = reinterpret_cast<char*>(&data) + first_byte_position;
    std::string string_to_append(start_pos, start_pos + length_to_copy);
    output.append(string_to_append);
    length -= length_to_copy;
    ++seed_offset;
    first_byte_position = 0;
  }
  return output;
}

TestDownloadHttpResponse::TestDownloadHttpResponse(
    const net::test_server::HttpRequest& request,
    const Parameters& parameters,
    const OnResponseSentCallback& on_response_sent_callback)
    : requested_range_begin_(0),
      requested_range_end_(parameters.size - 1),
      parameters_(std::move(parameters)),
      request_(request),
      on_response_sent_callback_(on_response_sent_callback) {}

TestDownloadHttpResponse::~TestDownloadHttpResponse() = default;

std::string TestDownloadHttpResponse::GetResponseBody() {
  while (!parameters_.injected_errors.empty() &&
         parameters_.injected_errors.front() <= requested_range_begin_) {
    parameters_.injected_errors.pop();
  }

  int range_end = requested_range_end_;
  if (!parameters_.injected_errors.empty()) {
    int64_t error_offset = parameters_.injected_errors.front();
    if (range_end >= error_offset)
      range_end = error_offset - 1;
  }
  int bytes_to_copy = range_end - requested_range_begin_ + 1;

  return GetPatternBytes(parameters_.pattern_generator_seed,
                         requested_range_begin_, bytes_to_copy);
}

void TestDownloadHttpResponse::SendResponse(
    const net::test_server::SendBytesCallback& send,
    const net::test_server::SendCompleteCallback& done) {
  auto completed_request = base::MakeUnique<CompletedRequest>(request_);

  bool should_abort_immediately = !parameters_.injected_errors.empty() &&
                                  parameters_.injected_errors.front() == -1 &&
                                  !parameters_.inject_error_cb.is_null();
  if (should_abort_immediately) {
    send.Run(std::string(), done);
    return;
  }

  std::string header;
  std::string body;
  if (!parameters_.static_response.empty()) {
    header = parameters_.static_response;
  } else {
    header = GetResponseHeaders();
    body = GetResponseBody();
  }
  completed_request->transferred_byte_count = body.size();

  base::Closure request_completed_cb =
      base::Bind(&OnResponseSentOnServerIOThread, on_response_sent_callback_,
                 base::Passed(&completed_request));
  base::Closure inject_error_cb;
  if (!parameters_.injected_errors.empty() &&
      parameters_.injected_errors.front() < requested_range_end_ &&
      !parameters_.inject_error_cb.is_null()) {
    inject_error_cb = base::Bind(parameters_.inject_error_cb,
                                 requested_range_begin_, body.size());
  }

  SendResponses(header, body, requested_range_begin_, std::move(parameters_),
                base::Bind(&OnResponseBodySent, request_completed_cb,
                           inject_error_cb, done),
                send);
}

std::string TestDownloadHttpResponse::GetResponseHeaders() {
  std::string headers;
  if (parameters_.support_byte_ranges &&
      request_.headers.find(net::HttpRequestHeaders::kIfRange) !=
          request_.headers.end() &&
      request_.headers.at(net::HttpRequestHeaders::kIfRange) ==
          parameters_.etag &&
      HandleRangeAssumingValidatorMatch(headers)) {
    return headers;
  }

  if (parameters_.support_byte_ranges &&
      request_.headers.find(net::HttpRequestHeaders::kIfMatch) !=
          request_.headers.end()) {
    if (request_.headers.at(net::HttpRequestHeaders::kIfMatch) !=
            parameters_.etag ||
        !HandleRangeAssumingValidatorMatch(headers)) {
      // Unlike If-Range, If-Match returns an error if the validators don't
      // match.
      headers =
          "HTTP/1.1 412 Precondition failed\r\n"
          "Content-Length: 0\r\n"
          "\r\n";
      requested_range_begin_ = requested_range_end_ = -1;
    }
    return headers;
  }

  headers.append("HTTP/1.1 200 OK\r\n");
  if (parameters_.support_byte_ranges)
    headers.append("Accept-Ranges: bytes\r\n");
  headers.append(
      base::StringPrintf("Content-Length: %" PRId64 "\r\n", parameters_.size));
  headers.append(GetCommonEntityHeaders());
  return headers;
}

bool TestDownloadHttpResponse::HandleRangeAssumingValidatorMatch(
    std::string& response) {
  if (request_.headers.find(net::HttpRequestHeaders::kRange) ==
      request_.headers.end()) {
    return false;
  }

  std::vector<net::HttpByteRange> ranges;
  if (!net::HttpUtil::ParseRangeHeader(
          request_.headers.at(net::HttpRequestHeaders::kRange), &ranges) ||
      ranges.size() != 1) {
    return false;
  }

  // The request may have specified a range that's out of bounds.
  if (!ranges[0].ComputeBounds(parameters_.size)) {
    response = base::StringPrintf(
        "HTTP/1.1 416 Range not satisfiable\r\n"
        "Content-Range: bytes */%" PRId64
        "\r\n"
        "Content-Length: 0\r\n",
        parameters_.size);
    requested_range_begin_ = requested_range_end_ = -1;
    return true;
  }

  requested_range_begin_ = ranges[0].first_byte_position();
  requested_range_end_ = ranges[0].last_byte_position();

  response.append("HTTP/1.1 206 Partial content\r\n");
  response.append(base::StringPrintf(
      "Content-Range: bytes %" PRId64 "-%" PRId64 "/%" PRId64 "\r\n",
      requested_range_begin_, requested_range_end_, parameters_.size));
  response.append(
      base::StringPrintf("Content-Length: %" PRId64 "\r\n",
                         (requested_range_end_ - requested_range_begin_) + 1));
  response.append(GetCommonEntityHeaders());
  return true;
}

std::string TestDownloadHttpResponse::GetCommonEntityHeaders() {
  std::string headers;
  if (!parameters_.content_type.empty()) {
    headers.append(base::StringPrintf("Content-Type: %s\r\n",
                                      parameters_.content_type.c_str()));
  }

  if (!parameters_.etag.empty()) {
    headers.append(
        base::StringPrintf("ETag: %s\r\n", parameters_.etag.c_str()));
  }

  if (!parameters_.last_modified.empty()) {
    headers.append(base::StringPrintf("Last-Modified: %s\r\n",
                                      parameters_.last_modified.c_str()));
  }
  headers.append("\r\n");
  return headers;
}

}  // namespace content
