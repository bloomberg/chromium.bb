// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/resource_loader.h"

#include "base/message_loop.h"
#include "base/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/doomed_resource_handler.h"
#include "content/browser/renderer_host/resource_loader_delegate.h"
#include "content/browser/renderer_host/resource_request_info_impl.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/common/ssl_status_serialization.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"
#include "content/public/common/resource_response.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "webkit/appcache/appcache_interceptor.h"

using base::TimeDelta;
using base::TimeTicks;

namespace content {
namespace {

void PopulateResourceResponse(net::URLRequest* request,
                              ResourceResponse* response) {
  response->head.status = request->status();
  response->head.request_time = request->request_time();
  response->head.response_time = request->response_time();
  response->head.headers = request->response_headers();
  request->GetCharset(&response->head.charset);
  response->head.content_length = request->GetExpectedContentSize();
  request->GetMimeType(&response->head.mime_type);
  net::HttpResponseInfo response_info = request->response_info();
  response->head.was_fetched_via_spdy = response_info.was_fetched_via_spdy;
  response->head.was_npn_negotiated = response_info.was_npn_negotiated;
  response->head.npn_negotiated_protocol =
      response_info.npn_negotiated_protocol;
  response->head.was_fetched_via_proxy = request->was_fetched_via_proxy();
  response->head.socket_address = request->GetSocketAddress();
  appcache::AppCacheInterceptor::GetExtraResponseInfo(
      request,
      &response->head.appcache_id,
      &response->head.appcache_manifest_url);
}

}  // namespace

ResourceLoader::ResourceLoader(scoped_ptr<net::URLRequest> request,
                               scoped_ptr<ResourceHandler> handler,
                               ResourceLoaderDelegate* delegate)
    : deferred_stage_(DEFERRED_NONE),
      request_(request.Pass()),
      handler_(handler.Pass()),
      delegate_(delegate),
      last_upload_position_(0),
      waiting_for_upload_progress_ack_(false),
      called_on_response_started_(false),
      has_started_reading_(false),
      is_paused_(false),
      pause_count_(0),
      paused_read_bytes_(0),
      is_transferring_(false) {
  request_->set_delegate(this);
  handler_->SetController(this);
}

ResourceLoader::~ResourceLoader() {
  if (login_delegate_)
    login_delegate_->OnRequestCancelled();
  if (ssl_client_auth_handler_)
    ssl_client_auth_handler_->OnRequestCancelled();

  // Run ResourceHandler destructor before we tear-down the rest of our state
  // as the ResourceHandler may want to inspect the URLRequest and other state.
  handler_.reset();
}

void ResourceLoader::StartRequest() {
  if (delegate_->HandleExternalProtocol(this, request_->url())) {
    CancelRequestInternal(net::ERR_UNKNOWN_URL_SCHEME, false);
    return;
  }

  // Give the handler a chance to delay the URLRequest from being started.
  bool defer_start = false;
  if (!handler_->OnWillStart(GetRequestInfo()->GetRequestID(), request_->url(),
                             &defer_start)) {
    Cancel();
    return;
  }

  if (defer_start) {
    deferred_stage_ = DEFERRED_START;
  } else {
    StartRequestInternal();
  }
}

void ResourceLoader::CancelRequest(bool from_renderer) {
  CancelRequestInternal(net::ERR_ABORTED, from_renderer);
}

void ResourceLoader::ReportUploadProgress() {
  ResourceRequestInfoImpl* info = GetRequestInfo();

  if (waiting_for_upload_progress_ack_)
    return;  // Send one progress event at a time.

  uint64 size = info->GetUploadSize();
  if (!size)
    return;  // Nothing to upload.

  uint64 position = request_->GetUploadProgress();
  if (position == last_upload_position_)
    return;  // No progress made since last time.

  const uint64 kHalfPercentIncrements = 200;
  const TimeDelta kOneSecond = TimeDelta::FromMilliseconds(1000);

  uint64 amt_since_last = position - last_upload_position_;
  TimeDelta time_since_last = TimeTicks::Now() - last_upload_ticks_;

  bool is_finished = (size == position);
  bool enough_new_progress = (amt_since_last > (size / kHalfPercentIncrements));
  bool too_much_time_passed = time_since_last > kOneSecond;

  if (is_finished || enough_new_progress || too_much_time_passed) {
    if (request_->load_flags() & net::LOAD_ENABLE_UPLOAD_PROGRESS) {
      handler_->OnUploadProgress(info->GetRequestID(), position, size);
      waiting_for_upload_progress_ack_ = true;
    }
    last_upload_ticks_ = TimeTicks::Now();
    last_upload_position_ = position;
  }
}

void ResourceLoader::MarkAsTransferring() {
  is_transferring_ = true;

  // When an URLRequest is transferred to a new RenderViewHost, its
  // ResourceHandler should not receive any notifications because it may depend
  // on the state of the old RVH. We set a ResourceHandler that only allows
  // canceling requests, because on shutdown of the RDH all pending requests
  // are canceled. The RVH of requests that are being transferred may be gone
  // by that time. In CompleteTransfer, the ResoureHandlers are substituted
  // again.
  handler_.reset(new DoomedResourceHandler(handler_.Pass()));
}

void ResourceLoader::CompleteTransfer(scoped_ptr<ResourceHandler> new_handler) {
  DCHECK_EQ(DEFERRED_REDIRECT, deferred_stage_);

  handler_ = new_handler.Pass();
  handler_->SetController(this);
  is_transferring_ = false;

  Resume();
}

ResourceRequestInfoImpl* ResourceLoader::GetRequestInfo() {
  return ResourceRequestInfoImpl::ForRequest(request_.get());
}

void ResourceLoader::ClearLoginDelegate() {
  login_delegate_ = NULL;
}

void ResourceLoader::ClearSSLClientAuthHandler() {
  ssl_client_auth_handler_ = NULL;
}

void ResourceLoader::OnUploadProgressACK() {
  waiting_for_upload_progress_ack_ = false;
}

void ResourceLoader::OnReceivedRedirect(net::URLRequest* unused,
                                        const GURL& new_url,
                                        bool* defer) {
  DCHECK_EQ(request_.get(), unused);

  VLOG(1) << "OnReceivedRedirect: " << request_->url().spec();
  DCHECK(request_->status().is_success());

  ResourceRequestInfoImpl* info = GetRequestInfo();

  if (info->process_type() != PROCESS_TYPE_PLUGIN &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->
          CanRequestURL(info->GetChildID(), new_url)) {
    VLOG(1) << "Denied unauthorized request for "
            << new_url.possibly_invalid_spec();

    // Tell the renderer that this request was disallowed.
    Cancel();
    return;
  }

  delegate_->DidReceiveRedirect(this, new_url);

  if (delegate_->HandleExternalProtocol(this, new_url)) {
    // The request is complete so we can remove it.
    CancelRequestInternal(net::ERR_UNKNOWN_URL_SCHEME, false);
    return;
  }

  scoped_refptr<ResourceResponse> response(new ResourceResponse());
  PopulateResourceResponse(request_.get(), response);

  if (!handler_->OnRequestRedirected(info->GetRequestID(), new_url, response,
                                     defer)) {
    Cancel();
  }

  if (*defer)
    deferred_stage_ = DEFERRED_REDIRECT;
}

void ResourceLoader::OnAuthRequired(net::URLRequest* unused,
                                    net::AuthChallengeInfo* auth_info) {
  DCHECK_EQ(request_.get(), unused);

  if (request_->load_flags() & net::LOAD_DO_NOT_PROMPT_FOR_LOGIN) {
    request_->CancelAuth();
    return;
  }

  if (!delegate_->AcceptAuthRequest(this, auth_info)) {
    request_->CancelAuth();
    return;
  }

  // Create a login dialog on the UI thread to get authentication data, or pull
  // from cache and continue on the IO thread.

  DCHECK(!login_delegate_) <<
      "OnAuthRequired called with login_delegate pending";
  login_delegate_ = delegate_->CreateLoginDelegate(this, auth_info);
  if (!login_delegate_)
    request_->CancelAuth();
}

void ResourceLoader::OnCertificateRequested(
    net::URLRequest* unused,
    net::SSLCertRequestInfo* cert_info) {
  DCHECK_EQ(request_.get(), unused);

  if (!delegate_->AcceptSSLClientCertificateRequest(this, cert_info)) {
    request_->Cancel();
    return;
  }

  if (cert_info->client_certs.empty()) {
    // No need to query the user if there are no certs to choose from.
    request_->ContinueWithCertificate(NULL);
    return;
  }

  DCHECK(!ssl_client_auth_handler_) <<
      "OnCertificateRequested called with ssl_client_auth_handler pending";
  ssl_client_auth_handler_ = new SSLClientAuthHandler(request_.get(),
                                                      cert_info);
  ssl_client_auth_handler_->SelectCertificate();
}

void ResourceLoader::OnSSLCertificateError(net::URLRequest* request,
                                           const net::SSLInfo& ssl_info,
                                           bool fatal) {
  ResourceRequestInfoImpl* info = GetRequestInfo();

  int render_process_id;
  int render_view_id;
  if (!info->GetAssociatedRenderView(&render_process_id, &render_view_id))
    NOTREACHED();

  SSLManager::OnSSLCertificateError(
      AsWeakPtr(),
      info->GetGlobalRequestID(),
      info->GetResourceType(),
      request_->url(),
      render_process_id,
      render_view_id,
      ssl_info,
      fatal);
}

void ResourceLoader::OnResponseStarted(net::URLRequest* unused) {
  DCHECK_EQ(request_.get(), unused);

  VLOG(1) << "OnResponseStarted: " << request_->url().spec();

  if (request_->status().is_success()) {
    if (PauseRequestIfNeeded()) {
      VLOG(1) << "OnResponseStarted pausing: " << request_->url().spec();
      return;
    }

    // We want to send a final upload progress message prior to sending the
    // response complete message even if we're waiting for an ack to to a
    // previous upload progress message.
    waiting_for_upload_progress_ack_ = false;
    ReportUploadProgress();

    if (!CompleteResponseStarted()) {
      Cancel();
    } else {
      // Check if the handler paused the request in their OnResponseStarted.
      if (PauseRequestIfNeeded()) {
        VLOG(1) << "OnResponseStarted pausing2: " << request_->url().spec();
        return;
      }

      StartReading();
    }
  } else {
    ResponseCompleted();
  }
}

void ResourceLoader::OnReadCompleted(net::URLRequest* unused, int bytes_read) {
  DCHECK_EQ(request_.get(), unused);
  VLOG(1) << "OnReadCompleted: \"" << request_->url().spec() << "\""
          << " bytes_read = " << bytes_read;

  // bytes_read == -1 always implies an error, so we want to skip the pause
  // checks and just call ResponseCompleted.
  if (bytes_read == -1) {
    DCHECK(!request_->status().is_success());

    ResponseCompleted();
    return;
  }

  // OnReadCompleted can be called without Read (e.g., for chrome:// URLs).
  // Make sure we know that a read has begun.
  has_started_reading_ = true;

  if (PauseRequestIfNeeded()) {
    paused_read_bytes_ = bytes_read;
    VLOG(1) << "OnReadCompleted pausing: \"" << request_->url().spec() << "\""
            << " bytes_read = " << bytes_read;
    return;
  }

  if (request_->status().is_success() && CompleteRead(&bytes_read)) {
    // The request can be paused if we realize that the renderer is not
    // servicing messages fast enough.
    if (pause_count_ == 0 && ReadMore(&bytes_read) &&
        request_->status().is_success()) {
      if (bytes_read == 0) {
        CompleteRead(&bytes_read);
      } else {
        // Force the next CompleteRead / Read pair to run as a separate task.
        // This avoids a fast, large network request from monopolizing the IO
        // thread and starving other IO operations from running.
        VLOG(1) << "OnReadCompleted postponing: \""
                << request_->url().spec() << "\""
                << " bytes_read = " << bytes_read;

        paused_read_bytes_ = bytes_read;
        is_paused_ = true;

        MessageLoop::current()->PostTask(
            FROM_HERE, base::Bind(&ResourceLoader::ResumeRequest, AsWeakPtr()));
        return;
      }
    }
  }

  if (PauseRequestIfNeeded()) {
    paused_read_bytes_ = bytes_read;
    VLOG(1) << "OnReadCompleted (CompleteRead) pausing: \""
            << request_->url().spec() << "\""
            << " bytes_read = " << bytes_read;
    return;
  }

  // If the status is not IO pending then we've either finished (success) or we
  // had an error.  Either way, we're done!
  if (!request_->status().is_io_pending())
    ResponseCompleted();
}

void ResourceLoader::CancelSSLRequest(const GlobalRequestID& id,
                                      int error,
                                      const net::SSLInfo* ssl_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // The request can be NULL if it was cancelled by the renderer (as the
  // request of the user navigating to a new page from the location bar).
  if (!request_->is_pending())
    return;
  DVLOG(1) << "CancelSSLRequest() url: " << request_->url().spec();

  if (ssl_info) {
    request_->CancelWithSSLError(error, *ssl_info);
  } else {
    request_->CancelWithError(error);
  }
}

void ResourceLoader::ContinueSSLRequest(const GlobalRequestID& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DVLOG(1) << "ContinueSSLRequest() url: " << request_->url().spec();

  request_->ContinueDespiteLastError();
}

void ResourceLoader::Resume() {
  DCHECK(!is_transferring_);

  DeferredStage stage = deferred_stage_;
  deferred_stage_ = DEFERRED_NONE;
  switch (stage) {
    case DEFERRED_NONE:
      NOTREACHED();
      break;
    case DEFERRED_START:
      StartRequestInternal();
      break;
    case DEFERRED_REDIRECT:
      request_->FollowDeferredRedirect();
      break;
    case DEFERRED_RESPONSE:
    case DEFERRED_READ:
      PauseRequest(false);
      break;
    case DEFERRED_FINISH:
      // Delay self-destruction since we don't know how we were reached.
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ResourceLoader::CallDidFinishLoading, AsWeakPtr()));
      break;
  }
}

