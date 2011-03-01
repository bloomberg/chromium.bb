// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_add_cert_handler.h"

#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_notification_task.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "net/base/cert_database.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"
#include "net/url_request/url_request.h"

SSLAddCertHandler::SSLAddCertHandler(net::URLRequest* request,
                                     net::X509Certificate* cert,
                                     int render_process_host_id,
                                     int render_view_id)
    : cert_(cert),
      render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id) {
  ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  network_request_id_ = info->request_id();
  // Stay alive until the process completes and Finished() is called.
  AddRef();
  // Delay adding the certificate until the next mainloop iteration.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &SSLAddCertHandler::Run));
}

SSLAddCertHandler::~SSLAddCertHandler() {}

void SSLAddCertHandler::Run() {
  int cert_error;
  {
    net::CertDatabase db;
    cert_error = db.CheckUserCert(cert_);
  }
  if (cert_error != net::OK) {
    CallRenderViewHostSSLDelegate(
        render_process_host_id_, render_view_id_,
        &RenderViewHostDelegate::SSL::OnVerifyClientCertificateError,
        scoped_refptr<SSLAddCertHandler>(this), cert_error);
    Finished(false);
    return;
  }
  // TODO(davidben): Move the existing certificate dialog elsewhere, make
  // AskToAddCert send a message to the RenderViewHostDelegate, and ask when we
  // cannot completely verify the certificate for whatever reason.

  // AskToAddCert();
  Finished(true);
}

#if !defined(OS_MACOSX)
void SSLAddCertHandler::AskToAddCert() {
  // TODO(snej): Someone should add Windows and GTK implementations with UI.
  Finished(true);
}
#endif

void SSLAddCertHandler::Finished(bool add_cert) {
  if (add_cert) {
    net::CertDatabase db;
    int cert_error = db.AddUserCert(cert_);
    if (cert_error != net::OK) {
      CallRenderViewHostSSLDelegate(
          render_process_host_id_, render_view_id_,
          &RenderViewHostDelegate::SSL::OnAddClientCertificateError,
          scoped_refptr<SSLAddCertHandler>(this), cert_error);
    } else {
      CallRenderViewHostSSLDelegate(
          render_process_host_id_, render_view_id_,
          &RenderViewHostDelegate::SSL::OnAddClientCertificateSuccess,
          scoped_refptr<SSLAddCertHandler>(this));
    }
  }
  // Inform the RVH that we're finished
  CallRenderViewHostSSLDelegate(
      render_process_host_id_, render_view_id_,
      &RenderViewHostDelegate::SSL::OnAddClientCertificateFinished,
      scoped_refptr<SSLAddCertHandler>(this));

  Release();
}
