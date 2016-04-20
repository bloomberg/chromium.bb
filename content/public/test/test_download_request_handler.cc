// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_download_request_handler.h"

#include <inttypes.h>

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"

namespace content {

// Intercepts URLRequests on behalf of TestDownloadRequestHandler. Necessarily
// lives on the IO thread since that's where net::URLRequestFilter invokes the
// URLRequestInterceptor.
class TestDownloadRequestHandler::Interceptor
    : public net::URLRequestInterceptor {
 public:
  // Invoked on the IO thread to register a URLRequestInterceptor for |url|.
  // Returns an IO-thread bound weak pointer for interacting with the
  // interceptor.
  static base::WeakPtr<Interceptor> Register(
      const GURL& url,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner);

  using JobFactory =
      base::Callback<net::URLRequestJob*(net::URLRequest*,
                                         net::NetworkDelegate*,
                                         base::WeakPtr<Interceptor>)>;

  ~Interceptor() override;

  // Unregisters the URLRequestInterceptor. In reality it unregisters whatever
  // was registered to intercept |url_|. Since |this| is owned by
  // net::URLRequestFilter, unregistering the interceptor deletes |this| as a
  // side-effect.
  void Unregister();

  // Change the URLRequestJob factory. Can be called multiple times.
  void SetJobFactory(const JobFactory& factory);

  // Sets |requests| to the vector of completed requests and clears the internal
  // list. The returned requests are stored in the order in which they were
  // reported as being complete (not necessarily the order in which they were
  // received).
  void GetAndResetCompletedRequests(
      TestDownloadRequestHandler::CompletedRequests* requests);

  // Can be called by a URLRequestJob to notify this interceptor of a completed
  // request.
  void AddCompletedRequest(
      const TestDownloadRequestHandler::CompletedRequest& request);

  // Returns the task runner that should be used for invoking any client
  // supplied callbacks.
  scoped_refptr<base::SequencedTaskRunner> GetClientTaskRunner();

 private:
  Interceptor(const GURL& url,
              scoped_refptr<base::SequencedTaskRunner> client_task_runner);

  // net::URLRequestInterceptor
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  TestDownloadRequestHandler::CompletedRequests completed_requests_;
  GURL url_;
  JobFactory job_factory_;
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;

  // mutable because MaybeInterceptRequest() is inexplicably const.
  mutable base::WeakPtrFactory<Interceptor> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(Interceptor);
};

// A URLRequestJob that constructs a response to a URLRequest based on the
// contents of a Parameters object. It can handle partial (i.e. byte range
// requests). Created on and lives on the IO thread.
class TestDownloadRequestHandler::PartialResponseJob
    : public net::URLRequestJob {
 public:
  static net::URLRequestJob* Factory(const Parameters& parameters,
                                     net::URLRequest* request,
                                     net::NetworkDelegate* delegate,
                                     base::WeakPtr<Interceptor> interceptor);

  // URLRequestJob
  void Start() override;
  void GetResponseInfo(net::HttpResponseInfo* response_info) override;
  int64_t GetTotalReceivedBytes() const override;
  bool GetMimeType(std::string* mime_type) const override;
  int GetResponseCode() const override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;

 private:
  PartialResponseJob(std::unique_ptr<Parameters> parameters,
                     base::WeakPtr<Interceptor> interceptor,
                     net::URLRequest* url_request,
                     net::NetworkDelegate* network_delegate);

  ~PartialResponseJob() override;
  void ReportCompletedRequest();
  static void OnStartResponseCallbackOnPossiblyIncorrectThread(
      base::WeakPtr<PartialResponseJob> job,
      const std::string& headers,
      net::Error error);
  void OnStartResponseCallback(const std::string& headers, net::Error error);

  // In general, the Parameters object can specify an explicit OnStart handler.
  // In its absence or if the explicit OnStart handler requests the default
  // behavior, this method can be invoked to respond to the request based on the
  // remaining Parameters fields (as if there was no OnStart handler).
  void HandleOnStartDefault();

  // Respond Start() assuming that any If-Match or If-Range headers have been
  // successfully validated. This handler assumes that there *must* be a Range
  // header even though the spec doesn't strictly require it for If-Match.
  bool HandleRangeAssumingValidatorMatch();

  // Adds headers that describe the entity (Content-Type, ETag, Last-Modified).
  // It also adds an 'Accept-Ranges' header if appropriate.
  void AddCommonEntityHeaders();

  // Schedules NotifyHeadersComplete() to be called and sets
  // offset_of_next_read_ to begin reading. Since this interceptor is avoiding
  // network requests and hence may complete synchronously, it schedules the
  // NotifyHeadersComplete() call asynchronously in order to avoid unexpected
  // re-entrancy.
  void NotifyHeadersCompleteAndPrepareToRead();

  std::unique_ptr<Parameters> parameters_;

  base::WeakPtr<Interceptor> interceptor_;
  net::HttpResponseInfo response_info_;
  int64_t offset_of_next_read_ = -1;
  int64_t requested_range_begin_ = -1;
  int64_t requested_range_end_ = -1;
  int64_t read_byte_count_ = 0;
  base::WeakPtrFactory<PartialResponseJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PartialResponseJob);
};

