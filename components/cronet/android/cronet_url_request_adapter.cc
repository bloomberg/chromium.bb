// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cronet_url_request_adapter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "components/cronet/android/cronet_url_request_context_adapter.h"
#include "components/cronet/android/wrapped_channel_upload_element_reader.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"

namespace cronet {

static const int kReadBufferSize = 32768;

CronetURLRequestAdapter::CronetURLRequestAdapter(
    CronetURLRequestContextAdapter* context,
    scoped_ptr<CronetURLRequestAdapterDelegate> delegate,
    const GURL& url,
    net::RequestPriority priority)
    : context_(context),
      delegate_(delegate.Pass()),
      initial_url_(url),
      initial_priority_(priority),
      initial_method_("GET"),
      load_flags_(context->default_load_flags()) {
}

CronetURLRequestAdapter::~CronetURLRequestAdapter() {
  DCHECK(IsOnNetworkThread());
}

void CronetURLRequestAdapter::AddRequestHeader(const std::string& name,
                                               const std::string& value) {
  DCHECK(!IsOnNetworkThread());
  initial_request_headers_.SetHeader(name, value);
}

void CronetURLRequestAdapter::DisableCache() {
  DCHECK(!IsOnNetworkThread());
  load_flags_ |= net::LOAD_DISABLE_CACHE;
}

void CronetURLRequestAdapter::PostTaskToNetworkThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(!IsOnNetworkThread());
  context_->PostTaskToNetworkThread(from_here, task);
}

bool CronetURLRequestAdapter::IsOnNetworkThread() const {
  return context_->IsOnNetworkThread();
}

void CronetURLRequestAdapter::SetUpload(
    scoped_ptr<net::UploadDataStream> upload) {
  DCHECK(!IsOnNetworkThread());
  DCHECK(!upload_);
  upload_ = upload.Pass();
}

void CronetURLRequestAdapter::Start() {
  DCHECK(IsOnNetworkThread());
  VLOG(1) << "Starting chromium request: "
          << initial_url_.possibly_invalid_spec().c_str()
          << " priority: " << RequestPriorityToString(initial_priority_);
  url_request_ = context_->GetURLRequestContext()->CreateRequest(
      initial_url_, net::DEFAULT_PRIORITY, this, NULL);
  url_request_->SetLoadFlags(load_flags_);
  url_request_->set_method(initial_method_);
  url_request_->SetExtraRequestHeaders(initial_request_headers_);
  url_request_->SetPriority(initial_priority_);
  if (upload_)
    url_request_->set_upload(upload_.Pass());
  url_request_->Start();
}

void CronetURLRequestAdapter::FollowDeferredRedirect() {
  DCHECK(IsOnNetworkThread());

  url_request_->FollowDeferredRedirect();
}

void CronetURLRequestAdapter::ReadData() {
  DCHECK(IsOnNetworkThread());
  if (!read_buffer_.get())
    read_buffer_ = new net::IOBufferWithSize(kReadBufferSize);

  int bytes_read = 0;
  url_request_->Read(read_buffer_.get(), read_buffer_->size(), &bytes_read);
  // If IO is pending, wait for the URLRequest to call OnReadCompleted.
  if (url_request_->status().is_io_pending())
    return;

  OnReadCompleted(url_request_.get(), bytes_read);
}

void CronetURLRequestAdapter::Destroy() {
  DCHECK(IsOnNetworkThread());
  delete this;
}

const net::HttpResponseHeaders*
CronetURLRequestAdapter::GetResponseHeaders() const {
  DCHECK(IsOnNetworkThread());
  return url_request_->response_headers();
}

const std::string& CronetURLRequestAdapter::GetNegotiatedProtocol() const {
  DCHECK(IsOnNetworkThread());
  return url_request_->response_info().npn_negotiated_protocol;
}

bool CronetURLRequestAdapter::GetWasCached() const {
  DCHECK(IsOnNetworkThread());
  return url_request_->response_info().was_cached;
}

int64 CronetURLRequestAdapter::GetTotalReceivedBytes() const {
  DCHECK(IsOnNetworkThread());
  return url_request_->GetTotalReceivedBytes();
}

// net::URLRequest::Delegate overrides (called on network thread).

void CronetURLRequestAdapter::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  DCHECK(IsOnNetworkThread());
  DCHECK(request->status().is_success());
  delegate_->OnRedirect(redirect_info.new_url, redirect_info.status_code);
  *defer_redirect = true;
}

void CronetURLRequestAdapter::OnResponseStarted(net::URLRequest* request) {
  DCHECK(IsOnNetworkThread());
  if (MaybeReportError(request))
    return;
  delegate_->OnResponseStarted(request->GetResponseCode());
}

void CronetURLRequestAdapter::OnReadCompleted(net::URLRequest* request,
                                              int bytes_read) {
  DCHECK(IsOnNetworkThread());
  if (MaybeReportError(request))
    return;
  if (bytes_read != 0) {
    delegate_->OnBytesRead(
        reinterpret_cast<unsigned char*>(read_buffer_->data()), bytes_read);
  } else {
    delegate_->OnRequestFinished();
  }
}

bool CronetURLRequestAdapter::MaybeReportError(net::URLRequest* request) const {
  DCHECK_NE(net::URLRequestStatus::IO_PENDING, url_request_->status().status());
  DCHECK_EQ(request, url_request_);
  if (url_request_->status().is_success())
    return false;
  VLOG(1) << "Error " << url_request_->status().error()
          << " on chromium request: " << initial_url_.possibly_invalid_spec();
  delegate_->OnError(url_request_->status().error());
  return true;
}

}  // namespace cronet
