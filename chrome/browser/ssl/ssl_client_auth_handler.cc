// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_auth_handler.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request.h"

SSLClientAuthHandler::SSLClientAuthHandler(
    URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info)
    : request_(request),
      cert_request_info_(cert_request_info) {
  // Keep us alive until a cert is selected.
  AddRef();
}

SSLClientAuthHandler::~SSLClientAuthHandler() {
}

void SSLClientAuthHandler::OnRequestCancelled() {
  request_ = NULL;
}

void SSLClientAuthHandler::SelectCertificate() {
  // Let's move the request to the UI thread.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &SSLClientAuthHandler::DoSelectCertificate));
}

// Looking for DoSelectCertificate()?
// It's implemented in a separate source file for each platform.

// Notify the IO thread that we have selected a cert.
void SSLClientAuthHandler::CertificateSelected(net::X509Certificate* cert) {
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &SSLClientAuthHandler::DoCertificateSelected, cert));
}

void SSLClientAuthHandler::DoCertificateSelected(net::X509Certificate* cert) {
  // request_ could have been NULLed if the request was cancelled while the user
  // was choosing a cert.
  if (request_)
    request_->ContinueWithCertificate(cert);

  // We are done.
  Release();
}