namespace {

template <class T>
void StoreValueAndInvokeClosure(const base::Closure& closure,
                                T* value_receiver,
                                T value) {
  *value_receiver = value;
  closure.Run();
}

// Xorshift* PRNG from https://en.wikipedia.org/wiki/Xorshift
uint64_t XorShift64StarWithIndex(uint64_t seed, uint64_t index) {
  const uint64_t kMultiplier = UINT64_C(2685821657736338717);
  uint64_t x = seed * kMultiplier + index;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  return x * kMultiplier;
}

void RespondToOnStartedCallbackWithStaticHeaders(
    const std::string& headers,
    const net::HttpRequestHeaders&,
    const TestDownloadRequestHandler::OnStartResponseCallback& callback) {
  callback.Run(headers, net::OK);
}

GURL GetNextURLForDownloadInterceptor() {
  static int index = 0;
  std::string url_string =
      base::StringPrintf("https://%d.default.example.com/download/", ++index);
  return GURL(url_string);
}

scoped_refptr<net::HttpResponseHeaders> HeadersFromString(
    const std::string& headers_string) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
          headers_string.c_str(), headers_string.size()));
  return headers;
}

}  // namespace

// static
net::URLRequestJob* TestDownloadRequestHandler::PartialResponseJob::Factory(
    const Parameters& parameters,
    net::URLRequest* request,
    net::NetworkDelegate* delegate,
    base::WeakPtr<Interceptor> interceptor) {
  return new PartialResponseJob(base::WrapUnique(new Parameters(parameters)),
                                interceptor, request, delegate);
}

TestDownloadRequestHandler::PartialResponseJob::PartialResponseJob(
    std::unique_ptr<Parameters> parameters,
    base::WeakPtr<Interceptor> interceptor,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate)
    : net::URLRequestJob(request, network_delegate),
      parameters_(std::move(parameters)),
      interceptor_(interceptor),
      weak_factory_(this) {
  DCHECK(parameters_.get());
  DCHECK_LT(0, parameters_->size);
  DCHECK_NE(-1, parameters_->pattern_generator_seed);
}

TestDownloadRequestHandler::PartialResponseJob::~PartialResponseJob() {
  ReportCompletedRequest();
}

void TestDownloadRequestHandler::PartialResponseJob::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "Starting request for " << request()->url().spec();

  if (parameters_->on_start_handler.is_null() || !interceptor_.get()) {
    HandleOnStartDefault();
    return;
  }

  DVLOG(1) << "Invoking custom OnStart handler.";
  interceptor_->GetClientTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          parameters_->on_start_handler, request()->extra_request_headers(),
          base::Bind(&PartialResponseJob::
                         OnStartResponseCallbackOnPossiblyIncorrectThread,
                     weak_factory_.GetWeakPtr())));
}