void ResourceLoader::Cancel() {
  CancelRequest(false);
}

void ResourceLoader::StartRequestInternal() {
  DCHECK(!request_->is_pending());
  request_->Start();

  delegate_->DidStartRequest(this);
}

void ResourceLoader::CancelRequestInternal(int error, bool from_renderer) {
  VLOG(1) << "CancelRequestInternal: " << request_->url().spec();

  ResourceRequestInfoImpl* info = GetRequestInfo();

  // WebKit will send us a cancel for downloads since it no longer handles
  // them.  In this case, ignore the cancel since we handle downloads in the
  // browser.
  if (from_renderer && info->is_download())
    return;

  bool was_pending = request_->is_pending();

  if (login_delegate_) {
    login_delegate_->OnRequestCancelled();
    login_delegate_ = NULL;
  }
  if (ssl_client_auth_handler_) {
    ssl_client_auth_handler_->OnRequestCancelled();
    ssl_client_auth_handler_ = NULL;
  }

  request_->CancelWithError(error);

  if (!was_pending) {
    // If the request isn't in flight, then we won't get an asynchronous
    // notification from the request, so we have to signal ourselves to finish
    // this request.
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&ResourceLoader::ResponseCompleted, AsWeakPtr()));
  }
}

bool ResourceLoader::CompleteResponseStarted() {
  ResourceRequestInfoImpl* info = GetRequestInfo();

  scoped_refptr<ResourceResponse> response(new ResourceResponse());
  PopulateResourceResponse(request_.get(), response);

  if (request_->ssl_info().cert) {
    int cert_id =
        CertStore::GetInstance()->StoreCert(request_->ssl_info().cert,
                                            info->GetChildID());
    response->head.security_info = SerializeSecurityInfo(
        cert_id,
        request_->ssl_info().cert_status,
        request_->ssl_info().security_bits,
        request_->ssl_info().connection_status);
  } else {
    // We should not have any SSL state.
    DCHECK(!request_->ssl_info().cert_status &&
           request_->ssl_info().security_bits == -1 &&
           !request_->ssl_info().connection_status);
  }

  delegate_->DidReceiveResponse(this);
  called_on_response_started_ = true;

  bool defer = false;
  if (!handler_->OnResponseStarted(info->GetRequestID(), response, &defer))
    return false;

  if (defer) {
    deferred_stage_ = DEFERRED_RESPONSE;
    PauseRequest(true);
  }

  return true;
}

