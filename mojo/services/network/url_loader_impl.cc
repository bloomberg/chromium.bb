// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/url_loader_impl.h"

#include "base/message_loop/message_loop.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/services/network/network_context.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"

namespace mojo {
namespace {

const uint32_t kMaxReadSize = 64 * 1024;

// Generates an URLResponsePtr from the response state of a net::URLRequest.
URLResponsePtr MakeURLResponse(const net::URLRequest* url_request) {
  URLResponsePtr response(URLResponse::New());
  response->url = String::From(url_request->url());

  const net::HttpResponseHeaders* headers = url_request->response_headers();
  if (headers) {
    response->status_code = headers->response_code();
    response->status_line = headers->GetStatusLine();

    std::vector<String> header_lines;
    void* iter = NULL;
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

NetworkErrorPtr MakeNetworkError(int error_code) {
  NetworkErrorPtr error = NetworkError::New();
  error->code = error_code;
  error->description = net::ErrorToString(error_code);
  return error.Pass();
}

}  // namespace

// Keeps track of a pending two-phase write on a DataPipeProducerHandle.
class URLLoaderImpl::PendingWriteToDataPipe :
    public base::RefCountedThreadSafe<PendingWriteToDataPipe> {
 public:
  explicit PendingWriteToDataPipe(ScopedDataPipeProducerHandle handle)
      : handle_(handle.Pass()),
        buffer_(NULL) {
  }

  MojoResult BeginWrite(uint32_t* num_bytes) {
    MojoResult result = BeginWriteDataRaw(handle_.get(), &buffer_, num_bytes,
                                          MOJO_WRITE_DATA_FLAG_NONE);
    if (*num_bytes > kMaxReadSize)
      *num_bytes = kMaxReadSize;

    return result;
  }

  ScopedDataPipeProducerHandle Complete(uint32_t num_bytes) {
    EndWriteDataRaw(handle_.get(), num_bytes);
    buffer_ = NULL;
    return handle_.Pass();
  }

  char* buffer() { return static_cast<char*>(buffer_); }

 private:
  friend class base::RefCountedThreadSafe<PendingWriteToDataPipe>;

  ~PendingWriteToDataPipe() {
    if (handle_.is_valid())
      EndWriteDataRaw(handle_.get(), 0);
  }

  ScopedDataPipeProducerHandle handle_;
  void* buffer_;

  DISALLOW_COPY_AND_ASSIGN(PendingWriteToDataPipe);
};

// Takes ownership of a pending two-phase write on a DataPipeProducerHandle,
// and makes its buffer available as a net::IOBuffer.
class URLLoaderImpl::DependentIOBuffer : public net::WrappedIOBuffer {
 public:
  DependentIOBuffer(PendingWriteToDataPipe* pending_write)
      : net::WrappedIOBuffer(pending_write->buffer()),
        pending_write_(pending_write) {
  }
 private:
  virtual ~DependentIOBuffer() {}
  scoped_refptr<PendingWriteToDataPipe> pending_write_;
};

URLLoaderImpl::URLLoaderImpl(NetworkContext* context)
    : context_(context),
      response_body_buffer_size_(0),
      auto_follow_redirects_(true),
      weak_ptr_factory_(this) {
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

  url_request_.reset(
      new net::URLRequest(GURL(request->url),
                          net::DEFAULT_PRIORITY,
                          this,
                          context_->url_request_context()));
  url_request_->set_method(request->method);
  if (request->headers) {
    net::HttpRequestHeaders headers;
    for (size_t i = 0; i < request->headers.size(); ++i)
      headers.AddHeaderFromString(request->headers[i].To<base::StringPiece>());
    url_request_->SetExtraRequestHeaders(headers);
  }
  if (request->bypass_cache)
    url_request_->SetLoadFlags(net::LOAD_BYPASS_CACHE);
  // TODO(darin): Handle request body.

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

void URLLoaderImpl::OnReceivedRedirect(net::URLRequest* url_request,
                                       const GURL& new_url,
                                       bool* defer_redirect) {
  DCHECK(url_request == url_request_.get());
  DCHECK(url_request->status().is_success());

  if (auto_follow_redirects_)
    return;

  // Send the redirect response to the client, allowing them to inspect it and
  // optionally follow the redirect.
  *defer_redirect = true;

  URLResponsePtr response = MakeURLResponse(url_request);
  response->redirect_method =
      net::URLRequest::ComputeMethodForRedirect(url_request->method(),
                                                response->status_code);
  response->redirect_url = String::From(new_url);

  SendResponse(response.Pass());
}

void URLLoaderImpl::OnResponseStarted(net::URLRequest* url_request) {
  DCHECK(url_request == url_request_.get());

  if (!url_request->status().is_success()) {
    SendError(url_request->status().error(), callback_);
    callback_ = Callback<void(URLResponsePtr)>();
    return;
  }

  // TODO(darin): Add support for optional MIME sniffing.

  DataPipe data_pipe;
  // TODO(darin): Honor given buffer size.

  URLResponsePtr response = MakeURLResponse(url_request);
  response->body = data_pipe.consumer_handle.Pass();
  response_body_stream_ = data_pipe.producer_handle.Pass();

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
    pending_write_ = NULL;  // This closes the data pipe.
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
  ReadMore();
}

void URLLoaderImpl::WaitToReadMore() {
  handle_watcher_.Start(response_body_stream_.get(),
                        MOJO_HANDLE_SIGNAL_WRITABLE,
                        MOJO_DEADLINE_INDEFINITE,
                        base::Bind(&URLLoaderImpl::OnResponseBodyStreamReady,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void URLLoaderImpl::ReadMore() {
  DCHECK(!pending_write_);

  pending_write_ = new PendingWriteToDataPipe(response_body_stream_.Pass());

  uint32_t num_bytes;
  MojoResult result = pending_write_->BeginWrite(&num_bytes);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // The pipe is full. We need to wait for it to have more space.
    response_body_stream_ = pending_write_->Complete(num_bytes);
    pending_write_ = NULL;
    WaitToReadMore();
    return;
  }
  if (result != MOJO_RESULT_OK) {
    // The response body stream is in a bad state. Bail.
    // TODO(darin): How should this be communicated to our client?
    return;
  }
  CHECK_GT(static_cast<uint32_t>(std::numeric_limits<int>::max()), num_bytes);

  scoped_refptr<net::IOBuffer> buf = new DependentIOBuffer(pending_write_);

  int bytes_read;
  url_request_->Read(buf, static_cast<int>(num_bytes), &bytes_read);

  // Drop our reference to the buffer.
  buf = NULL;

  if (url_request_->status().is_io_pending()) {
    // Wait for OnReadCompleted.
  } else if (url_request_->status().is_success() && bytes_read > 0) {
    DidRead(static_cast<uint32_t>(bytes_read), true);
  } else {
    pending_write_->Complete(0);
    pending_write_ = NULL;  // This closes the data pipe.
  }
}

void URLLoaderImpl::DidRead(uint32_t num_bytes, bool completed_synchronously) {
  DCHECK(url_request_->status().is_success());

  response_body_stream_ = pending_write_->Complete(num_bytes);
  pending_write_ = NULL;

  if (completed_synchronously) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&URLLoaderImpl::ReadMore, weak_ptr_factory_.GetWeakPtr()));
  } else {
    ReadMore();
  }
}

}  // namespace mojo
