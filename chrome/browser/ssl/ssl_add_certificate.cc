// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_add_certificate.h"

#include "base/basictypes.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/infobars/simple_alert_infobar_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::RenderFrameHost;
using content::WebContents;

namespace chrome {

namespace {

class SSLAddCertificateInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an SSL certificate enrollment result infobar and delegate.
  static void Create(InfoBarService* infobar_service,
                     net::X509Certificate* cert) {
    infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
        scoped_ptr<ConfirmInfoBarDelegate>(
            new SSLAddCertificateInfoBarDelegate(cert))));
  }

 private:
  explicit SSLAddCertificateInfoBarDelegate(net::X509Certificate* cert)
      : cert_(cert) {}
  virtual ~SSLAddCertificateInfoBarDelegate() {}

  // ConfirmInfoBarDelegate implementation:
  virtual int GetIconID() const OVERRIDE {
    // TODO(davidben): Use a more appropriate icon.
    return IDR_INFOBAR_SAVE_PASSWORD;
  }

  virtual Type GetInfoBarType() const OVERRIDE {
    return PAGE_ACTION_TYPE;
  }

  virtual base::string16 GetMessageText() const OVERRIDE {
    // TODO(evanm): GetDisplayName should return UTF-16.
    return l10n_util::GetStringFUTF16(IDS_ADD_CERT_SUCCESS_INFOBAR_LABEL,
                                      base::UTF8ToUTF16(
                                          cert_->issuer().GetDisplayName()));
  }

  virtual int GetButtons() const OVERRIDE {
    return BUTTON_OK;
  }

  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE {
    DCHECK_EQ(BUTTON_OK, button);
    return l10n_util::GetStringUTF16(IDS_ADD_CERT_SUCCESS_INFOBAR_BUTTON);
  }

  virtual bool Accept() OVERRIDE {
    WebContents* web_contents =
        InfoBarService::WebContentsFromInfoBar(infobar());
    ShowCertificateViewer(web_contents,
                          web_contents->GetTopLevelNativeWindow(),
                          cert_.get());
    // It looks weird to hide the infobar just as the dialog opens.
    return false;
  }

  // The certificate that was added.
  scoped_refptr<net::X509Certificate> cert_;

  DISALLOW_COPY_AND_ASSIGN(SSLAddCertificateInfoBarDelegate);
};

void ShowErrorInfoBar(int message_id,
                      int render_process_id,
                      int render_frame_id,
                      int cert_error) {
  WebContents* web_contents = WebContents::FromRenderFrameHost(
      RenderFrameHost::FromID(render_process_id, render_frame_id));
  if (!web_contents)
    return;

  // TODO(davidben): Use a more appropriate icon.
  // TODO(davidben): Display a more user-friendly error string.
  SimpleAlertInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents),
      IDR_INFOBAR_SAVE_PASSWORD,
      l10n_util::GetStringFUTF16(IDS_ADD_CERT_ERR_INVALID_CERT,
                                 base::IntToString16(-cert_error),
                                 base::ASCIIToUTF16(
                                     net::ErrorToString(cert_error))),
      true);
}

void ShowSuccessInfoBar(int render_process_id,
                        int render_frame_id,
                        net::X509Certificate* cert) {
  WebContents* web_contents = WebContents::FromRenderFrameHost(
      RenderFrameHost::FromID(render_process_id, render_frame_id));
  if (!web_contents)
    return;

  SSLAddCertificateInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents), cert);
}

}  // namespace

void SSLAddCertificate(
    net::CertificateMimeType cert_type,
    const void* cert_data,
    size_t cert_size,
    int render_process_id,
    int render_frame_id) {
  // Chromium only supports X.509 User certificates on non-Android
  // platforms. Note that this method should not be called for other
  // certificate mime types.
  if (cert_type != net::CERTIFICATE_MIME_TYPE_X509_USER_CERT)
    return;

  scoped_refptr<net::X509Certificate> cert;
  if (cert_data != NULL) {
    cert = net::X509Certificate::CreateFromBytes(
        reinterpret_cast<const char*>(cert_data), cert_size);
  }
  // NOTE: Passing a NULL cert pointer if |cert_data| was NULL is
  // intentional here.

  // Check if we have a corresponding private key.
  int cert_error = net::CertDatabase::GetInstance()->CheckUserCert(cert.get());
  if (cert_error != net::OK) {
    LOG_IF(ERROR, cert_error == net::ERR_NO_PRIVATE_KEY_FOR_CERT)
        << "No corresponding private key in store for cert: "
        << (cert.get() ? cert->subject().GetDisplayName() : "NULL");

    BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ShowErrorInfoBar, IDS_ADD_CERT_ERR_INVALID_CERT,
                 render_process_id, render_frame_id, cert_error));
    return;
  }

  // Install it.
  cert_error = net::CertDatabase::GetInstance()->AddUserCert(cert.get());

  // Show the appropriate infobar.
  if (cert_error != net::OK) {
    BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ShowErrorInfoBar, IDS_ADD_CERT_ERR_FAILED,
                 render_process_id, render_frame_id, cert_error));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ShowSuccessInfoBar,
                   render_process_id, render_frame_id, cert));
  }
}

}  // namespace chrome
