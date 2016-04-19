// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_add_certificate.h"

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icons_public.h"

using content::BrowserThread;
using content::RenderFrameHost;
using content::WebContents;

namespace chrome {

namespace {

class SSLAddCertificateInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an SSL certificate enrollment result infobar and delegate and adds
   // the infobar to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     net::X509Certificate* cert);

 private:
  explicit SSLAddCertificateInfoBarDelegate(net::X509Certificate* cert);
  ~SSLAddCertificateInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  Type GetInfoBarType() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  gfx::VectorIconId GetVectorIconId() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

  // The certificate that was added.
  scoped_refptr<net::X509Certificate> cert_;

  DISALLOW_COPY_AND_ASSIGN(SSLAddCertificateInfoBarDelegate);
};

// static
void SSLAddCertificateInfoBarDelegate::Create(InfoBarService* infobar_service,
                                              net::X509Certificate* cert) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new SSLAddCertificateInfoBarDelegate(cert))));
}

SSLAddCertificateInfoBarDelegate::SSLAddCertificateInfoBarDelegate(
    net::X509Certificate* cert)
    : cert_(cert) {
}

SSLAddCertificateInfoBarDelegate::~SSLAddCertificateInfoBarDelegate() {
}

infobars::InfoBarDelegate::Type
SSLAddCertificateInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

infobars::InfoBarDelegate::InfoBarIdentifier
SSLAddCertificateInfoBarDelegate::GetIdentifier() const {
  return SSL_ADD_CERTIFICATE_INFOBAR_DELEGATE;
}

int SSLAddCertificateInfoBarDelegate::GetIconId() const {
  // TODO(davidben): Use a more appropriate icon.
  return IDR_INFOBAR_SAVE_PASSWORD;
}

gfx::VectorIconId SSLAddCertificateInfoBarDelegate::GetVectorIconId() const {
#if !defined(OS_MACOSX)
  return gfx::VectorIconId::AUTOLOGIN;
#else
  return gfx::VectorIconId::VECTOR_ICON_NONE;
#endif
}

base::string16 SSLAddCertificateInfoBarDelegate::GetMessageText() const {
  // TODO(evanm): GetDisplayName should return UTF-16.
  return l10n_util::GetStringFUTF16(IDS_ADD_CERT_SUCCESS_INFOBAR_LABEL,
                                    base::UTF8ToUTF16(
                                        cert_->issuer().GetDisplayName()));
}

int SSLAddCertificateInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 SSLAddCertificateInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_ADD_CERT_SUCCESS_INFOBAR_BUTTON);
}

bool SSLAddCertificateInfoBarDelegate::Accept() {
  WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  ShowCertificateViewer(web_contents,
                        web_contents->GetTopLevelNativeWindow(),
                        cert_.get());
  // It looks weird to hide the infobar just as the dialog opens.
  return false;
}

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
      infobars::InfoBarDelegate::SSL_ADD_CERTIFICATE,
      IDR_INFOBAR_SAVE_PASSWORD,
#if !defined(OS_MACOSX)
      gfx::VectorIconId::AUTOLOGIN,
#else
      gfx::VectorIconId::VECTOR_ICON_NONE,
#endif
      l10n_util::GetStringFUTF16(
          IDS_ADD_CERT_ERR_INVALID_CERT, base::IntToString16(-cert_error),
          base::ASCIIToUTF16(net::ErrorToString(cert_error))),
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
        base::Bind(&ShowSuccessInfoBar, render_process_id, render_frame_id,
                   base::RetainedRef(cert)));
  }
}

}  // namespace chrome