void ResourceLoader::StartReading() {
  int bytes_read = 0;
  if (ReadMore(&bytes_read)) {
    OnReadCompleted(request_.get(), bytes_read);
  } else if (!request_->status().is_io_pending()) {
    DCHECK(!is_paused_);
    // If the error is not an IO pending, then we're done reading.
    ResponseCompleted();
  }
}

bool ResourceLoader::ReadMore(int* bytes_read) {
  ResourceRequestInfoImpl* info = GetRequestInfo();
  DCHECK(!is_paused_);

  net::IOBuffer* buf;
  int buf_size;
  if (!handler_->OnWillRead(info->GetRequestID(), &buf, &buf_size, -1))
    return false;

  DCHECK(buf);
  DCHECK(buf_size > 0);

  has_started_reading_ = true;
  return request_->Read(buf, buf_size, bytes_read);
}

bool ResourceLoader::CompleteRead(int* bytes_read) {
  if (!request_.get() || !request_->status().is_success()) {
    NOTREACHED();
    return false;
  }

  ResourceRequestInfoImpl* info = GetRequestInfo();

  bool defer = false;
  if (!handler_->OnReadCompleted(info->GetRequestID(), bytes_read, &defer)) {
    Cancel();
    return false;
  }

  if (defer) {
    deferred_stage_ = DEFERRED_READ;
    PauseRequest(true);
  }

  return *bytes_read != 0;
}

