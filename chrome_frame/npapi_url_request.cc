// Copyright 2009, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome_frame/npapi_url_request.h"

#include "base/string_util.h"
#include "chrome_frame/np_browser_functions.h"
#include "net/base/net_errors.h"

int NPAPIUrlRequest::instance_count_ = 0;

NPAPIUrlRequest::NPAPIUrlRequest(NPP instance)
    : ref_count_(0), instance_(instance), stream_(NULL),
      pending_read_size_(0),
      status_(URLRequestStatus::FAILED, net::ERR_FAILED),
      thread_(PlatformThread::CurrentId()) {
  DLOG(INFO) << "Created request. Count: " << ++instance_count_;
}

NPAPIUrlRequest::~NPAPIUrlRequest() {
  DLOG(INFO) << "Deleted request. Count: " << --instance_count_;
}

// NPAPIUrlRequest member defines.
bool NPAPIUrlRequest::Start() {
  NPError result = NPERR_GENERIC_ERROR;
  DLOG(INFO) << "Starting URL request: " << url();
  if (LowerCaseEqualsASCII(method(), "get")) {
    // TODO(joshia): if we have extra headers for HTTP GET, then implement
    // it using XHR
    result = npapi::GetURLNotify(instance_, url().c_str(), NULL, this);
  } else if (LowerCaseEqualsASCII(method(), "post")) {
    result = npapi::PostURLNotify(instance_, url().c_str(), NULL,
        extra_headers().length(), extra_headers().c_str(), false, this);
  } else {
    NOTREACHED() << "PluginUrlRequest only supports 'GET'/'POST'";
  }

  if (NPERR_NO_ERROR == result) {
    request_handler()->AddRequest(this);
  } else {
    int os_error = net::ERR_FAILED;
    switch (result) {
      case NPERR_INVALID_URL:
        os_error = net::ERR_INVALID_URL;
        break;
      default:
        break;
    }

    OnResponseEnd(URLRequestStatus(URLRequestStatus::FAILED, os_error));
    return false;
  }

  return true;
}

void NPAPIUrlRequest::Stop() {
  DLOG(INFO) << "Finished request: Url - " << url() << " result: "
      << static_cast<int>(status_.status());
  if (stream_) {
    npapi::DestroyStream(instance_, stream_, NPRES_USER_BREAK);
    stream_ = NULL;
  }

  request_handler()->RemoveRequest(this);
  if (!status_.is_io_pending())
    OnResponseEnd(status_);
}

bool NPAPIUrlRequest::Read(int bytes_to_read) {
  pending_read_size_ = bytes_to_read;
  return true;
}

bool NPAPIUrlRequest::OnStreamCreated(const char* mime_type, NPStream* stream) {
  stream_ = stream;
  status_.set_status(URLRequestStatus::IO_PENDING);
  // TODO(iyengar)
  // Add support for passing persistent cookies and information about any URL
  // redirects to Chrome.
  OnResponseStarted(mime_type, stream->headers, stream->end,
      base::Time::FromTimeT(stream->lastmodified), std::string(),
      std::string(), 0);
  return true;
}

void NPAPIUrlRequest::OnStreamDestroyed(NPReason reason) {
  URLRequestStatus::Status status = URLRequestStatus::FAILED;
  switch (reason) {
    case NPRES_DONE:
      status_.set_status(URLRequestStatus::SUCCESS);
      status_.set_os_error(0);
      break;
    case NPRES_USER_BREAK:
      status_.set_status(URLRequestStatus::CANCELED);
      status_.set_os_error(net::ERR_ABORTED);
      break;
    case NPRES_NETWORK_ERR:
    default:
      status_.set_status(URLRequestStatus::FAILED);
      status_.set_os_error(net::ERR_CONNECTION_CLOSED);
      break;
  }
}

int NPAPIUrlRequest::OnWriteReady() {
  return pending_read_size_;
}

int NPAPIUrlRequest::OnWrite(void* buffer, int len) {
  pending_read_size_ = 0;
  OnReadComplete(buffer, len);
  return len;
}

STDMETHODIMP_(ULONG) NPAPIUrlRequest::AddRef() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);
  ++ref_count_;
  return ref_count_;
}

STDMETHODIMP_(ULONG) NPAPIUrlRequest::Release() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_);
  unsigned long ret = --ref_count_;
  if (!ret)
    delete this;

  return ret;
}

