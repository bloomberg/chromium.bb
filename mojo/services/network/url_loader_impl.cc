// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/url_loader_impl.h"

#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/services/network/net_adapters.h"
#include "mojo/services/network/network_context.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"

namespace mojo {
namespace {

// Generates an URLResponsePtr from the response state of a net::URLRequest.
URLResponsePtr MakeURLResponse(const net::URLRequest* url_request) {
  URLResponsePtr response(URLResponse::New());
  response->url = String::From(url_request->url());

  const net::HttpResponseHeaders* headers = url_request->response_headers();
  if (headers) {
    response->status_code = headers->response_code();
    response->status_line = headers->GetStatusLine();

    std::vector<String> header_lines;
    void* iter = nullptr;
    std::string name, value;
    while (headers->EnumerateHeaderLines(&iter, &name, &value))
      header_lines.push_back(name + ": " + value);
    if (!header_lines.empty())
      response->headers.Swap(&header_lines);
  }

  std::string mime_type;
  url_request->GetMimeType(&mime_type);
  response->mime_type = mime_type;

  std::string charset;
  url_request->GetCharset(&charset);
  response->charset = charset;

  return response.Pass();
}

// Reads the request body upload data from a DataPipe.
class UploadDataPipeElementReader : public net::UploadElementReader {
 public:
  UploadDataPipeElementReader(ScopedDataPipeConsumerHandle pipe)
      : pipe_(pipe.Pass()), num_bytes_(0) {}
  ~UploadDataPipeElementReader() override {}

  // UploadElementReader overrides:
  int Init(const net::CompletionCallback& callback) override {
    offset_ = 0;
    ReadDataRaw(pipe_.get(), nullptr, &num_bytes_, MOJO_READ_DATA_FLAG_QUERY);
    return net::OK;
  }
  uint64 GetContentLength() const override { return num_bytes_; }
  uint64 BytesRemaining() const override { return num_bytes_ - offset_; }
  bool IsInMemory() const override { return false; }
  int Read(net::IOBuffer* buf,
           int buf_length,
           const net::CompletionCallback& callback) override {
    uint32_t bytes_read =
        std::min(static_cast<uint32_t>(BytesRemaining()),
                 static_cast<uint32_t>(buf_length));
    if (bytes_read > 0) {
      ReadDataRaw(pipe_.get(), buf->data(), &bytes_read,
                  MOJO_READ_DATA_FLAG_NONE);
    }

    offset_ += bytes_read;
    return bytes_read;
  }

 private:
  ScopedDataPipeConsumerHandle pipe_;
  uint32_t num_bytes_;
  uint32_t offset_;

