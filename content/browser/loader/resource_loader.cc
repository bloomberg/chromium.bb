// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_loader.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/loader/doomed_resource_handler.h"
#include "content/browser/loader/resource_loader_delegate.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/common/ssl_status_serialization.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/client_cert_store_impl.h"
#include "webkit/appcache/appcache_interceptor.h"

using base::TimeDelta;
using base::TimeTicks;

namespace content {
namespace {

void PopulateResourceResponse(net::URLRequest* request,
                              ResourceResponse* response) {
  response->head.error_code = request->status().error();
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
    : weak_ptr_factory_(this) {
  scoped_ptr<net::ClientCertStore> client_cert_store;
#if !defined(USE_OPENSSL)
  client_cert_store.reset(new net::ClientCertStoreImpl());
#endif
  Init(request.Pass(), handler.Pass(), delegate, client_cert_store.Pass());
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
    CancelAndIgnore();
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

void ResourceLoader::CancelAndIgnore() {
  ResourceRequestInfoImpl* info = GetRequestInfo();
  info->set_was_ignored_by_handler(true);
  CancelRequest(false);
}

void ResourceLoader::CancelWithError(int error_code) {
  CancelRequestInternal(error_code, false);
}

void ResourceLoader::ReportUploadProgress() {
  ResourceRequestInfoImpl* info = GetRequestInfo();

  if (waiting_for_upload_progress_ack_)
    return;  // Send one progress event at a time.

  net::UploadProgress progress = request_->GetUploadProgress();
  if (!progress.size())
    return;  // Nothing to upload.

  if (progress.position() == last_upload_position_)
    return;  // No progress made since last time.

  const uint64 kHalfPercentIncrements = 200;
  const TimeDelta kOneSecond = TimeDelta::FromMilliseconds(1000);

  uint64 amt_since_last = progress.position() - last_upload_position_;
  TimeDelta time_since_last = TimeTicks::Now() - last_upload_ticks_;

  bool is_finished = (progress.size() == progress.position());
  bool enough_new_progress =
      (amt_since_last > (progress.size() / kHalfPercentIncrements));
  bool too_much_time_passed = time_since_last > kOneSecond;

  if (is_finished || enough_new_progress || too_much_time_passed) {
    if (request_->load_flags() & net::LOAD_ENABLE_UPLOAD_PROGRESS) {
      handler_->OnUploadProgress(
          info->GetRequestID(), progress.position(), progress.size());
      waiting_for_upload_progress_ack_ = true;
    }
    last_upload_ticks_ = TimeTicks::Now();
    last_upload_position_ = progress.position();
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

void ResourceLoader::WillCompleteTransfer() {
  handler_.reset();
}

void ResourceLoader::CompleteTransfer(scoped_ptr<ResourceHandler> new_handler) {
  DCHECK_EQ(DEFERRED_REDIRECT, deferred_stage_);
  DCHECK(!handler_.get());

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

ResourceLoader::ResourceLoader(
    scoped_ptr<net::URLRequest> request,
    scoped_ptr<ResourceHandler> handler,
    ResourceLoaderDelegate* delegate,
    scoped_ptr<net::ClientCertStore> client_cert_store)
    : weak_ptr_factory_(this) {
  Init(request.Pass(), handler.Pass(), delegate, client_cert_store.Pass());
}

void ResourceLoader::Init(scoped_ptr<net::URLRequest> request,
                          scoped_ptr<ResourceHandler> handler,
                          ResourceLoaderDelegate* delegate,
                          scoped_ptr<net::ClientCertStore> client_cert_store) {
  deferred_stage_ = DEFERRED_NONE;
  request_ = request.Pass();
  handler_ = handler.Pass();
  delegate_ = delegate;
  last_upload_position_ = 0;
  waiting_for_upload_progress_ack_ = false;
  is_transferring_ = false;
  client_cert_store_ = client_cert_store.Pass();

  request_->set_delegate(this);
  handler_->SetController(this);
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
    CancelAndIgnore();
    return;
  }

  scoped_refptr<ResourceResponse> response(new ResourceResponse());
  PopulateResourceResponse(request_.get(), response);

  if (!handler_->OnRequestRedirected(info->GetRequestID(), new_url, response,
                                     defer)) {
    Cancel();
  } else if (*defer) {
    deferred_stage_ = DEFERRED_REDIRECT;  // Follow redirect when resumed.
  }
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

#if !defined(USE_OPENSSL)
  client_cert_store_->GetClientCerts(*cert_info, &cert_info->client_certs);
  if (cert_info->client_certs.empty()) {
    // No need to query the user if there are no certs to choose from.
    request_->ContinueWithCertificate(NULL);
    return;
  }
#endif

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
      weak_ptr_factory_.GetWeakPtr(),
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

  // The CanLoadPage check should take place after any server redirects have
  // finished, at the point in time that we know a page will commit in the
  // renderer process.
  ResourceRequestInfoImpl* info = GetRequestInfo();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanLoadPage(info->GetChildID(),
                           request_->url(),
                           info->GetResourceType())) {
    Cancel();
    return;
  }

  if (!request_->status().is_success()) {
    ResponseCompleted();
    return;
  }

  // We want to send a final upload progress message prior to sending the
  // response complete message even if we're waiting for an ack to to a
  // previous upload progress message.
  waiting_for_upload_progress_ack_ = false;
  ReportUploadProgress();

  CompleteResponseStarted();

  if (is_deferred())
    return;

  if (request_->status().is_success()) {
    StartReading(false);  // Read the first chunk.
  } else {
    ResponseCompleted();
  }
}

void ResourceLoader::OnReadCompleted(net::URLRequest* unused, int bytes_read) {
  DCHECK_EQ(request_.get(), unused);
  VLOG(1) << "OnReadCompleted: \"" << request_->url().spec() << "\""
          << " bytes_read = " << bytes_read;

  // bytes_read == -1 always implies an error.
  if (bytes_read == -1 || !request_->status().is_success()) {
    ResponseCompleted();
    return;
  }

  CompleteRead(bytes_read);

  if (is_deferred())
    return;

  if (request_->status().is_success() && bytes_read > 0) {
    StartReading(true);  // Read the next chunk.
  } else {
    ResponseCompleted();
  }
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
    case DEFERRED_READ:
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ResourceLoader::ResumeReading,
                     weak_ptr_factory_.GetWeakPtr()));
      break;
    case DEFERRED_FINISH:
      // Delay self-destruction since we don't know how we were reached.
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ResourceLoader::CallDidFinishLoading,
                     weak_ptr_factory_.GetWeakPtr()));
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
  if (from_renderer && (info->is_download() || info->is_stream()))
    return;

