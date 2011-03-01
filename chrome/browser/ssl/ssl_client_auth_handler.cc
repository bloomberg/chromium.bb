// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_auth_handler.h"

#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_notification_task.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "net/url_request/url_request.h"

SSLClientAuthHandler::SSLClientAuthHandler(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info)
    : request_(request),
      cert_request_info_(cert_request_info) {
}

SSLClientAuthHandler::~SSLClientAuthHandler() {
  // If we were simply dropped, then act as if we selected no certificate.
  DoCertificateSelected(NULL);
}

void SSLClientAuthHandler::OnRequestCancelled() {
  request_ = NULL;
}

void SSLClientAuthHandler::SelectCertificate() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  int render_process_host_id;
  int render_view_host_id;
  if (!ResourceDispatcherHost::RenderViewForRequest(request_,
                                                    &render_process_host_id,
                                                    &render_view_host_id))
    NOTREACHED();

  // If the RVH does not exist by the time this task gets run, then the task
  // will be dropped and the scoped_refptr to SSLClientAuthHandler will go
  // away, so we do not leak anything. The destructor takes care of ensuring
  // the net::URLRequest always gets a response.
  CallRenderViewHostSSLDelegate(
      render_process_host_id, render_view_host_id,
      &RenderViewHostDelegate::SSL::ShowClientCertificateRequestDialog,
      scoped_refptr<SSLClientAuthHandler>(this));
}

// Notify the IO thread that we have selected a cert.
void SSLClientAuthHandler::CertificateSelected(net::X509Certificate* cert) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &SSLClientAuthHandler::DoCertificateSelected, cert));
}

void SSLClientAuthHandler::DoCertificateSelected(net::X509Certificate* cert) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // request_ could have been NULLed if the request was cancelled while the
  // user was choosing a cert, or because we have already responded to the
  // certificate.
  if (request_) {
    request_->ContinueWithCertificate(cert);
    request_ = NULL;
  }
}
