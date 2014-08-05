// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url_request_peer.h"

#include "base/strings/string_number_conversions.h"
#include "components/cronet/android/url_request_context_peer.h"
#include "components/cronet/android/wrapped_channel_upload_element_reader.h"
#include "net/base/load_flags.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_status_code.h"

namespace cronet {

static const size_t kBufferSizeIncrement = 8192;

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
      expected_size_(0)  {
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

void URLRequestPeer::SetUploadContent(const char* bytes, int bytes_len) {
  std::vector<char> data(bytes, bytes + bytes_len);
  scoped_ptr<net::UploadElementReader> reader(
      new net::UploadOwnedBytesElementReader(&data));
  upload_data_stream_.reset(net::UploadDataStream::CreateWithReader(
      reader.Pass(), 0));
}

void URLRequestPeer::SetUploadChannel(
    JNIEnv* env, jobject channel, int64 content_length) {
  scoped_ptr<net::UploadElementReader> reader(
      new WrappedChannelElementReader(env, channel, content_length));
  upload_data_stream_.reset(net::UploadDataStream::CreateWithReader(
      reader.Pass(), 0));
}

std::string URLRequestPeer::GetHeader(const std::string &name) const {
  std::string value;
  if (url_request_ != NULL) {
    url_request_->GetResponseHeaderByName(name, &value);
  }
  return value;
}

net::HttpResponseHeaders* URLRequestPeer::GetResponseHeaders() const {
  if (url_request_ == NULL) {
    return NULL;
  }
  return url_request_->response_headers();
}

void URLRequestPeer::Start() {
  context_->GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestPeer::OnInitiateConnection,
                 base::Unretained(this)));
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
  if (!headers_.HasHeader(net::HttpRequestHeaders::kUserAgent)) {
    std::string user_agent;
    user_agent = context_->GetUserAgent(url_);
    url_request_->SetExtraRequestHeaderByName(
      net::HttpRequestHeaders::kUserAgent, user_agent, true /* override */);
  }

  if (upload_data_stream_)
    url_request_->set_upload(upload_data_stream_.Pass());

  url_request_->SetPriority(priority_);

  url_request_->Start();
}

void URLRequestPeer::Cancel() {
  if (canceled_) {
    return;
  }

  canceled_ = true;

  context_->GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestPeer::OnCancelRequest, base::Unretained(this)));
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

}  // namespace cronet