  // TODO(darin): Perhaps we should really be looking to see if the status is
  // IO_PENDING?
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
        FROM_HERE, base::Bind(&ResourceLoader::ResponseCompleted,
                              weak_ptr_factory_.GetWeakPtr()));
  }
}

void ResourceLoader::CompleteResponseStarted() {
  ResourceRequestInfoImpl* info = GetRequestInfo();

  scoped_refptr<ResourceResponse> response(new ResourceResponse());
  PopulateResourceResponse(request_.get(), response);

  // The --site-per-process flag enables an out-of-process iframes
  // prototype. It works by changing the MIME type of cross-site subframe
  // responses to a Chrome specific one. This new type causes the subframe
  // to be replaced by a <webview> tag with the same URL, which results in
  // using a renderer in a different process.
  //
  // For prototyping purposes, we will use a small hack to ensure same site
  // iframes are not changed. We can compare the URL for the subframe
  // request with the referrer. If the two don't match, then it should be a
  // cross-site iframe.
  // Also, we don't do the MIME type change for chrome:// URLs, as those
  // require different privileges and are not allowed in regular renderers.
  //
  // The usage of SiteInstance::IsSameWebSite is safe on the IO thread,
  // if the browser_context parameter is NULL. This does not work for hosted
  // apps, but should be fine for prototyping.
  // TODO(nasko): Once the SiteInstance check is fixed, ensure we do the
  // right thing here. http://crbug.com/160576
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kSitePerProcess) &&
      GetRequestInfo()->GetResourceType() == ResourceType::SUB_FRAME &&
      response->head.mime_type == "text/html" &&
      !request_->url().SchemeIs(chrome::kChromeUIScheme) &&
      !SiteInstance::IsSameWebSite(NULL, request_->url(),
          request_->GetSanitizedReferrer())) {
    response->head.mime_type = "application/browser-plugin";
  }

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

  bool defer = false;
  if (!handler_->OnResponseStarted(info->GetRequestID(), response, &defer)) {
    Cancel();
  } else if (defer) {
    deferred_stage_ = DEFERRED_READ;  // Read first chunk when resumed.
  }
}

void ResourceLoader::StartReading(bool is_continuation) {
  int bytes_read = 0;
  ReadMore(&bytes_read);

  // If IO is pending, wait for the URLRequest to call OnReadCompleted.
  if (request_->status().is_io_pending())
    return;

  if (!is_continuation || bytes_read <= 0) {
    OnReadCompleted(request_.get(), bytes_read);
  } else {
    // Else, trigger OnReadCompleted asynchronously to avoid starving the IO
    // thread in case the URLRequest can provide data synchronously.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ResourceLoader::OnReadCompleted,
                   weak_ptr_factory_.GetWeakPtr(),
                   request_.get(), bytes_read));
  }
}

void ResourceLoader::ResumeReading() {
  DCHECK(!is_deferred());

  if (request_->status().is_success()) {
    StartReading(false);  // Read the next chunk (OK to complete synchronously).
  } else {
    ResponseCompleted();
  }
}

void ResourceLoader::ReadMore(int* bytes_read) {
  ResourceRequestInfoImpl* info = GetRequestInfo();
  DCHECK(!is_deferred());

  net::IOBuffer* buf;
  int buf_size;
  if (!handler_->OnWillRead(info->GetRequestID(), &buf, &buf_size, -1)) {
    Cancel();
    return;
  }

  DCHECK(buf);
  DCHECK(buf_size > 0);

  request_->Read(buf, buf_size, bytes_read);

  // No need to check the return value here as we'll detect errors by
  // inspecting the URLRequest's status.
}

void ResourceLoader::CompleteRead(int bytes_read) {
  DCHECK(bytes_read >= 0);
  DCHECK(request_->status().is_success());

  ResourceRequestInfoImpl* info = GetRequestInfo();

  bool defer = false;
  if (!handler_->OnReadCompleted(info->GetRequestID(), bytes_read, &defer)) {
    Cancel();
  } else if (defer) {
    deferred_stage_ = DEFERRED_READ;  // Read next chunk when resumed.
  }
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

void ResourceLoader::CallDidFinishLoading() {
  delegate_->DidFinishLoading(this);
}

}  // namespace content