void TestDownloadRequestHandler::PartialResponseJob::GetResponseInfo(
    net::HttpResponseInfo* response_info) {
  *response_info = response_info_;
}

int64_t TestDownloadRequestHandler::PartialResponseJob::GetTotalReceivedBytes()
    const {
  return offset_of_next_read_ - requested_range_begin_;
}

bool TestDownloadRequestHandler::PartialResponseJob::GetMimeType(
    std::string* mime_type) const {
  *mime_type = parameters_->content_type;
  return !parameters_->content_type.empty();
}

int TestDownloadRequestHandler::PartialResponseJob::GetResponseCode() const {
  return response_info_.headers.get() ? response_info_.headers->response_code()
                                      : 0;
}

int TestDownloadRequestHandler::PartialResponseJob::ReadRawData(
    net::IOBuffer* buf,
    int buf_size) {
  DVLOG(1) << "Preparing to read " << buf_size << " bytes";

  // requested_range_begin_ == -1 implies that the body was empty.
  if (offset_of_next_read_ > requested_range_end_ ||
      requested_range_begin_ == -1) {
    DVLOG(1) << "Done reading.";
    return 0;
  }

  int64_t range_end =
      std::min(requested_range_end_, offset_of_next_read_ + buf_size - 1);

  if (!parameters_->injected_errors.empty()) {
    const InjectedError& injected_error = parameters_->injected_errors.front();

    if (offset_of_next_read_ == injected_error.offset) {
      int error = injected_error.error;
      DVLOG(1) << "Returning error " << net::ErrorToString(error);
      parameters_->injected_errors.pop();
      return error;
    }

    if (offset_of_next_read_ < injected_error.offset &&
        injected_error.offset <= range_end)
      range_end = injected_error.offset - 1;
  }
  int bytes_to_copy = (range_end - offset_of_next_read_) + 1;

  TestDownloadRequestHandler::GetPatternBytes(
      parameters_->pattern_generator_seed, offset_of_next_read_, bytes_to_copy,
      buf->data());
  DVLOG(1) << "Read " << bytes_to_copy << " bytes at offset "
           << offset_of_next_read_;
  offset_of_next_read_ += bytes_to_copy;
  read_byte_count_ += bytes_to_copy;
  return bytes_to_copy;
}

void TestDownloadRequestHandler::PartialResponseJob::ReportCompletedRequest() {
  if (interceptor_.get()) {
    TestDownloadRequestHandler::CompletedRequest completed_request;
    completed_request.transferred_byte_count = read_byte_count_;
    completed_request.request_headers = request()->extra_request_headers();
    interceptor_->AddCompletedRequest(completed_request);
  }
}

// static
void TestDownloadRequestHandler::PartialResponseJob::
    OnStartResponseCallbackOnPossiblyIncorrectThread(
        base::WeakPtr<PartialResponseJob> job,
        const std::string& headers,
        net::Error error) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PartialResponseJob::OnStartResponseCallback, job, headers,
                 error));
}

void TestDownloadRequestHandler::PartialResponseJob::OnStartResponseCallback(
    const std::string& headers,
    net::Error error) {
  DVLOG(1) << "OnStartResponse invoked with error:" << error
           << " and headers:" << std::endl
           << headers;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (headers.empty() && error == net::OK) {
    HandleOnStartDefault();
    return;
  }

  if (error != net::OK) {
    NotifyStartError(net::URLRequestStatus::FromError(error));
    return;
  }

  response_info_.headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size()));
  NotifyHeadersCompleteAndPrepareToRead();
}

