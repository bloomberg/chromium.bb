// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_blocking_page.h"

#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/jstemplate_builder.h"
#include "content/browser/cert_store.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/ssl/ssl_cert_error_handler.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

enum SSLBlockingPageEvent {
  SHOW,
  PROCEED,
  DONT_PROCEED,
  UNUSED_ENUM,
};

void RecordSSLBlockingPageStats(SSLBlockingPageEvent event) {
  UMA_HISTOGRAM_ENUMERATION("interstial.ssl", event, UNUSED_ENUM);
}

}  // namespace

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
SSLBlockingPage::SSLBlockingPage(
    SSLCertErrorHandler* handler,
    bool overridable,
    const base::Callback<void(SSLCertErrorHandler*, bool)>& callback)
    : ChromeInterstitialPage(
          tab_util::GetTabContentsByID(
              handler->render_process_host_id(), handler->tab_contents_id()),
          true,
          handler->request_url()),
      handler_(handler),
      callback_(callback),
      overridable_(overridable) {
  RecordSSLBlockingPageStats(SHOW);
}

SSLBlockingPage::~SSLBlockingPage() {
  if (!callback_.is_null()) {
    // The page is closed without the user having chosen what to do, default to
    // deny.
    NotifyDenyCertificate();
  }
}

std::string SSLBlockingPage::GetHTMLContents() {
  // Let's build the html error page.
  DictionaryValue strings;
  SSLErrorInfo error_info = SSLErrorInfo::CreateError(
      SSLErrorInfo::NetErrorToErrorType(handler_->cert_error()),
      handler_->ssl_info().cert, handler_->request_url());

  strings.SetString("headLine", error_info.title());
  strings.SetString("description", error_info.details());

  strings.SetString("moreInfoTitle",
                    l10n_util::GetStringUTF16(IDS_CERT_ERROR_EXTRA_INFO_TITLE));
  SetExtraInfo(&strings, error_info.extra_information());

  int resource_id;
  if (overridable_) {
    resource_id = IDR_SSL_ROAD_BLOCK_HTML;
    strings.SetString("title",
                      l10n_util::GetStringUTF16(IDS_SSL_BLOCKING_PAGE_TITLE));
    strings.SetString("proceed",
                      l10n_util::GetStringUTF16(IDS_SSL_BLOCKING_PAGE_PROCEED));
    strings.SetString("exit",
                      l10n_util::GetStringUTF16(IDS_SSL_BLOCKING_PAGE_EXIT));
  } else {
    resource_id = IDR_SSL_ERROR_HTML;
    strings.SetString("title",
                      l10n_util::GetStringUTF16(IDS_SSL_ERROR_PAGE_TITLE));
    strings.SetString("back",
                      l10n_util::GetStringUTF16(IDS_SSL_ERROR_PAGE_BACK));
  }

  strings.SetString("textdirection", base::i18n::IsRTL() ? "rtl" : "ltr");

  base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id));

  return jstemplate_builder::GetI18nTemplateHtml(html, &strings);
}

void SSLBlockingPage::UpdateEntry(NavigationEntry* entry) {
  const net::SSLInfo& ssl_info = handler_->ssl_info();
  int cert_id = CertStore::GetInstance()->StoreCert(
      ssl_info.cert, tab()->render_view_host()->process()->GetID());

  entry->ssl().set_security_style(
      content::SECURITY_STYLE_AUTHENTICATION_BROKEN);
  entry->ssl().set_cert_id(cert_id);
  entry->ssl().set_cert_status(ssl_info.cert_status);
  entry->ssl().set_security_bits(ssl_info.security_bits);
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_SSL_VISIBLE_STATE_CHANGED,
      content::Source<NavigationController>(&tab()->controller()),
      content::NotificationService::NoDetails());
}

void SSLBlockingPage::CommandReceived(const std::string& command) {
  if (command == "1") {
    Proceed();
  } else {
    DontProceed();
  }
}

void SSLBlockingPage::Proceed() {
  RecordSSLBlockingPageStats(PROCEED);

  // Accepting the certificate resumes the loading of the page.
  NotifyAllowCertificate();

  // This call hides and deletes the interstitial.
  InterstitialPage::Proceed();
}

void SSLBlockingPage::DontProceed() {
  RecordSSLBlockingPageStats(DONT_PROCEED);

  NotifyDenyCertificate();
  InterstitialPage::DontProceed();
}

void SSLBlockingPage::NotifyDenyCertificate() {
  // It's possible that callback_ may not exist if the user clicks "Proceed"
  // followed by pressing the back button before the interstitial is hidden.
  // In that case the certificate will still be treated as allowed.
  if (callback_.is_null())
    return;

  callback_.Run(handler_, false);
  callback_.Reset();
}

void SSLBlockingPage::NotifyAllowCertificate() {
  DCHECK(!callback_.is_null());

  callback_.Run(handler_, true);
  callback_.Reset();
}

// static
void SSLBlockingPage::SetExtraInfo(
    DictionaryValue* strings,
    const std::vector<string16>& extra_info) {
  DCHECK(extra_info.size() < 5);  // We allow 5 paragraphs max.
  const char* keys[5] = {
      "moreInfo1", "moreInfo2", "moreInfo3", "moreInfo4", "moreInfo5"
  };
  int i;
  for (i = 0; i < static_cast<int>(extra_info.size()); i++) {
    strings->SetString(keys[i], extra_info[i]);
  }
  for (; i < 5; i++) {
    strings->SetString(keys[i], "");
  }
}
