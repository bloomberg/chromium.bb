// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file comes from Google Home(cast) implementation.

#include "chromeos/services/assistant/chromium_http_connection.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "net/url_request/url_request_context_getter.h"

using net::URLFetcher;
using net::URLRequestStatus;
using HttpResponseHeaders = net::HttpResponseHeaders;
using assistant_client::HttpConnection;

namespace chromeos {
namespace assistant {

namespace {

// Passes response data to ChromiumHttpConnection's Delegate as it's received
// from URLFetcher. Only should be active if EnablePartialResults() has been
// called.
class URLFetcherPartialResponseWriter : public ::net::URLFetcherResponseWriter {
 public:
  URLFetcherPartialResponseWriter(HttpConnection::Delegate* delegate,
                                  const ::net::URLFetcher* fetcher)
      : delegate_(delegate) {
    DCHECK(delegate_);
    DCHECK(fetcher);
    // See comments in Initialize(). This class assumes URLFetcher is not setup
    // to automatically retry on error (which is URLFetcher's default behavior).
    DCHECK_EQ(fetcher->GetMaxRetriesOn5xx(), 0);
  }

  ~URLFetcherPartialResponseWriter() override = default;

  // ::net::URLFetcherResponseWriter implementation:
  int Initialize(::net::CompletionOnceCallback callback) override {
    // The API states that "Calling this method again after a Initialize()
    // success results in discarding already written data". Libassistant's
    // HttpConnection API does not provide a way of doing this. However, this is
    // not an issue because URLFetcher only calls Initialize() multiple times
    // for:
    // * Automatic retries of a 500 status.
    // * Automatic retries when the network changes.
    // Both of these automatic retries are explicitly disabled in
    // ChromiumHttpConnection, so no action is required here. The DCHECK below
    // should fail if this assumption is wrong.
    DCHECK_EQ(total_bytes_written_, 0);
    return ::net::OK;
  }

  int Write(::net::IOBuffer* buffer,
            int num_bytes,
            ::net::CompletionOnceCallback callback) override {
    DCHECK(buffer);
    VLOG(2) << "Notifying Delegate of partial response data";
    std::string response(buffer->data(), num_bytes);
    delegate_->OnPartialResponse(response);
    total_bytes_written_ += num_bytes;
    return num_bytes;
  }

  int Finish(int net_error, ::net::CompletionOnceCallback callback) override {
    return ::net::OK;
  }

 private:
  // ChromiumHttpConnection owns URLFetcher, which owns
  // URLFetcherPartialResponseWriter. Since HttpConnection::Delegate must
  // outlive the top-level ChromiumHttpConnection, a raw pointer is safe here.
  HttpConnection::Delegate* const delegate_;
  int64_t total_bytes_written_ = 0;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherPartialResponseWriter);
};

}  // namespace

ChromiumHttpConnectionFactory::ChromiumHttpConnectionFactory(
    scoped_refptr<::net::URLRequestContextGetter> url_request_context_getter)
    : url_request_context_getter_(url_request_context_getter) {}

ChromiumHttpConnectionFactory::~ChromiumHttpConnectionFactory() = default;

HttpConnection* ChromiumHttpConnectionFactory::Create(
    HttpConnection::Delegate* delegate) {
  return new ChromiumHttpConnection(url_request_context_getter_, delegate);
}

ChromiumHttpConnection::ChromiumHttpConnection(
    scoped_refptr<::net::URLRequestContextGetter> url_request_context_getter,
    Delegate* delegate)
    : url_request_context_getter_(url_request_context_getter),
      delegate_(delegate),
      network_task_runner_(url_request_context_getter->GetNetworkTaskRunner()) {
  DCHECK(url_request_context_getter_);
  DCHECK(delegate_);
  DCHECK(network_task_runner_);

  // URLFetcher does not ignore client cert requests by default.
  // We do not have such certificates.
  // TODO(igorc): Talk to Cast platform to see if we can do better.
  URLFetcher::SetIgnoreCertificateRequests(true);

  // Add a reference, so |this| cannot go away until Close() is called.
  AddRef();
}

ChromiumHttpConnection::~ChromiumHttpConnection() {
  // The destructor may be called on non-network thread when the connection
  // is cancelled early, for example due to a reconfigure event.
  DCHECK(state_ == State::DESTROYED);
}

void ChromiumHttpConnection::SetRequest(const std::string& url, Method method) {
  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ChromiumHttpConnection::SetRequestOnThread, this,
                            url, method));
}

