// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ssl_client_certificate_selector.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

#if defined(USE_NSS_CERTS) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/crypto_module_password_dialog_nss.h"
#endif

SSLClientCertificateSelector::SSLClientCertificateSelector(
    content::WebContents* web_contents,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
    std::unique_ptr<content::ClientCertificateDelegate> delegate)
    : CertificateSelector(cert_request_info->client_certs, web_contents),
      SSLClientAuthObserver(web_contents->GetBrowserContext(),
                            cert_request_info,
                            std::move(delegate)),
      WebContentsObserver(web_contents) {}

SSLClientCertificateSelector::~SSLClientCertificateSelector() {}

void SSLClientCertificateSelector::Init() {
  StartObserving();
  std::unique_ptr<views::Label> text_label(
      new views::Label(l10n_util::GetStringFUTF16(
          IDS_CLIENT_CERT_DIALOG_TEXT,
          base::ASCIIToUTF16(cert_request_info()->host_and_port.ToString()))));
  text_label->SetMultiLine(true);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label->SetAllowCharacterBreak(true);
  text_label->SizeToFit(kTableViewWidth);
  InitWithText(std::move(text_label));
}

void SSLClientCertificateSelector::OnCertSelectedByNotification() {
  GetWidget()->Close();
}

void SSLClientCertificateSelector::DeleteDelegate() {
  // This is here and not in Cancel() to give WebContentsDestroyed a chance
  // to abort instead of proceeding with a null certificate. (This will be
  // ignored if there was a previous call to CertificateSelected or
  // CancelCertificateSelection.)
  CertificateSelected(nullptr);
  chrome::CertificateSelector::DeleteDelegate();
}

bool SSLClientCertificateSelector::Accept() {
  scoped_refptr<net::X509Certificate> cert = GetSelectedCert();
  if (cert.get()) {
    // Remove the observer before we try unlocking, otherwise we might act on a
    // notification while waiting for the unlock dialog, causing us to delete
    // ourself before the Unlocked callback gets called.
    StopObserving();
#if defined(USE_NSS_CERTS) && !defined(OS_CHROMEOS)
    chrome::UnlockCertSlotIfNecessary(
        cert.get(), chrome::kCryptoModulePasswordClientAuth,
        cert_request_info()->host_and_port, GetWidget()->GetNativeView(),
        base::Bind(&SSLClientCertificateSelector::Unlocked,
                   base::Unretained(this), base::RetainedRef(cert)));
#else
    Unlocked(cert.get());
#endif
    return false;  // Unlocked() will close the dialog.
  }

  return false;
}

void SSLClientCertificateSelector::WebContentsDestroyed() {
  // If the dialog is closed by closing the containing tab, abort the request.
  CancelCertificateSelection();
}

void SSLClientCertificateSelector::Unlocked(net::X509Certificate* cert) {
  CertificateSelected(cert);
  GetWidget()->Close();
}

namespace chrome {

void ShowSSLClientCertificateSelector(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Not all WebContentses can show modal dialogs.
  //
  // TODO(davidben): Move this hook to the WebContentsDelegate and only try to
  // show a dialog in Browser's implementation. https://crbug.com/456255
  if (!SSLClientCertificateSelector::CanShow(contents))
    return;

  SSLClientCertificateSelector* selector = new SSLClientCertificateSelector(
      contents, cert_request_info, std::move(delegate));
  selector->Init();
  selector->Show();
}

}  // namespace chrome
