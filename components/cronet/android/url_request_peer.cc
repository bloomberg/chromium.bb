// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url_request_peer.h"

#include "base/strings/string_number_conversions.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"

static const size_t kBufferSizeIncrement = 8192;

// Fragment automatically inserted in the User-Agent header to indicate
// that the request is coming from this network stack.
static const char kUserAgentFragment[] = "; ChromiumJNI/";

URLRequestPeer::URLRequestPeer(URLRequestContextPeer* context,
                               URLRequestPeerDelegate* delegate,
                               GURL url,
                               net::RequestPriority priority)
    : method_("GET"),
      url_request_(NULL),
      read_buffer_(new net::GrowableIOBuffer()),
      bytes_read_(0),
      total_bytes_read_(0),
      error_code_(0),
      http_status_code_(0),
      canceled_(false),
      expected_size_(0),
      streaming_upload_(false) {
  context_ = context;
  delegate_ = delegate;
  url_ = url;
  priority_ = priority;
}

URLRequestPeer::~URLRequestPeer() { CHECK(url_request_ == NULL); }

void URLRequestPeer::SetMethod(const std::string& method) { method_ = method; }

void URLRequestPeer::AddHeader(const std::string& name,
                               const std::string& value) {
  headers_.SetHeader(name, value);
}

void URLRequestPeer::SetPostContent(const char* bytes, int bytes_len) {
  if (!upload_data_stream_) {
    upload_data_stream_.reset(
        new net::UploadDataStream(net::UploadDataStream::CHUNKED, 0));
  }
  upload_data_stream_->AppendChunk(bytes, bytes_len, true /* is_last_chunk */);
}

void URLRequestPeer::EnableStreamingUpload() { streaming_upload_ = true; }

void URLRequestPeer::AppendChunk(const char* bytes,
                                 int bytes_len,
                                 bool is_last_chunk) {
  VLOG(context_->logging_level()) << "AppendChunk, len: " << bytes_len
                                  << ", last: " << is_last_chunk;

  context_->GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestPeer::OnAppendChunkWrapper,
                 this,
                 bytes,
                 bytes_len,
                 is_last_chunk));
}

void URLRequestPeer::Start() {
  context_->GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestPeer::OnInitiateConnectionWrapper, this));
}

// static
void URLRequestPeer::OnAppendChunkWrapper(URLRequestPeer* self,
                                          const char* bytes,
                                          int bytes_len,
                                          bool is_last_chunk) {
  self->OnAppendChunk(bytes, bytes_len, is_last_chunk);
}

void URLRequestPeer::OnAppendChunk(const char* bytes,
                                   int bytes_len,
                                   bool is_last_chunk) {
  if (url_request_ != NULL) {
    url_request_->AppendChunkToUpload(bytes, bytes_len, is_last_chunk);
    delegate_->OnAppendChunkCompleted(this);
  }
}

// static
void URLRequestPeer::OnInitiateConnectionWrapper(URLRequestPeer* self) {
  self->OnInitiateConnection();
}

void URLRequestPeer::OnInitiateConnection() {
  if (canceled_) {
    return;
  }

  VLOG(context_->logging_level())
      << "Starting chromium request: " << url_.possibly_invalid_spec().c_str()
      << " priority: " << RequestPriorityToString(priority_);
  url_request_ = new net::URLRequest(
      url_, net::DEFAULT_PRIORITY, this, context_->GetURLRequestContext());
  url_request_->SetLoadFlags(net::LOAD_DISABLE_CACHE |
                             net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES);
  url_request_->set_method(method_);
  url_request_->SetExtraRequestHeaders(headers_);
  std::string user_agent;
  if (headers_.HasHeader(net::HttpRequestHeaders::kUserAgent)) {
    headers_.GetHeader(net::HttpRequestHeaders::kUserAgent, &user_agent);
  } else {
    user_agent = context_->GetUserAgent(url_);
  }
  size_t pos = user_agent.find(')');
  if (pos != std::string::npos) {
    user_agent.insert(pos, context_->version());
    user_agent.insert(pos, kUserAgentFragment);
  }
  url_request_->SetExtraRequestHeaderByName(
      net::HttpRequestHeaders::kUserAgent, user_agent, true /* override */);

  VLOG(context_->logging_level()) << "User agent: " << user_agent;

  if (upload_data_stream_) {
    url_request_->set_upload(make_scoped_ptr(upload_data_stream_.release()));
  } else if (streaming_upload_) {
    url_request_->EnableChunkedUpload();
  }

  url_request_->SetPriority(priority_);

  url_request_->Start();
}

