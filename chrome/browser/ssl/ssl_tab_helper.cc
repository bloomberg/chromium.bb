// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_tab_helper.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_add_cert_handler.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"


// SSLCertResultInfoBarDelegate -----------------------------------------------

namespace {

class SSLCertResultInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an SSL cert result infobar and delegate.  If |previous_infobar| is
  // NULL, adds the infobar to |infobar_service|; otherwise, replaces
  // |previous_infobar|.  Returns the new infobar if it was successfully added.
  // |cert| is valid iff cert addition was successful.
  static InfoBar* Create(InfoBarService* infobar_service,
                         InfoBar* previous_infobar,
                         const base::string16& message,
                         net::X509Certificate* cert);

 private:
  SSLCertResultInfoBarDelegate(const base::string16& message,
                               net::X509Certificate* cert);
  virtual ~SSLCertResultInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual int GetIconID() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  base::string16 message_;
  scoped_refptr<net::X509Certificate> cert_;  // The cert we added, if any.

  DISALLOW_COPY_AND_ASSIGN(SSLCertResultInfoBarDelegate);
};

// static
InfoBar* SSLCertResultInfoBarDelegate::Create(InfoBarService* infobar_service,
                                              InfoBar* previous_infobar,
                                              const base::string16& message,
                                              net::X509Certificate* cert) {
  scoped_ptr<InfoBar> infobar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(
          new SSLCertResultInfoBarDelegate(message, cert))));
  return previous_infobar ?
      infobar_service->ReplaceInfoBar(previous_infobar, infobar.Pass()) :
      infobar_service->AddInfoBar(infobar.Pass());
}

SSLCertResultInfoBarDelegate::SSLCertResultInfoBarDelegate(
    const base::string16& message,
    net::X509Certificate* cert)
    : ConfirmInfoBarDelegate(),
      message_(message),
      cert_(cert) {
}

SSLCertResultInfoBarDelegate::~SSLCertResultInfoBarDelegate() {
}

int SSLCertResultInfoBarDelegate::GetIconID() const {
  // TODO(davidben): use a more appropriate icon.
  return IDR_INFOBAR_SAVE_PASSWORD;
}

InfoBarDelegate::Type SSLCertResultInfoBarDelegate::GetInfoBarType() const {
  return cert_.get() ? PAGE_ACTION_TYPE : WARNING_TYPE;
}

base::string16 SSLCertResultInfoBarDelegate::GetMessageText() const {
  return message_;
}

int SSLCertResultInfoBarDelegate::GetButtons() const {
  return cert_.get() ? BUTTON_OK : BUTTON_NONE;
}

base::string16 SSLCertResultInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_ADD_CERT_SUCCESS_INFOBAR_BUTTON);
}

bool SSLCertResultInfoBarDelegate::Accept() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  ShowCertificateViewer(web_contents,
                        web_contents->GetView()->GetTopLevelNativeWindow(),
                        cert_.get());
  return false;  // Hiding the infobar just as the dialog opens looks weird.
}

}  // namespace


// SSLTabHelper::SSLAddCertData ------------------------------------------------

class SSLTabHelper::SSLAddCertData
    : public content::NotificationObserver {
 public:
  explicit SSLAddCertData(InfoBarService* infobar_service);
  virtual ~SSLAddCertData();

  // Displays an infobar, replacing |infobar_| if it exists.
  void ShowInfoBar(const base::string16& message, net::X509Certificate* cert);

 private:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  InfoBarService* infobar_service_;
  InfoBar* infobar_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SSLAddCertData);
};

SSLTabHelper::SSLAddCertData::SSLAddCertData(InfoBarService* infobar_service)
    : infobar_service_(infobar_service),
      infobar_(NULL) {
  content::Source<InfoBarService> source(infobar_service_);
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                 source);
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REPLACED,
                 source);
}

SSLTabHelper::SSLAddCertData::~SSLAddCertData() {
}

