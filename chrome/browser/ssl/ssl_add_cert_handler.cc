// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_add_cert_handler.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/platform_util.h"
#include "grit/generated_resources.h"
#include "net/base/cert_database.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"
#include "net/url_request/url_request.h"

SSLAddCertHandler::SSLAddCertHandler(URLRequest* request,
                                     net::X509Certificate* cert)
    : cert_(cert) {
  // Stay alive until the UI completes and Finished() is called.
  AddRef();
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &SSLAddCertHandler::RunUI));
}

void SSLAddCertHandler::RunUI() {
  int cert_error;
  {
    net::CertDatabase db;
    cert_error = db.CheckUserCert(cert_);
  }
  if (cert_error != net::OK) {
    // TODO(snej): Map cert_error to a more specific error message.
    ShowError(l10n_util::GetStringFUTF16(
        IDS_ADD_CERT_ERR_INVALID_CERT,
        IntToString16(-cert_error),
        ASCIIToUTF16(net::ErrorToString(cert_error))));
    Finished(false);
    return;
  }
  AskToAddCert();
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
      // TODO(snej): Map cert_error to a more specific error message.
      ShowError(l10n_util::GetStringFUTF16(
          IDS_ADD_CERT_ERR_FAILED,
          IntToString16(-cert_error),
          ASCIIToUTF16(net::ErrorToString(cert_error))));
    }
  }
  Release();
}

void SSLAddCertHandler::ShowError(const string16& error) {
  Browser* browser = BrowserList::GetLastActive();
  platform_util::SimpleErrorBox(
      browser ? browser->window()->GetNativeHandle() : NULL,
      l10n_util::GetStringUTF16(IDS_ADD_CERT_FAILURE_TITLE),
      error);
}