void URLRequestPeer::Cancel() {
  if (canceled_) {
    return;
  }

  canceled_ = true;

  context_->GetNetworkTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&URLRequestPeer::OnCancelRequestWrapper, this));
}

// static
void URLRequestPeer::OnCancelRequestWrapper(URLRequestPeer* self) {
  self->OnCancelRequest();
}

void URLRequestPeer::OnCancelRequest() {
  VLOG(context_->logging_level())
      << "Canceling chromium request: " << url_.possibly_invalid_spec();

  if (url_request_ != NULL) {
    url_request_->Cancel();
  }

  OnRequestCanceled();
}

void URLRequestPeer::Destroy() {
  context_->GetNetworkTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&URLRequestPeer::OnDestroyRequest, this));
}

// static
void URLRequestPeer::OnDestroyRequest(URLRequestPeer* self) {
  VLOG(self->context_->logging_level())
      << "Destroying chromium request: " << self->url_.possibly_invalid_spec();
  delete self;
}

void URLRequestPeer::OnResponseStarted(net::URLRequest* request) {
  if (request->status().status() != net::URLRequestStatus::SUCCESS) {
    OnRequestFailed();
    return;
  }

  http_status_code_ = request->GetResponseCode();
  VLOG(context_->logging_level())
      << "Response started with status: " << http_status_code_;

  request->GetResponseHeaderByName("Content-Type", &content_type_);
  expected_size_ = request->GetExpectedContentSize();
  delegate_->OnResponseStarted(this);

  Read();
}

// Reads all available data or starts an asynchronous read.
void URLRequestPeer::Read() {
  while (true) {
    if (read_buffer_->RemainingCapacity() == 0) {
      int new_capacity = read_buffer_->capacity() + kBufferSizeIncrement;
      read_buffer_->SetCapacity(new_capacity);
    }

    int bytes_read;
    if (url_request_->Read(
            read_buffer_, read_buffer_->RemainingCapacity(), &bytes_read)) {
      if (bytes_read == 0) {
        OnRequestSucceeded();
        break;
      }

      VLOG(context_->logging_level()) << "Synchronously read: " << bytes_read
                                      << " bytes";
      OnBytesRead(bytes_read);
    } else if (url_request_->status().status() ==
               net::URLRequestStatus::IO_PENDING) {
      if (bytes_read_ != 0) {
        VLOG(context_->logging_level()) << "Flushing buffer: " << bytes_read_
                                        << " bytes";

        delegate_->OnBytesRead(this);
        read_buffer_->set_offset(0);
        bytes_read_ = 0;
      }
      VLOG(context_->logging_level()) << "Started async read";
      break;
    } else {
      OnRequestFailed();
      break;
    }
  }
}

void URLRequestPeer::OnReadCompleted(net::URLRequest* request, int bytes_read) {
  VLOG(context_->logging_level()) << "Asynchronously read: " << bytes_read
                                  << " bytes";
  if (bytes_read < 0) {
    OnRequestFailed();
    return;
  } else if (bytes_read == 0) {
    OnRequestSucceeded();
    return;
  }

  OnBytesRead(bytes_read);
  Read();
}

void URLRequestPeer::OnBytesRead(int bytes_read) {
  read_buffer_->set_offset(read_buffer_->offset() + bytes_read);
  bytes_read_ += bytes_read;
  total_bytes_read_ += bytes_read;
}

void URLRequestPeer::OnRequestSucceeded() {
  if (canceled_) {
    return;
  }

  VLOG(context_->logging_level())
      << "Request completed with HTTP status: " << http_status_code_
      << ". Total bytes read: " << total_bytes_read_;

  OnRequestCompleted();
}

void URLRequestPeer::OnRequestFailed() {
  if (canceled_) {
    return;
  }

  error_code_ = url_request_->status().error();
  VLOG(context_->logging_level())
      << "Request failed with status: " << url_request_->status().status()
      << " and error: " << net::ErrorToString(error_code_);
  OnRequestCompleted();
}

void URLRequestPeer::OnRequestCanceled() { OnRequestCompleted(); }

void URLRequestPeer::OnRequestCompleted() {
  VLOG(context_->logging_level())
      << "Completed: " << url_.possibly_invalid_spec();
  if (url_request_ != NULL) {
    delete url_request_;
    url_request_ = NULL;
  }

  delegate_->OnBytesRead(this);
  delegate_->OnRequestFinished(this);
}

unsigned char* URLRequestPeer::Data() const {
  return reinterpret_cast<unsigned char*>(read_buffer_->StartOfBuffer());
}