void TestDownloadRequestHandler::PartialResponseJob::HandleOnStartDefault() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const net::HttpRequestHeaders& extra_headers =
      request()->extra_request_headers();

  DCHECK(request()->method() == "GET") << "PartialResponseJob only "
                                          "knows how to respond to GET "
                                          "requests";
  std::string value;

  // If the request contains an 'If-Range' header and the value matches our
  // ETag, then try to handle the range request.
  if (parameters_->support_byte_ranges &&
      extra_headers.GetHeader(net::HttpRequestHeaders::kIfRange, &value) &&
      value == parameters_->etag && HandleRangeAssumingValidatorMatch()) {
    return;
  }

  if (parameters_->support_byte_ranges &&
      extra_headers.GetHeader("If-Match", &value)) {
    if (value == parameters_->etag && HandleRangeAssumingValidatorMatch())
      return;

    // Unlike If-Range, If-Match returns an error if the validators don't match.
    response_info_.headers = HeadersFromString(
        "HTTP/1.1 412 Precondition failed\r\n"
        "Content-Length: 0\r\n");
    requested_range_begin_ = requested_range_end_ = -1;
    NotifyHeadersCompleteAndPrepareToRead();
    return;
  }

  requested_range_begin_ = 0;
  requested_range_end_ = parameters_->size - 1;
  response_info_.headers =
      HeadersFromString(base::StringPrintf("HTTP/1.1 200 Success\r\n"
                                           "Content-Length: %" PRId64 "\r\n",
                                           parameters_->size));
  AddCommonEntityHeaders();
  NotifyHeadersCompleteAndPrepareToRead();
  return;
}

bool TestDownloadRequestHandler::PartialResponseJob::
    HandleRangeAssumingValidatorMatch() {
  const net::HttpRequestHeaders& extra_headers =
      request()->extra_request_headers();

  std::string range_header;
  std::vector<net::HttpByteRange> byte_ranges;

  // There needs to be a 'Range' header and it should have exactly one range.
  // This server is not going to deal with multiple ranges.
  if (!extra_headers.GetHeader(net::HttpRequestHeaders::kRange,
                               &range_header) ||
      !net::HttpUtil::ParseRangeHeader(range_header, &byte_ranges) ||
      byte_ranges.size() != 1)
    return false;

  // The request may have specified a range that's out of bounds.
  if (!byte_ranges[0].ComputeBounds(parameters_->size)) {
    response_info_.headers = HeadersFromString(
        base::StringPrintf("HTTP/1.1 416 Range not satisfiable\r\n"
                           "Content-Range: bytes */%" PRId64 "\r\n"
                           "Content-Length: 0\r\n",
                           parameters_->size));
    requested_range_begin_ = requested_range_end_ = -1;
    NotifyHeadersCompleteAndPrepareToRead();
    return true;
  }

  requested_range_begin_ = byte_ranges[0].first_byte_position();
  requested_range_end_ = byte_ranges[0].last_byte_position();
  response_info_.headers =
      HeadersFromString(base::StringPrintf(
          "HTTP/1.1 206 Partial content\r\n"
          "Content-Range: bytes %" PRId64 "-%" PRId64 "/%" PRId64 "\r\n"
          "Content-Length: %" PRId64 "\r\n",
          requested_range_begin_, requested_range_end_, parameters_->size,
          (requested_range_end_ - requested_range_begin_) + 1));
  AddCommonEntityHeaders();
  NotifyHeadersCompleteAndPrepareToRead();
  return true;
}

void TestDownloadRequestHandler::PartialResponseJob::AddCommonEntityHeaders() {
  if (parameters_->support_byte_ranges)
    response_info_.headers->AddHeader("Accept-Ranges: bytes");

  if (!parameters_->content_type.empty())
    response_info_.headers->AddHeader(base::StringPrintf(
        "Content-Type: %s", parameters_->content_type.c_str()));

  if (!parameters_->etag.empty())
    response_info_.headers->AddHeader(
        base::StringPrintf("ETag: %s", parameters_->etag.c_str()));

  if (!parameters_->last_modified.empty())
    response_info_.headers->AddHeader(base::StringPrintf(
        "Last-Modified: %s", parameters_->last_modified.c_str()));
}

