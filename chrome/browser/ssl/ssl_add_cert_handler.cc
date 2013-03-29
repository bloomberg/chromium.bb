// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_add_cert_handler.h"

#include "base/bind.h"
#include "chrome/browser/ssl/ssl_tab_helper.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/x509_certificate.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::WebContents;

SSLAddCertHandler::SSLAddCertHandler(net::URLRequest* request,
                                     net::X509Certificate* cert,
                                     int render_process_host_id,
                                     int render_view_id)
    : cert_(cert),
      render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id) {
  network_request_id_
      = content::ResourceRequestInfo::ForRequest(request)->GetRequestID();
  // Stay alive until the process completes and Finished() is called.
  AddRef();
  // Delay adding the certificate until the next mainloop iteration.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SSLAddCertHandler::Run, this));
}

SSLAddCertHandler::~SSLAddCertHandler() {}

void SSLAddCertHandler::Run() {
  int cert_error = net::CertDatabase::GetInstance()->CheckUserCert(cert_);
  if (cert_error != net::OK) {
    LOG_IF(ERROR, cert_error == net::ERR_NO_PRIVATE_KEY_FOR_CERT)
        << "No corresponding private key in store for cert: "
        << (cert_.get() ? cert_->subject().GetDisplayName() : "NULL");

    BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &SSLAddCertHandler::CallVerifyClientCertificateError, this,
          cert_error));
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
  int cert_error = net::OK;
  if (add_cert)
    cert_error = net::CertDatabase::GetInstance()->AddUserCert(cert_);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &SSLAddCertHandler::CallAddClientCertificate, this,
          add_cert, cert_error));

  Release();
}

void SSLAddCertHandler::CallVerifyClientCertificateError(int cert_error) {
  WebContents* tab = tab_util::GetWebContentsByID(
      render_process_host_id_, render_view_id_);
  if (!tab)
    return;

  SSLTabHelper* ssl_tab_helper = SSLTabHelper::FromWebContents(tab);
  ssl_tab_helper->OnVerifyClientCertificateError(this, cert_error);
}

void SSLAddCertHandler::CallAddClientCertificate(bool add_cert,
                                                 int cert_error) {
  WebContents* tab = tab_util::GetWebContentsByID(
      render_process_host_id_, render_view_id_);
  if (!tab)
    return;

  SSLTabHelper* ssl_tab_helper = SSLTabHelper::FromWebContents(tab);
  if (add_cert) {
    if (cert_error == net::OK) {
      ssl_tab_helper->OnAddClientCertificateSuccess(this);
    } else {
      ssl_tab_helper->OnAddClientCertificateError(this, cert_error);
    }
  }
  ssl_tab_helper->OnAddClientCertificateFinished(this);
}
