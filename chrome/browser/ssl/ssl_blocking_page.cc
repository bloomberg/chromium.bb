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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
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
#include "ui/webui/jstemplate_builder.h"

using base::TimeDelta;
using base::TimeTicks;
using content::InterstitialPage;
using content::NavigationController;
using content::NavigationEntry;

#define HISTOGRAM_INTERSTITIAL_SMALL_TIME(name, sample) \
    UMA_HISTOGRAM_CUSTOM_TIMES( \
        name, \
        sample, \
        base::TimeDelta::FromMilliseconds(400), \
        base::TimeDelta::FromMinutes(15), 75);

#define HISTOGRAM_INTERSTITIAL_LARGE_TIME(name, sample) \
    UMA_HISTOGRAM_CUSTOM_TIMES( \
        name, \
        sample, \
        base::TimeDelta::FromMilliseconds(400), \
        base::TimeDelta::FromMinutes(20), 50);

namespace {

// These represent the commands sent by ssl_roadblock.html.
enum SSLBlockingPageCommands {
  CMD_DONT_PROCEED,
  CMD_PROCEED,
  CMD_FOCUS,
  CMD_MORE,
};

// Events for UMA.
enum SSLBlockingPageEvent {
  SHOW_ALL,
  SHOW_OVERRIDABLE,
  PROCEED_OVERRIDABLE,
  PROCEED_NAME,
  PROCEED_DATE,
  PROCEED_AUTHORITY,
  DONT_PROCEED_OVERRIDABLE,
  DONT_PROCEED_NAME,
  DONT_PROCEED_DATE,
  DONT_PROCEED_AUTHORITY,
  MORE,
  UNUSED_BLOCKING_PAGE_EVENT,
};

void RecordSSLBlockingPageEventStats(SSLBlockingPageEvent event) {
  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl",
                            event,
                            UNUSED_BLOCKING_PAGE_EVENT);
}

void RecordSSLBlockingPageTimeStats(
    bool proceed,
    int cert_error,
    bool overridable,
    const base::TimeTicks& start_time,
    const base::TimeTicks& end_time) {
  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl_error_type",
     SSLErrorInfo::NetErrorToErrorType(cert_error), SSLErrorInfo::END_OF_ENUM);
  if (start_time.is_null() || !overridable) {
    // A null start time will occur if the page never came into focus and the
    // user quit without seeing it. If so, we don't record the time.
    // The user might not have an option except to turn back; that happens
    // if overridable is true.  If so, the time/outcome isn't meaningful.
    return;
  }
  base::TimeDelta delta = end_time - start_time;
  if (proceed) {
    RecordSSLBlockingPageEventStats(PROCEED_OVERRIDABLE);
    HISTOGRAM_INTERSTITIAL_LARGE_TIME("interstitial.ssl_accept_time", delta);
  } else if (!proceed) {
    RecordSSLBlockingPageEventStats(DONT_PROCEED_OVERRIDABLE);
    HISTOGRAM_INTERSTITIAL_LARGE_TIME("interstitial.ssl_reject_time", delta);
  }
  SSLErrorInfo::ErrorType type = SSLErrorInfo::NetErrorToErrorType(cert_error);
  switch (type) {
    case SSLErrorInfo::CERT_COMMON_NAME_INVALID: {
      HISTOGRAM_INTERSTITIAL_SMALL_TIME(
          "interstitial.common_name_invalid_time",
          delta);
      if (proceed)
        RecordSSLBlockingPageEventStats(PROCEED_NAME);
      else
        RecordSSLBlockingPageEventStats(DONT_PROCEED_NAME);
      break;
    }
    case SSLErrorInfo::CERT_DATE_INVALID: {
      HISTOGRAM_INTERSTITIAL_SMALL_TIME(
          "interstitial.date_invalid_time",
          delta);
      if (proceed)
        RecordSSLBlockingPageEventStats(PROCEED_DATE);
      else
        RecordSSLBlockingPageEventStats(DONT_PROCEED_DATE);
      break;
    }
    case SSLErrorInfo::CERT_AUTHORITY_INVALID: {
      HISTOGRAM_INTERSTITIAL_SMALL_TIME(
          "interstitial.authority_invalid_time",
          delta);
      if (proceed)
        RecordSSLBlockingPageEventStats(PROCEED_AUTHORITY);
      else
        RecordSSLBlockingPageEventStats(DONT_PROCEED_AUTHORITY);
      break;
    }
    default: {
      break;
    }
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
  RecordSSLBlockingPageEventStats(SHOW_ALL);
  if (overridable_ && !strict_enforcement_)
    RecordSSLBlockingPageEventStats(SHOW_OVERRIDABLE);

  interstitial_page_ = InterstitialPage::Create(
      web_contents_, true, request_url, this);
  display_start_time_ = TimeTicks();
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

  strings.SetString("exit",
                    l10n_util::GetStringUTF16(IDS_SSL_BLOCKING_PAGE_EXIT));

  int resource_id = IDR_SSL_ROAD_BLOCK_HTML;
  if (overridable_ && !strict_enforcement_) {
    strings.SetString("title",
                      l10n_util::GetStringUTF16(IDS_SSL_BLOCKING_PAGE_TITLE));
    strings.SetString("proceed",
                      l10n_util::GetStringUTF16(IDS_SSL_BLOCKING_PAGE_PROCEED));
    strings.SetString("reasonForNotProceeding",
                      l10n_util::GetStringUTF16(
                          IDS_SSL_BLOCKING_PAGE_SHOULD_NOT_PROCEED));
    // The value of errorType doesn't matter; we actually just check if it's
    // empty or not in ssl_roadblock.
    strings.SetString("errorType",
                      l10n_util::GetStringUTF16(IDS_SSL_BLOCKING_PAGE_TITLE));
  } else {
    strings.SetString("title",
                      l10n_util::GetStringUTF16(IDS_SSL_ERROR_PAGE_TITLE));
    if (strict_enforcement_) {
      strings.SetString("reasonForNotProceeding",
                        l10n_util::GetStringUTF16(
                            IDS_SSL_ERROR_PAGE_CANNOT_PROCEED));
    } else {
      strings.SetString("reasonForNotProceeding", "");
    }
    strings.SetString("errorType", "");
  }

  strings.SetString("textdirection", base::i18n::IsRTL() ? "rtl" : "ltr");

  base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          resource_id));

  return webui::GetI18nTemplateHtml(html, &strings);
}

