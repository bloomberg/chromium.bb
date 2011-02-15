// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_handler.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/browser/ssl/ssl_cert_error_handler.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

SSLErrorHandler::SSLErrorHandler(ResourceDispatcherHost* rdh,
                                 net::URLRequest* request,
                                 ResourceType::Type resource_type)
    : manager_(NULL),
      request_id_(0, 0),
      resource_dispatcher_host_(rdh),
      request_url_(request->url()),
      resource_type_(resource_type),
      request_has_been_notified_(false) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  request_id_.child_id = info->child_id();
  request_id_.request_id = info->request_id();

  if (!ResourceDispatcherHost::RenderViewForRequest(request,
                                                    &render_process_host_id_,
                                                    &tab_contents_id_))
    NOTREACHED();

  // This makes sure we don't disappear on the IO thread until we've given an
  // answer to the net::URLRequest.
  //
  // Release in CompleteCancelRequest, CompleteContinueRequest, or
  // CompleteTakeNoAction.
  AddRef();
}

SSLErrorHandler::~SSLErrorHandler() {}

void SSLErrorHandler::OnDispatchFailed() {
  TakeNoAction();
}

void SSLErrorHandler::OnDispatched() {
  TakeNoAction();
}

SSLCertErrorHandler* SSLErrorHandler::AsSSLCertErrorHandler() {
  return NULL;
}

void SSLErrorHandler::Dispatch() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TabContents* tab_contents = GetTabContents();
  if (!tab_contents) {
    // We arrived on the UI thread, but the tab we're looking for is no longer
    // here.
    OnDispatchFailed();
    return;
  }

  // Hand ourselves off to the SSLManager.
  manager_ = tab_contents->controller().ssl_manager();
  OnDispatched();
}

TabContents* SSLErrorHandler::GetTabContents() {
  return tab_util::GetTabContentsByID(render_process_host_id_,
                                      tab_contents_id_);
}

void SSLErrorHandler::CancelRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We need to complete this task on the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &SSLErrorHandler::CompleteCancelRequest, net::ERR_ABORTED));
}

void SSLErrorHandler::DenyRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We need to complete this task on the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &SSLErrorHandler::CompleteCancelRequest,
          net::ERR_INSECURE_RESPONSE));
}

void SSLErrorHandler::ContinueRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We need to complete this task on the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &SSLErrorHandler::CompleteContinueRequest));
}

void SSLErrorHandler::TakeNoAction() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We need to complete this task on the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &SSLErrorHandler::CompleteTakeNoAction));
}

void SSLErrorHandler::CompleteCancelRequest(int error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // It is important that we notify the net::URLRequest only once.  If we try
  // to notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);
  if (request_has_been_notified_)
    return;

  net::URLRequest* request =
      resource_dispatcher_host_->GetURLRequest(request_id_);
  if (request) {
    // The request can be NULL if it was cancelled by the renderer (as the
    // result of the user navigating to a new page from the location bar).
    DVLOG(1) << "CompleteCancelRequest() url: " << request->url().spec();
    SSLCertErrorHandler* cert_error = AsSSLCertErrorHandler();
    if (cert_error)
      request->SimulateSSLError(error, cert_error->ssl_info());
    else
      request->SimulateError(error);
  }
  request_has_been_notified_ = true;

  // We're done with this object on the IO thread.
  Release();
}

void SSLErrorHandler::CompleteContinueRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // It is important that we notify the net::URLRequest only once. If we try to
  // notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);
  if (request_has_been_notified_)
    return;

  net::URLRequest* request =
      resource_dispatcher_host_->GetURLRequest(request_id_);
  if (request) {
    // The request can be NULL if it was cancelled by the renderer (as the
    // result of the user navigating to a new page from the location bar).
    DVLOG(1) << "CompleteContinueRequest() url: " << request->url().spec();
    request->ContinueDespiteLastError();
  }
  request_has_been_notified_ = true;

  // We're done with this object on the IO thread.
  Release();
}

void SSLErrorHandler::CompleteTakeNoAction() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // It is important that we notify the net::URLRequest only once. If we try to
  // notify the request twice, it may no longer exist and |this| might have
  // already have been deleted.
  DCHECK(!request_has_been_notified_);
  if (request_has_been_notified_)
    return;

  request_has_been_notified_ = true;

  // We're done with this object on the IO thread.
  Release();
}
