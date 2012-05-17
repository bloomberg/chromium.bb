// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_blocking_page.h"

#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/common/jstemplate_builder.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

using content::InterstitialPage;
using content::NavigationController;
using content::NavigationEntry;

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
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool overridable,
    bool strict_enforcement,
    const base::Callback<void(bool)>& callback)
    : callback_(callback),
      web_contents_(web_contents),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      request_url_(request_url),
      overridable_(overridable),
      strict_enforcement_(strict_enforcement) {
  RecordSSLBlockingPageStats(SHOW);
  interstitial_page_ = InterstitialPage::Create(
      web_contents_, true, request_url, this);
  interstitial_page_->Show();
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
      SSLErrorInfo::NetErrorToErrorType(cert_error_), ssl_info_.cert,
      request_url_);

  strings.SetString("headLine", error_info.title());
  strings.SetString("description", error_info.details());

  strings.SetString("moreInfoTitle",
                    l10n_util::GetStringUTF16(IDS_CERT_ERROR_EXTRA_INFO_TITLE));
  SetExtraInfo(&strings, error_info.extra_information());

  int resource_id;
  if (overridable_ && !strict_enforcement_) {
    resource_id = IDR_SSL_ROAD_BLOCK_HTML;
    strings.SetString("title",
                      l10n_util::GetStringUTF16(IDS_SSL_BLOCKING_PAGE_TITLE));
    strings.SetString("proceed",
                      l10n_util::GetStringUTF16(IDS_SSL_BLOCKING_PAGE_PROCEED));
    strings.SetString("exit",
                      l10n_util::GetStringUTF16(IDS_SSL_BLOCKING_PAGE_EXIT));
    strings.SetString("shouldNotProceed",
                      l10n_util::GetStringUTF16(
                          IDS_SSL_BLOCKING_PAGE_SHOULD_NOT_PROCEED));
  } else {
    resource_id = IDR_SSL_ERROR_HTML;
    strings.SetString("title",
                      l10n_util::GetStringUTF16(IDS_SSL_ERROR_PAGE_TITLE));
    strings.SetString("back",
                      l10n_util::GetStringUTF16(IDS_SSL_ERROR_PAGE_BACK));
    if (strict_enforcement_) {
      strings.SetString("cannotProceed",
                        l10n_util::GetStringUTF16(
                            IDS_SSL_ERROR_PAGE_CANNOT_PROCEED));
    }
  }

  strings.SetString("textdirection", base::i18n::IsRTL() ? "rtl" : "ltr");

  base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          resource_id, ui::SCALE_FACTOR_NONE));

  return jstemplate_builder::GetI18nTemplateHtml(html, &strings);
}

void SSLBlockingPage::OverrideEntry(NavigationEntry* entry) {
  int cert_id = content::CertStore::GetInstance()->StoreCert(
      ssl_info_.cert, web_contents_->GetRenderProcessHost()->GetID());

  entry->GetSSL().security_style =
      content::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  entry->GetSSL().cert_id = cert_id;
  entry->GetSSL().cert_status = ssl_info_.cert_status;
  entry->GetSSL().security_bits = ssl_info_.security_bits;
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_SSL_VISIBLE_STATE_CHANGED,
      content::Source<NavigationController>(&web_contents_->GetController()),
      content::NotificationService::NoDetails());
}

void SSLBlockingPage::CommandReceived(const std::string& command) {
  if (command == "1") {
    interstitial_page_->Proceed();
  } else {
    interstitial_page_->DontProceed();
  }
}

void SSLBlockingPage::OverrideRendererPrefs(
      content::RendererPreferences* prefs) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile);
}

void SSLBlockingPage::OnProceed() {
  RecordSSLBlockingPageStats(PROCEED);

  // Accepting the certificate resumes the loading of the page.
  NotifyAllowCertificate();
}

void SSLBlockingPage::OnDontProceed() {
  RecordSSLBlockingPageStats(DONT_PROCEED);

  NotifyDenyCertificate();
}

void SSLBlockingPage::NotifyDenyCertificate() {
  // It's possible that callback_ may not exist if the user clicks "Proceed"
  // followed by pressing the back button before the interstitial is hidden.
  // In that case the certificate will still be treated as allowed.
  if (callback_.is_null())
    return;

  callback_.Run(false);
  callback_.Reset();
}

void SSLBlockingPage::NotifyAllowCertificate() {
  DCHECK(!callback_.is_null());

  callback_.Run(true);
  callback_.Reset();
}

// static
void SSLBlockingPage::SetExtraInfo(
    DictionaryValue* strings,
    const std::vector<string16>& extra_info) {
  DCHECK_LT(extra_info.size(), 5U);  // We allow 5 paragraphs max.
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