void TestDownloadRequestHandler::PartialResponseJob::
    NotifyHeadersCompleteAndPrepareToRead() {
  std::string normalized_headers;
  response_info_.headers->GetNormalizedHeaders(&normalized_headers);
  DVLOG(1) << "Notify ready with headers:\n" << normalized_headers;

  offset_of_next_read_ = requested_range_begin_;

  // Flush out injected_errors that no longer apply. We are going to skip over
  // ones where the |offset| == |requested_range_begin_| as well. While it
  // prevents injecting an error at offset 0, it makes it much easier to set up
  // parameter sets for download resumption. It means that when a request is
  // interrupted at offset O, a subsequent request for the range O-<end> won't
  // immediately interrupt as well. If we don't exclude interruptions at
  // relative offset 0, then test writers would need to reset the parameter
  // prior to each resumption rather than once at the beginning of the test.
  while (!parameters_->injected_errors.empty() &&
         parameters_->injected_errors.front().offset <= requested_range_begin_)
    parameters_->injected_errors.pop();

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&PartialResponseJob::NotifyHeadersComplete,
                            weak_factory_.GetWeakPtr()));
}

// static
base::WeakPtr<TestDownloadRequestHandler::Interceptor>
TestDownloadRequestHandler::Interceptor::Register(
    const GURL& url,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner) {
  DCHECK(url.is_valid());
  std::unique_ptr<Interceptor> interceptor(
      new Interceptor(url, client_task_runner));
  base::WeakPtr<Interceptor> weak_reference =
      interceptor->weak_ptr_factory_.GetWeakPtr();
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddUrlInterceptor(url, std::move(interceptor));
  return weak_reference;
}

void TestDownloadRequestHandler::Interceptor::Unregister() {
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  GURL url = url_;  // Make a copy as |this| will be deleted.
  filter->RemoveUrlHandler(url);
  // We are deleted now since the filter owned |this|.
}

void TestDownloadRequestHandler::Interceptor::SetJobFactory(
    const JobFactory& job_factory) {
  job_factory_ = job_factory;
}

void TestDownloadRequestHandler::Interceptor::GetAndResetCompletedRequests(
    TestDownloadRequestHandler::CompletedRequests* requests) {
  requests->clear();
  completed_requests_.swap(*requests);
}

void TestDownloadRequestHandler::Interceptor::AddCompletedRequest(
    const TestDownloadRequestHandler::CompletedRequest& request) {
  completed_requests_.push_back(request);
}

scoped_refptr<base::SequencedTaskRunner>
TestDownloadRequestHandler::Interceptor::GetClientTaskRunner() {
  return client_task_runner_;
}

TestDownloadRequestHandler::Interceptor::Interceptor(
    const GURL& url,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner)
    : url_(url),
      client_task_runner_(client_task_runner),
      weak_ptr_factory_(this) {}

TestDownloadRequestHandler::Interceptor::~Interceptor() {}

net::URLRequestJob*
TestDownloadRequestHandler::Interceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  DVLOG(1) << "Intercepting request for " << request->url()
           << " with headers:\n" << request->extra_request_headers().ToString();
  if (job_factory_.is_null())
    return nullptr;
  return job_factory_.Run(request, network_delegate,
                          weak_ptr_factory_.GetWeakPtr());
}

TestDownloadRequestHandler::InjectedError::InjectedError(int64_t offset,
                                                         net::Error error)
    : offset(offset), error(error) {}

// static
TestDownloadRequestHandler::Parameters
TestDownloadRequestHandler::Parameters::WithSingleInterruption() {
  Parameters parameters;
  parameters.injected_errors.push(
      InjectedError(parameters.size / 2, net::ERR_CONNECTION_RESET));
  return parameters;
}

TestDownloadRequestHandler::Parameters::Parameters()
    : etag("abcd"),
      last_modified("Tue, 15 Nov 1994 12:45:26 GMT"),
      content_type("application/octet-stream"),
      size(102400),
      pattern_generator_seed(1),
      support_byte_ranges(true) {}