void ChromiumHttpConnection::SetRequestOnThread(const std::string& url,
                                                Method method) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == State::NEW);
  url_ = GURL(url);
  method_ = method;
}

void ChromiumHttpConnection::AddHeader(const std::string& name,
                                       const std::string& value) {
  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ChromiumHttpConnection::AddHeaderOnThread, this,
                            name, value));
}

void ChromiumHttpConnection::AddHeaderOnThread(const std::string& name,
                                               const std::string& value) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == State::NEW);
  // From https://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2:
  // "Multiple message-header fields with the same field-name MAY be present in
  // a message if and only if the entire field-value for that header field is
  // defined as a comma-separated list [i.e., #(values)]. It MUST be possible to
  // combine the multiple header fields into one "field-name: field-value" pair,
  // without changing the semantics of the message, by appending each subsequent
  // field-value to the first, each separated by a comma."
  std::string existing_value;
  if (headers_.GetHeader(name, &existing_value)) {
    headers_.SetHeader(name, existing_value + ',' + value);
  } else {
    headers_.SetHeader(name, value);
  }
}

void ChromiumHttpConnection::SetUploadContent(const std::string& content,
                                              const std::string& content_type) {
  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ChromiumHttpConnection::SetUploadContentOnThread,
                            this, content, content_type));
}

void ChromiumHttpConnection::SetUploadContentOnThread(
    const std::string& content,
    const std::string& content_type) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == State::NEW);
  upload_content_ = content;
  upload_content_type_ = content_type;
  chunked_upload_content_type_ = "";
}

void ChromiumHttpConnection::SetChunkedUploadContentType(
    const std::string& content_type) {
  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ChromiumHttpConnection::SetChunkedUploadContentTypeOnThread,
                 this, content_type));
}

void ChromiumHttpConnection::SetChunkedUploadContentTypeOnThread(
    const std::string& content_type) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == State::NEW);
  upload_content_ = "";
  upload_content_type_ = "";
  chunked_upload_content_type_ = content_type;
}

void ChromiumHttpConnection::EnableHeaderResponse() {
  NOTIMPLEMENTED();
}

void ChromiumHttpConnection::EnablePartialResults() {
  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ChromiumHttpConnection::EnablePartialResultsOnThread, this));
}

void ChromiumHttpConnection::EnablePartialResultsOnThread() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == State::NEW);
  handle_partial_response_ = true;
}

void ChromiumHttpConnection::Start() {
  VLOG(2) << "Requested to start connection";
  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ChromiumHttpConnection::StartOnThread, this));
}

