// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol_http_request.h"

#include "base/threading/thread.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace {

const int kBufferSize = 16 * 1024;

}  // namespace

ProtocolHttpRequest::ProtocolHttpRequest(
    Profile* profile,
    const std::string& url,
    const Callback& callback)
    : request_context_(profile->GetRequestContext()),
      url_(url),
      callback_(callback),
      result_(net::OK) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!url_.is_valid()) {
    data_ = "Invalid URL: " + url_.possibly_invalid_spec();
    result_ = net::ERR_FAILED;
    RespondOnUIThread();
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ProtocolHttpRequest::Start, base::Unretained(this)));
}

ProtocolHttpRequest::~ProtocolHttpRequest() {
}

void ProtocolHttpRequest::Start() {
  request_ = new net::URLRequest(url_, this,
      request_context_->GetURLRequestContext());
  io_buffer_ = new net::IOBuffer(kBufferSize);
  request_->Start();
}

void ProtocolHttpRequest::OnResponseStarted(net::URLRequest* request) {
  if (!request->status().is_success()) {
    data_ = "HTTP 404";
    result_ = net::ERR_FAILED;
  }
  int bytes_read = 0;
  if (request->status().is_success())
    request->Read(io_buffer_.get(), kBufferSize, &bytes_read);
  OnReadCompleted(request, bytes_read);
}

void ProtocolHttpRequest::OnReadCompleted(net::URLRequest* request,
                                          int bytes_read) {
  do {
    if (!request->status().is_success() || bytes_read <= 0)
      break;
    data_ += std::string(io_buffer_->data(), bytes_read);
  } while (request->Read(io_buffer_, kBufferSize, &bytes_read));

  if (!request->status().is_io_pending()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ProtocolHttpRequest::RespondOnUIThread,
                   base::Unretained(this)));
    delete request;
  }
}

void ProtocolHttpRequest::RespondOnUIThread() {
  callback_.Run(result_, data_);
  delete this;
}