void ResourceLoader::ResponseCompleted() {
  VLOG(1) << "ResponseCompleted: " << request_->url().spec();
  ResourceRequestInfoImpl* info = GetRequestInfo();

  std::string security_info;
  const net::SSLInfo& ssl_info = request_->ssl_info();
  if (ssl_info.cert != NULL) {
    int cert_id = CertStore::GetInstance()->StoreCert(ssl_info.cert,
                                                      info->GetChildID());
    security_info = SerializeSecurityInfo(
        cert_id, ssl_info.cert_status, ssl_info.security_bits,
        ssl_info.connection_status);
  }

  if (handler_->OnResponseCompleted(info->GetRequestID(), request_->status(),
                                    security_info)) {
    // This will result in our destruction.
    CallDidFinishLoading();
  } else {
    // The handler is not ready to die yet.  We will call DidFinishLoading when
    // we resume.
    deferred_stage_ = DEFERRED_FINISH;
  }
}

bool ResourceLoader::PauseRequestIfNeeded() {
  if (pause_count_ > 0)
    is_paused_ = true;
  return is_paused_;
}

void ResourceLoader::PauseRequest(bool pause) {
  int pause_count = pause_count_ + (pause ? 1 : -1);
  if (pause_count < 0) {
    NOTREACHED();  // Unbalanced call to pause.
    return;
  }
  pause_count_ = pause_count;

  VLOG(1) << "To pause (" << pause << "): " << request_->url().spec();

  // If we're resuming, kick the request to start reading again. Run the read
  // asynchronously to avoid recursion problems.
  if (pause_count_ == 0) {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&ResourceLoader::ResumeRequest, AsWeakPtr()));
  }
}

void ResourceLoader::ResumeRequest() {
  // We may already be unpaused, or the pause count may have increased since we
  // posted the task to call ResumeRequest.
  if (!is_paused_)
    return;
  is_paused_ = false;
  if (PauseRequestIfNeeded())
    return;

  VLOG(1) << "Resuming: \"" << request_->url().spec() << "\""
          << " paused_read_bytes = " << paused_read_bytes_
          << " called response started = " << called_on_response_started_
          << " started reading = " << has_started_reading_;

  if (called_on_response_started_) {
    if (has_started_reading_) {
      OnReadCompleted(request_.get(), paused_read_bytes_);
    } else {
      StartReading();
    }
  } else {
    OnResponseStarted(request_.get());
  }
}

void ResourceLoader::CallDidFinishLoading() {
  delegate_->DidFinishLoading(this);
}

}  // namespace content
