// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_blocking_page.h"

#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/sha1.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
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
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using base::TimeDelta;
using base::TimeTicks;
using content::InterstitialPage;
using content::NavigationController;
using content::NavigationEntry;

namespace {

// kMalware1 is the SHA1 SPKI hash of a key used by a piece of malware that MS
// Security Essentials identifies as Win32/Sirefef.gen!C.
const uint8 kMalware1[base::kSHA1Length] = {
  0xa4, 0xf5, 0x6e, 0x9e, 0x1d, 0x9a, 0x3b, 0x7b, 0x1a, 0xc3,
  0x31, 0xcf, 0x64, 0xfc, 0x76, 0x2c, 0xd0, 0x51, 0xfb, 0xa4,
};

// IsSpecialCaseCertError returns true if the public key hashes in |ssl_info|
// indicate that this is a special case error. If so, a URL with more
// information will be returned in |out_url| and an (untranslated) message in
// |out_message|.
bool IsSpecialCaseCertError(const net::SSLInfo& ssl_info,
                            std::string* out_url,
                            std::string* out_message) {
  for (net::HashValueVector::const_iterator i =
       ssl_info.public_key_hashes.begin();
       i != ssl_info.public_key_hashes.end(); ++i) {
    if (i->tag != net::HASH_VALUE_SHA1 ||
        0 != memcmp(i->data(), kMalware1, base::kSHA1Length)) {
      continue;
    }

    // In the future this information will come from the CRLSet. Until then
    // this case is hardcoded.
    *out_url = "http://support.google.com/chrome/?p=e_malware_Sirefef";
    *out_message =
      "<p>The certificate received indicates that this computer is infected"
      " with Sirefef.gen!C.</p>"

      "<p>Sirefef.gen!C is a computer virus that intercepts secure web"
      " connections and can steal passwords and other sensitive data.</p>"

      "<p>Chrome recognises this virus, but it affects all software on the"
      " computer. Other browsers and software may continue to work but"
      " they are also affected and rendered insecure.</p>"

      "<p>Microsoft Security Essentials can reportedly remove this virus."
      " When the virus is removed, the warnings in Chrome will stop.</p>"

      "<p>Microsoft Security Essentials is freely available from Microsoft "
      " at "
      "http://windows.microsoft.com/en-US/windows/security-essentials-download";
    return true;
  }

  return false;
}

enum SSLBlockingPageEvent {
  SHOW,
  PROCEED,
  DONT_PROCEED,
  UNUSED_ENUM,
};

void RecordSSLBlockingPageEventStats(SSLBlockingPageEvent event) {
  UMA_HISTOGRAM_ENUMERATION("interstial.ssl", event, UNUSED_ENUM);
}

void RecordSSLBlockingPageTimeStats(
    bool proceed,
    int cert_error,
    const base::TimeTicks& start_time,
    const base::TimeTicks& end_time) {
  UMA_HISTOGRAM_ENUMERATION("interstial.ssl_error_type",
     SSLErrorInfo::NetErrorToErrorType(cert_error), SSLErrorInfo::END_OF_ENUM);
  base::TimeDelta delta = end_time - start_time;
  if (proceed) {
    UMA_HISTOGRAM_CUSTOM_TIMES("interstial.ssl_accept_time", delta,
        base::TimeDelta::FromMilliseconds(30),
        base::TimeDelta::FromMinutes(30), 50);
  } else {
    UMA_HISTOGRAM_CUSTOM_TIMES("interstial.ssl_reject_time", delta,
        base::TimeDelta::FromMilliseconds(30),
        base::TimeDelta::FromMinutes(30), 50);
  }
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
  RecordSSLBlockingPageEventStats(SHOW);
  interstitial_page_ = InterstitialPage::Create(
      web_contents_, true, request_url, this);
  display_start_time_ = base::TimeTicks::Now();
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

  // ERR_CERT_REVOKED handles both online (OCSP, CRL) and offline (CRLSet)
  // revocation. If the certificate was revoked for being in a CRLSet, see if
  // there is a user-friendly error message or link to direct them to that may
  // explain why it was revoked. In the future, these messages will be
  // contained within the CRLSet itself and they will be loaded from there, but
  // for now, this is a hardcoded list.
  std::string url, message;
  if (cert_error_ == net::ERR_CERT_REVOKED &&
      IsSpecialCaseCertError(ssl_info_, &url, &message)) {
    strings.SetString("headLine", l10n_util::GetStringUTF16(
        IDS_CERT_ERROR_SPECIAL_CASE_TITLE));

    string16 details = l10n_util::GetStringFUTF16(
        IDS_CERT_ERROR_SPECIAL_CASE_DETAILS,
        UTF8ToUTF16(google_util::StringAppendGoogleLocaleParam(url)));
    details += UTF8ToUTF16("<br><br>") + UTF8ToUTF16(message);
    strings.SetString("description", details);

    // If this is the only error for the site, then the user can override.
    if ((ssl_info_.cert_status & net::CERT_STATUS_ALL_ERRORS) ==
        net::CERT_STATUS_REVOKED) {
      overridable_ = true;
      strict_enforcement_ = false;
    }
  } else {
    SSLErrorInfo error_info = SSLErrorInfo::CreateError(
        SSLErrorInfo::NetErrorToErrorType(cert_error_), ssl_info_.cert,
        request_url_);

    strings.SetString("headLine", error_info.title());
    strings.SetString("description", error_info.details());

    SetExtraInfo(&strings, error_info.extra_information());
  }

  strings.SetString("moreInfoTitle",
                    l10n_util::GetStringUTF16(IDS_CERT_ERROR_EXTRA_INFO_TITLE));

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
          resource_id));

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
  RecordSSLBlockingPageTimeStats(true, cert_error_, display_start_time_,
      base::TimeTicks::Now());
  RecordSSLBlockingPageEventStats(PROCEED);

  // Accepting the certificate resumes the loading of the page.
  NotifyAllowCertificate();
}

void SSLBlockingPage::OnDontProceed() {
  RecordSSLBlockingPageTimeStats(false, cert_error_, display_start_time_,
      base::TimeTicks::Now());
  RecordSSLBlockingPageEventStats(DONT_PROCEED);

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
