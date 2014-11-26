// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_client_auth_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_store.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

typedef base::Callback<void(net::X509Certificate*)> CertificateCallback;

void CertificateSelectedOnUIThread(
    const CertificateCallback& io_thread_callback,
    net::X509Certificate* cert) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(io_thread_callback, make_scoped_refptr(cert)));
}

void SelectCertificateOnUIThread(
    int render_process_host_id,
    int render_frame_host_id,
    net::SSLCertRequestInfo* cert_request_info,
    const CertificateCallback& io_thread_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GetContentClient()->browser()->SelectClientCertificate(
      render_process_host_id, render_frame_host_id, cert_request_info,
      base::Bind(&CertificateSelectedOnUIThread, io_thread_callback));
}

}  // namespace

SSLClientAuthHandler::SSLClientAuthHandler(
    scoped_ptr<net::ClientCertStore> client_cert_store,
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info,
    const SSLClientAuthHandler::CertificateCallback& callback)
    : request_(request),
      cert_request_info_(cert_request_info),
      client_cert_store_(client_cert_store.Pass()),
      callback_(callback),
      weak_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

SSLClientAuthHandler::~SSLClientAuthHandler() {
}

void SSLClientAuthHandler::SelectCertificate() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (client_cert_store_) {
    client_cert_store_->GetClientCerts(
        *cert_request_info_,
        &cert_request_info_->client_certs,
        base::Bind(&SSLClientAuthHandler::DidGetClientCerts,
                   weak_factory_.GetWeakPtr()));
  } else {
    DidGetClientCerts();
  }
}

void SSLClientAuthHandler::DidGetClientCerts() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Note that if |client_cert_store_| is NULL, we intentionally fall through to
  // DoCertificateSelected. This is for platforms where the client cert matching
  // is not performed by Chrome. Those platforms handle the cert matching before
  // showing the dialog.
  if (client_cert_store_ && cert_request_info_->client_certs.empty()) {
    // No need to query the user if there are no certs to choose from.
    CertificateSelected(NULL);
    return;
  }

  int render_process_host_id;
  int render_frame_host_id;
  if (!ResourceRequestInfo::ForRequest(request_)->GetAssociatedRenderFrame(
          &render_process_host_id,
          &render_frame_host_id)) {
    NOTREACHED();
    CertificateSelected(NULL);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SelectCertificateOnUIThread,
                 render_process_host_id, render_frame_host_id,
                 cert_request_info_,
                 base::Bind(&SSLClientAuthHandler::CertificateSelected,
                            weak_factory_.GetWeakPtr())));
}

void SSLClientAuthHandler::CertificateSelected(net::X509Certificate* cert) {
  DVLOG(1) << this << " DoCertificateSelected " << cert;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  callback_.Run(cert);
  // |this| may be deleted at this point.
}

}  // namespace content
