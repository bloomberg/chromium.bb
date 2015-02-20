// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ssl_client_certificate_selector.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

#if defined(USE_NSS)
#include "chrome/browser/ui/crypto_module_password_dialog_nss.h"
#endif

SSLClientCertificateSelector::SSLClientCertificateSelector(
    content::WebContents* web_contents,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
    const chrome::SelectCertificateCallback& callback)
    : CertificateSelector(cert_request_info->client_certs, web_contents),
      SSLClientAuthObserver(web_contents->GetBrowserContext(),
                            cert_request_info,
                            callback) {
  DVLOG(1) << __FUNCTION__;
}

SSLClientCertificateSelector::~SSLClientCertificateSelector() {
}

void SSLClientCertificateSelector::Init() {
  StartObserving();
  InitWithText(l10n_util::GetStringFUTF16(
      IDS_CLIENT_CERT_DIALOG_TEXT,
      base::ASCIIToUTF16(cert_request_info()->host_and_port.ToString())));
}

void SSLClientCertificateSelector::OnCertSelectedByNotification() {
  DVLOG(1) << __FUNCTION__;
  GetWidget()->Close();
}

bool SSLClientCertificateSelector::Cancel() {
  DVLOG(1) << __FUNCTION__;
  StopObserving();
  CertificateSelected(nullptr);
  return true;
}

bool SSLClientCertificateSelector::Accept() {
  DVLOG(1) << __FUNCTION__;
  scoped_refptr<net::X509Certificate> cert = GetSelectedCert();
  if (cert.get()) {
    // Remove the observer before we try unlocking, otherwise we might act on a
    // notification while waiting for the unlock dialog, causing us to delete
    // ourself before the Unlocked callback gets called.
    StopObserving();
#if defined(USE_NSS)
    chrome::UnlockCertSlotIfNecessary(
        cert.get(), chrome::kCryptoModulePasswordClientAuth,
        cert_request_info()->host_and_port, GetWidget()->GetNativeView(),
        base::Bind(&SSLClientCertificateSelector::Unlocked,
                   base::Unretained(this), cert));
#else
    Unlocked(cert.get());
#endif
    return false;  // Unlocked() will close the dialog.
  }

  return false;
}

void SSLClientCertificateSelector::Unlocked(net::X509Certificate* cert) {
  DVLOG(1) << __FUNCTION__;
  CertificateSelected(cert);
  GetWidget()->Close();
}

namespace chrome {

void ShowSSLClientCertificateSelector(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    const chrome::SelectCertificateCallback& callback) {
  DVLOG(1) << __FUNCTION__ << " " << contents;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SSLClientCertificateSelector* selector =
      new SSLClientCertificateSelector(contents, cert_request_info, callback);
  selector->Init();
  selector->Show();
}

}  // namespace chrome