void SSLTabHelper::SSLAddCertData::ShowInfoBar(const base::string16& message,
                                               net::X509Certificate* cert) {
  infobar_ = SSLCertResultInfoBarDelegate::Create(infobar_service_, infobar_,
                                                  message, cert);
}

void SSLTabHelper::SSLAddCertData::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED ||
         type == chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REPLACED);
  if (infobar_ ==
      ((type == chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED) ?
          content::Details<InfoBar::RemovedDetails>(details)->first :
          content::Details<InfoBar::ReplacedDetails>(details)->first))
    infobar_ = NULL;
}


// SSLTabHelper ----------------------------------------------------------------

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SSLTabHelper);

SSLTabHelper::SSLTabHelper(content::WebContents* contents)
    : WebContentsObserver(contents),
      web_contents_(contents) {
}

SSLTabHelper::~SSLTabHelper() {
}

void SSLTabHelper::DidChangeVisibleSSLState() {
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (browser)
    browser->VisibleSSLStateChanged(web_contents_);
#endif  // !defined(OS_ANDROID)
}

void SSLTabHelper::ShowClientCertificateRequestDialog(
    const net::HttpNetworkSession* network_session,
    net::SSLCertRequestInfo* cert_request_info,
    const base::Callback<void(net::X509Certificate*)>& callback) {
  chrome::ShowSSLClientCertificateSelector(web_contents_, network_session,
                                           cert_request_info, callback);
}

void SSLTabHelper::OnVerifyClientCertificateError(
    scoped_refptr<SSLAddCertHandler> handler, int error_code) {
  // Display an infobar with the error message.
  // TODO(davidben): Display a more user-friendly error string.
  GetAddCertData(handler.get())->ShowInfoBar(
      l10n_util::GetStringFUTF16(IDS_ADD_CERT_ERR_INVALID_CERT,
                                 base::IntToString16(-error_code),
                                 base::ASCIIToUTF16(
                                     net::ErrorToString(error_code))),
      NULL);
}

void SSLTabHelper::AskToAddClientCertificate(
    scoped_refptr<SSLAddCertHandler> handler) {
  NOTREACHED();  // Not implemented yet.
}

void SSLTabHelper::OnAddClientCertificateSuccess(
    scoped_refptr<SSLAddCertHandler> handler) {
  net::X509Certificate* cert = handler->cert();
  // TODO(evanm): GetDisplayName should return UTF-16.
  GetAddCertData(handler.get())->ShowInfoBar(
      l10n_util::GetStringFUTF16(IDS_ADD_CERT_SUCCESS_INFOBAR_LABEL,
                                 base::UTF8ToUTF16(
                                     cert->issuer().GetDisplayName())),
      cert);
}

void SSLTabHelper::OnAddClientCertificateError(
    scoped_refptr<SSLAddCertHandler> handler,
    int error_code) {
  // TODO(davidben): Display a more user-friendly error string.
  GetAddCertData(handler.get())->ShowInfoBar(
      l10n_util::GetStringFUTF16(IDS_ADD_CERT_ERR_FAILED,
                                 base::IntToString16(-error_code),
                                 base::ASCIIToUTF16(
                                     net::ErrorToString(error_code))),
      NULL);
}

void SSLTabHelper::OnAddClientCertificateFinished(
    scoped_refptr<SSLAddCertHandler> handler) {
  // Clean up.
  request_id_to_add_cert_data_.erase(handler->network_request_id());
}

SSLTabHelper::SSLAddCertData*
    SSLTabHelper::GetAddCertData(SSLAddCertHandler* handler) {
  // Find/create the slot.
  linked_ptr<SSLAddCertData>& ptr_ref =
      request_id_to_add_cert_data_[handler->network_request_id()];
  // Fill it if necessary.
  if (!ptr_ref.get()) {
    ptr_ref.reset(
        new SSLAddCertData(InfoBarService::FromWebContents(web_contents_)));
  }
  return ptr_ref.get();
}
