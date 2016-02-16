// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ssl/ios_ssl_blocking_page.h"

#include <utility>

#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/interstitials/ios_chrome_controller_client.h"
#include "ios/chrome/browser/interstitials/ios_chrome_metrics_helper.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/browser_constants.h"
#include "ios/web/public/web_state/web_state.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using security_interstitials::SSLErrorUI;

namespace {
// Events for UMA. Do not reorder or change!
enum SSLExpirationAndDecision {
  EXPIRED_AND_PROCEED,
  EXPIRED_AND_DO_NOT_PROCEED,
  NOT_EXPIRED_AND_PROCEED,
  NOT_EXPIRED_AND_DO_NOT_PROCEED,
  END_OF_SSL_EXPIRATION_AND_DECISION,
};

// Rappor prefix, which is used for both overridable and non-overridable
// interstitials so we don't leak the "overridable" bit.
const char kSSLRapporPrefix[] = "ssl2";

void RecordSSLExpirationPageEventState(bool expired_but_previously_allowed,
                                       bool proceed,
                                       bool overridable) {
  SSLExpirationAndDecision event;
  if (expired_but_previously_allowed && proceed)
    event = EXPIRED_AND_PROCEED;
  else if (expired_but_previously_allowed && !proceed)
    event = EXPIRED_AND_DO_NOT_PROCEED;
  else if (!expired_but_previously_allowed && proceed)
    event = NOT_EXPIRED_AND_PROCEED;
  else
    event = NOT_EXPIRED_AND_DO_NOT_PROCEED;

  if (overridable) {
    UMA_HISTOGRAM_ENUMERATION(
        "interstitial.ssl.expiration_and_decision.overridable", event,
        END_OF_SSL_EXPIRATION_AND_DECISION);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "interstitial.ssl.expiration_and_decision.nonoverridable", event,
        END_OF_SSL_EXPIRATION_AND_DECISION);
  }
}
}  // namespace

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
IOSSSLBlockingPage::IOSSSLBlockingPage(
    web::WebState* web_state,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    const base::Callback<void(bool)>& callback)
    : IOSSecurityInterstitialPage(web_state, request_url),
      callback_(callback),
      ssl_info_(ssl_info),
      overridable_(IsOverridable(options_mask)),
      expired_but_previously_allowed_(
          (options_mask & SSLErrorUI::EXPIRED_BUT_PREVIOUSLY_ALLOWED) != 0),
      controller_(new IOSChromeControllerClient(web_state)) {
  // Get the language and override prefs for the SSLErrorUI.
  std::string languages;
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());
  if (browser_state) {
    languages = browser_state->GetPrefs()->GetString(prefs::kAcceptLanguages);
  }
  if (overridable_)
    options_mask |= SSLErrorUI::SOFT_OVERRIDE_ENABLED;
  else
    options_mask &= ~SSLErrorUI::SOFT_OVERRIDE_ENABLED;

  // Set up the metrics helper for the SSLErrorUI.
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix =
      overridable_ ? "ssl_overridable" : "ssl_nonoverridable";
  reporting_info.rappor_prefix = kSSLRapporPrefix;
  reporting_info.rappor_report_type = rappor::UMA_RAPPOR_TYPE;
  IOSChromeMetricsHelper* ios_chrome_metrics_helper =
      new IOSChromeMetricsHelper(web_state, request_url, reporting_info);
  controller_->set_metrics_helper(make_scoped_ptr(ios_chrome_metrics_helper));

  ssl_error_ui_.reset(new SSLErrorUI(request_url, cert_error, ssl_info,
                                     options_mask, time_triggered, languages,
                                     controller_.get()));

  // Creating an interstitial without showing (e.g. from chrome://interstitials)
  // it leaks memory, so don't create it here.
}

bool IOSSSLBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

IOSSSLBlockingPage::~IOSSSLBlockingPage() {
  if (!callback_.is_null()) {
    // The page is closed without the user having chosen what to do, default to
    // deny.
    RecordSSLExpirationPageEventState(expired_but_previously_allowed_, false,
                                      overridable_);
    NotifyDenyCertificate();
  }
}

void IOSSSLBlockingPage::AfterShow() {
  controller_->SetWebInterstitial(web_interstitial());
}

void IOSSSLBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) const {
  ssl_error_ui_->PopulateStringsForHTML(load_time_data);
  // Spoofing attempts have a custom message on iOS.
  // This code will no longer be necessary once UIWebView is gone.
  if (ssl_info_.cert->subject().GetDisplayName() == ios::kSpoofingAttemptFlag) {
    load_time_data->SetString(
        "errorCode", base::string16(base::ASCIIToUTF16("Unverified URL")));
    load_time_data->SetString(
        "tabTitle", l10n_util::GetStringUTF16(
                        IDS_IOS_INTERSTITIAL_HEADING_SPOOFING_ATTEMPT_ERROR));
    load_time_data->SetString(
        "heading", l10n_util::GetStringUTF16(
                       IDS_IOS_INTERSTITIAL_HEADING_SPOOFING_ATTEMPT_ERROR));
    load_time_data->SetString(
        "primaryParagraph",
        l10n_util::GetStringUTF16(
            IDS_IOS_INTERSTITIAL_SUMMARY_SPOOFING_ATTEMPT_ERROR));
    load_time_data->SetString(
        "explanationParagraph",
        l10n_util::GetStringUTF16(
            IDS_IOS_INTERSTITIAL_DETAILS_SPOOFING_ATTEMPT_ERROR));
    load_time_data->SetString("finalParagraph", base::string16());
  }
}

// This handles the commands sent from the interstitial JavaScript.
void IOSSSLBlockingPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);
  ssl_error_ui_->HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommands>(cmd));
}

void IOSSSLBlockingPage::OnProceed() {
  RecordSSLExpirationPageEventState(expired_but_previously_allowed_, true,
                                    overridable_);

  // Accepting the certificate resumes the loading of the page.
  DCHECK(!callback_.is_null());
  callback_.Run(true);
  callback_.Reset();
}

void IOSSSLBlockingPage::OnDontProceed() {
  RecordSSLExpirationPageEventState(expired_but_previously_allowed_, false,
                                    overridable_);

  NotifyDenyCertificate();
}

void IOSSSLBlockingPage::NotifyDenyCertificate() {
  // It's possible that callback_ may not exist if the user clicks "Proceed"
  // followed by pressing the back button before the interstitial is hidden.
  // In that case the certificate will still be treated as allowed.
  if (callback_.is_null())
    return;

  callback_.Run(false);
  callback_.Reset();
}

// static
bool IOSSSLBlockingPage::IsOverridable(int options_mask) {
  const bool is_overridable =
      (options_mask & SSLErrorUI::SOFT_OVERRIDE_ENABLED) &&
      !(options_mask & SSLErrorUI::STRICT_ENFORCEMENT);
  return is_overridable;
}