  DISALLOW_COPY_AND_ASSIGN(UploadDataPipeElementReader);
};

}  // namespace

URLLoaderImpl::URLLoaderImpl(NetworkContext* context,
                             InterfaceRequest<URLLoader> request)
    : context_(context),
      response_body_buffer_size_(0),
      auto_follow_redirects_(true),
      connected_(true),
      binding_(this, request.Pass()),
      weak_ptr_factory_(this) {
  binding_.set_error_handler(this);
}

URLLoaderImpl::~URLLoaderImpl() {
}

void URLLoaderImpl::Start(URLRequestPtr request,
                          const Callback<void(URLResponsePtr)>& callback) {
  if (url_request_) {
    SendError(net::ERR_UNEXPECTED, callback);
    return;
  }

  if (!request) {
    SendError(net::ERR_INVALID_ARGUMENT, callback);
    return;
  }

  url_request_ = context_->url_request_context()->CreateRequest(
      GURL(request->url), net::DEFAULT_PRIORITY, this, nullptr);
  url_request_->set_method(request->method);
  if (request->headers) {
    net::HttpRequestHeaders headers;
    for (size_t i = 0; i < request->headers.size(); ++i)
      headers.AddHeaderFromString(request->headers[i].To<base::StringPiece>());
    url_request_->SetExtraRequestHeaders(headers);
  }
  if (request->body) {
    ScopedVector<net::UploadElementReader> element_readers;
    for (size_t i = 0; i < request->body.size(); ++i) {
      element_readers.push_back(
          new UploadDataPipeElementReader(request->body[i].Pass()));
    }
    url_request_->set_upload(make_scoped_ptr<net::UploadDataStream>(
        new net::ElementsUploadDataStream(element_readers.Pass(), 0)));
  }
  if (request->bypass_cache)
    url_request_->SetLoadFlags(net::LOAD_BYPASS_CACHE);

  callback_ = callback;
  response_body_buffer_size_ = request->response_body_buffer_size;
  auto_follow_redirects_ = request->auto_follow_redirects;

  url_request_->Start();
}

void URLLoaderImpl::FollowRedirect(
    const Callback<void(URLResponsePtr)>& callback) {
  if (!url_request_) {
    SendError(net::ERR_UNEXPECTED, callback);
    return;
  }

  if (auto_follow_redirects_) {
    DLOG(ERROR) << "Spurious call to FollowRedirect";
    SendError(net::ERR_UNEXPECTED, callback);
    return;
  }

  // TODO(darin): Verify that it makes sense to call FollowDeferredRedirect.
  url_request_->FollowDeferredRedirect();
}

void URLLoaderImpl::QueryStatus(
    const Callback<void(URLLoaderStatusPtr)>& callback) {
  URLLoaderStatusPtr status(URLLoaderStatus::New());
  if (url_request_) {
    status->is_loading = url_request_->is_pending();
    if (!url_request_->status().is_success())
      status->error = MakeNetworkError(url_request_->status().error());
  } else {
    status->is_loading = false;
  }
  // TODO(darin): Populate more status fields.
  callback.Run(status.Pass());
}

void URLLoaderImpl::OnConnectionError() {
  connected_ = false;
  DeleteIfNeeded();
}

void URLLoaderImpl::OnReceivedRedirect(net::URLRequest* url_request,
                                       const net::RedirectInfo& redirect_info,
                                       bool* defer_redirect) {
  DCHECK(url_request == url_request_.get());
  DCHECK(url_request->status().is_success());

  if (auto_follow_redirects_)
    return;

  // Send the redirect response to the client, allowing them to inspect it and
  // optionally follow the redirect.
  *defer_redirect = true;

  URLResponsePtr response = MakeURLResponse(url_request);
  response->redirect_method = redirect_info.new_method;
  response->redirect_url = String::From(redirect_info.new_url);

  SendResponse(response.Pass());

  DeleteIfNeeded();
}

void URLLoaderImpl::OnResponseStarted(net::URLRequest* url_request) {
  DCHECK(url_request == url_request_.get());

  if (!url_request->status().is_success()) {
    SendError(url_request->status().error(), callback_);
    callback_ = Callback<void(URLResponsePtr)>();
    DeleteIfNeeded();
    return;
  }

  // TODO(darin): Add support for optional MIME sniffing.

  DataPipe data_pipe;
  // TODO(darin): Honor given buffer size.

  URLResponsePtr response = MakeURLResponse(url_request);
  response->body = data_pipe.consumer_handle.Pass();
  response_body_stream_ = data_pipe.producer_handle.Pass();
  ListenForPeerClosed();

  SendResponse(response.Pass());

  // Start reading...
  ReadMore();
}

void URLLoaderImpl::OnReadCompleted(net::URLRequest* url_request,
                                    int bytes_read) {
  DCHECK(url_request == url_request_.get());

  if (url_request->status().is_success()) {
    DidRead(static_cast<uint32_t>(bytes_read), false);
  } else {
    handle_watcher_.Stop();
    pending_write_ = nullptr;  // This closes the data pipe.
    DeleteIfNeeded();
    return;
  }
}

void URLLoaderImpl::SendError(
    int error_code,
    const Callback<void(URLResponsePtr)>& callback) {
  URLResponsePtr response(URLResponse::New());
  if (url_request_)
    response->url = String::From(url_request_->url());
  response->error = MakeNetworkError(error_code);
  callback.Run(response.Pass());
}

void URLLoaderImpl::SendResponse(URLResponsePtr response) {
  Callback<void(URLResponsePtr)> callback;
  std::swap(callback_, callback);
  callback.Run(response.Pass());
}

void URLLoaderImpl::OnResponseBodyStreamReady(MojoResult result) {
  // TODO(darin): Handle a bad |result| value.

  // Continue watching the handle in case the peer is closed.
  ListenForPeerClosed();
  ReadMore();
}

void URLLoaderImpl::OnResponseBodyStreamClosed(MojoResult result) {
  response_body_stream_.reset();
  pending_write_ = nullptr;
  DeleteIfNeeded();
}

void URLLoaderImpl::ReadMore() {
  DCHECK(!pending_write_.get());

  uint32_t num_bytes;
  MojoResult result = NetToMojoPendingBuffer::BeginWrite(
      &response_body_stream_, &pending_write_, &num_bytes);

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // The pipe is full. We need to wait for it to have more space.
    handle_watcher_.Start(response_body_stream_.get(),
                          MOJO_HANDLE_SIGNAL_WRITABLE, MOJO_DEADLINE_INDEFINITE,
                          base::Bind(&URLLoaderImpl::OnResponseBodyStreamReady,
                                     base::Unretained(this)));
    return;
  } else if (result != MOJO_RESULT_OK) {
    // The response body stream is in a bad state. Bail.
    // TODO(darin): How should this be communicated to our client?
    handle_watcher_.Stop();
    response_body_stream_.reset();
    DeleteIfNeeded();
    return;
  }
  CHECK_GT(static_cast<uint32_t>(std::numeric_limits<int>::max()), num_bytes);

  scoped_refptr<net::IOBuffer> buf(new NetToMojoIOBuffer(pending_write_.get()));

  int bytes_read;
  url_request_->Read(buf.get(), static_cast<int>(num_bytes), &bytes_read);
  if (url_request_->status().is_io_pending()) {
    // Wait for OnReadCompleted.
  } else if (url_request_->status().is_success() && bytes_read > 0) {
    DidRead(static_cast<uint32_t>(bytes_read), true);
  } else {
    handle_watcher_.Stop();
    pending_write_->Complete(0);
    pending_write_ = nullptr;  // This closes the data pipe.
    DeleteIfNeeded();
    return;
  }
}

void URLLoaderImpl::DidRead(uint32_t num_bytes, bool completed_synchronously) {
  DCHECK(url_request_->status().is_success());

  response_body_stream_ = pending_write_->Complete(num_bytes);
  pending_write_ = nullptr;

  if (completed_synchronously) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&URLLoaderImpl::ReadMore, weak_ptr_factory_.GetWeakPtr()));
  } else {
    ReadMore();
  }
}

void URLLoaderImpl::DeleteIfNeeded() {
  bool has_data_pipe = pending_write_.get() || response_body_stream_.is_valid();
  if (!connected_ && !has_data_pipe)
    delete this;
}

void URLLoaderImpl::ListenForPeerClosed() {
  handle_watcher_.Start(response_body_stream_.get(),
                        MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                        MOJO_DEADLINE_INDEFINITE,
                        base::Bind(&URLLoaderImpl::OnResponseBodyStreamClosed,
                                   base::Unretained(this)));
}

}  // namespace mojo