void ChromiumHttpConnection::StartOnThread() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == State::NEW);
  state_ = State::STARTED;

  if (!url_.is_valid()) {
    // Handle invalid URL to prevent URLFetcher crashes.
    state_ = State::COMPLETED;
    VLOG(2) << "Completing connection with invalid URL";
    delegate_->OnNetworkError(-1, "Invalid GURL");
    return;
  }

  URLFetcher::RequestType request_type;
  switch (method_) {
    case Method::GET:
      request_type = URLFetcher::RequestType::GET;
      break;
    case Method::POST:
      request_type = URLFetcher::RequestType::POST;
      break;
    case Method::HEAD:
      request_type = URLFetcher::RequestType::HEAD;
      break;
  }

  DCHECK(!url_fetcher_);
  url_fetcher_ = URLFetcher::Create(url_, request_type, this);
  url_fetcher_->SetLoadFlags(::net::LOAD_DO_NOT_SEND_AUTH_DATA |
                             ::net::LOAD_DO_NOT_SAVE_COOKIES |
                             ::net::LOAD_DO_NOT_SEND_COOKIES);
  url_fetcher_->SetExtraRequestHeaders(headers_.ToString());
  url_fetcher_->SetAutomaticallyRetryOn5xx(false);
  url_fetcher_->SetAutomaticallyRetryOnNetworkChanges(false);

  if (handle_partial_response_) {
    url_fetcher_->SaveResponseWithWriter(
        std::make_unique<URLFetcherPartialResponseWriter>(delegate_,
                                                          url_fetcher_.get()));
  }

  if (!upload_content_type_.empty()) {
    url_fetcher_->SetUploadData(upload_content_type_, upload_content_);
  } else if (!chunked_upload_content_type_.empty() && method_ == Method::POST) {
    url_fetcher_->SetChunkedUpload(chunked_upload_content_type_);
  }

  url_fetcher_->SetRequestContext(url_request_context_getter_.get());
  url_fetcher_->Start();
}

void ChromiumHttpConnection::Pause() {
  NOTIMPLEMENTED();
}

void ChromiumHttpConnection::Resume() {
  NOTIMPLEMENTED();
}

void ChromiumHttpConnection::Close() {
  VLOG(2) << "Requesting to close connection object";
  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ChromiumHttpConnection::CloseOnThread, this));
}

void ChromiumHttpConnection::CloseOnThread() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (state_ == State::DESTROYED)
    return;

  VLOG(2) << "Closing connection object";
  state_ = State::DESTROYED;
  url_fetcher_ = nullptr;

  delegate_->OnConnectionDestroyed();

  Release();
}

void ChromiumHttpConnection::UploadData(const std::string& data,
                                        bool is_last_chunk) {
  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ChromiumHttpConnection::UploadDataOnThread, this,
                            data, is_last_chunk));
}

void ChromiumHttpConnection::UploadDataOnThread(const std::string& data,
                                                bool is_last_chunk) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (state_ != State::STARTED)
    return;

  // URLFetcher does not expose async IO to know when to add more data
  // to the buffer. The write callback is received by
  // HttpStreamParser::DoSendBody() and DoSendBodyComplete(), but there
  // appears to be no way to know that it has happened.
  if (url_fetcher_)
    url_fetcher_->AppendChunkToUpload(data, is_last_chunk);
}

void ChromiumHttpConnection::OnURLFetchComplete(const URLFetcher* source) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (state_ != State::STARTED)
    return;

  state_ = State::COMPLETED;

  DCHECK(url_fetcher_.get() == source);
  int response_code = source->GetResponseCode();
  if (response_code != URLFetcher::RESPONSE_CODE_INVALID) {
    std::string response;
    if (!source->GetResponseAsString(&response)) {
      DCHECK(handle_partial_response_) << "Partial responses are disabled. "
                                          "URLFetcher should be writing "
                                          "response data to string";
    }

    VLOG(2) << "ChromiumHttpConnection completed with response_code="
            << response_code;
    std::string response_headers;
    HttpResponseHeaders* headers = source->GetResponseHeaders();
    if (headers)
      response_headers = headers->raw_headers();

    delegate_->OnCompleteResponse(response_code, response_headers, response);
    return;
  }

  std::string message;
  int error = source->GetStatus().error();
  switch (source->GetStatus().status()) {
    case URLRequestStatus::IO_PENDING:
      message = "IO Pending";
      break;
    case URLRequestStatus::CANCELED:
      message = "Canceled";
      break;
    case URLRequestStatus::FAILED:
      message = "Failed";
      break;
    default:
      message = "Unexpected URLFetcher status";
      break;
  }

  VLOG(2) << "ChromiumHttpConnection completed with network error=" << error
          << ": " << message;
  delegate_->OnNetworkError(error, message);
}

}  // namespace assistant
}  // namespace chromeos