// Copy and move constructors / assignment operators are all defaults.
TestDownloadRequestHandler::Parameters::Parameters(const Parameters&) = default;
TestDownloadRequestHandler::Parameters& TestDownloadRequestHandler::Parameters::
operator=(const Parameters&) = default;

TestDownloadRequestHandler::Parameters::Parameters(Parameters&& that)
    : etag(std::move(that.etag)),
      last_modified(std::move(that.last_modified)),
      content_type(std::move(that.content_type)),
      size(that.size),
      pattern_generator_seed(that.pattern_generator_seed),
      support_byte_ranges(that.support_byte_ranges),
      on_start_handler(that.on_start_handler),
      injected_errors(std::move(that.injected_errors)) {}

TestDownloadRequestHandler::Parameters& TestDownloadRequestHandler::Parameters::
operator=(Parameters&& that) {
  etag = std::move(that.etag);
  last_modified = std::move(that.etag);
  content_type = std::move(that.content_type);
  size = that.size;
  pattern_generator_seed = that.pattern_generator_seed;
  support_byte_ranges = that.support_byte_ranges;
  on_start_handler = that.on_start_handler;
  injected_errors = std::move(that.injected_errors);
  return *this;
}

TestDownloadRequestHandler::Parameters::~Parameters() {}

void TestDownloadRequestHandler::Parameters::ClearInjectedErrors() {
  std::queue<InjectedError> empty_error_list;
  injected_errors.swap(empty_error_list);
}

TestDownloadRequestHandler::TestDownloadRequestHandler()
    : TestDownloadRequestHandler(GetNextURLForDownloadInterceptor()) {}

TestDownloadRequestHandler::TestDownloadRequestHandler(const GURL& url)
    : url_(url) {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  base::RunLoop run_loop;
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&Interceptor::Register, url_,
                 base::SequencedTaskRunnerHandle::Get()),
      base::Bind(&StoreValueAndInvokeClosure<base::WeakPtr<Interceptor>>,
                 run_loop.QuitClosure(), &interceptor_));
  run_loop.Run();
}

void TestDownloadRequestHandler::StartServing(const Parameters& parameters) {
  DCHECK(CalledOnValidThread());
  Interceptor::JobFactory job_factory =
      base::Bind(&PartialResponseJob::Factory, parameters);
  // Interceptor, if valid, is already registered and serving requests. We just
  // need to set the correct job factory for it to start serving using the new
  // parameters.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&Interceptor::SetJobFactory, interceptor_, job_factory));
}

void TestDownloadRequestHandler::StartServingStaticResponse(
    const base::StringPiece& headers) {
  DCHECK(CalledOnValidThread());
  Parameters parameters;
  parameters.on_start_handler = base::Bind(
      &RespondToOnStartedCallbackWithStaticHeaders, headers.as_string());
  StartServing(parameters);
}

// static
void TestDownloadRequestHandler::GetPatternBytes(int seed,
                                                 int64_t starting_offset,
                                                 int length,
                                                 char* buffer) {
  int64_t seed_offset = starting_offset / sizeof(int64_t);
  int64_t first_byte_position = starting_offset % sizeof(int64_t);
  while (length > 0) {
    uint64_t data = XorShift64StarWithIndex(seed, seed_offset);
    int length_to_copy =
        std::min(length, static_cast<int>(sizeof(data) - first_byte_position));
    memcpy(buffer, reinterpret_cast<char*>(&data) + first_byte_position,
           length_to_copy);
    buffer += length_to_copy;
    length -= length_to_copy;
    ++seed_offset;
    first_byte_position = 0;
  }
}

TestDownloadRequestHandler::~TestDownloadRequestHandler() {
  DCHECK(CalledOnValidThread());
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Interceptor::Unregister, interceptor_));
}

void TestDownloadRequestHandler::GetCompletedRequestInfo(
    TestDownloadRequestHandler::CompletedRequests* requests) {
  DCHECK(CalledOnValidThread());
  base::RunLoop run_loop;
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&Interceptor::GetAndResetCompletedRequests, interceptor_,
                 requests),
      run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace content