void SSLBlockingPage::OverrideEntry(NavigationEntry* entry) {
  int cert_id = content::CertStore::GetInstance()->StoreCert(
      ssl_info_.cert, web_contents_->GetRenderProcessHost()->GetID());

  entry->GetSSL().security_style =
      content::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  entry->GetSSL().cert_id = cert_id;
  entry->GetSSL().cert_status = ssl_info_.cert_status;
  entry->GetSSL().security_bits = ssl_info_.security_bits;
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (browser)
    browser->VisibleSSLStateChanged(web_contents_);
#endif  // !defined(OS_ANDROID)
}

// Matches events defined in ssl_error.html and ssl_roadblock.html.
void SSLBlockingPage::CommandReceived(const std::string& command) {
  int cmd = atoi(command.c_str());
  if (cmd == CMD_DONT_PROCEED) {
    interstitial_page_->DontProceed();
  } else if (cmd == CMD_PROCEED) {
    interstitial_page_->Proceed();
  } else if (cmd == CMD_FOCUS) {
    // Start recording the time when the page is first in focus
    display_start_time_ = base::TimeTicks::Now();
  } else if (cmd == CMD_MORE) {
    RecordSSLBlockingPageEventStats(MORE);
  }
}

void SSLBlockingPage::OverrideRendererPrefs(
      content::RendererPreferences* prefs) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile);
}

void SSLBlockingPage::OnProceed() {
  RecordSSLBlockingPageTimeStats(true, cert_error_,
      overridable_ && !strict_enforcement_, display_start_time_,
      base::TimeTicks::Now());

  // Accepting the certificate resumes the loading of the page.
  NotifyAllowCertificate();
}

void SSLBlockingPage::OnDontProceed() {
  RecordSSLBlockingPageTimeStats(false, cert_error_,
    overridable_ && !strict_enforcement_, display_start_time_,
    base::TimeTicks::Now());

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
