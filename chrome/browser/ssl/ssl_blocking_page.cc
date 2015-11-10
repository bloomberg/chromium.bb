// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_blocking_page.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/interstitials/chrome_metrics_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "components/ssl_errors/error_classification.h"
#include "components/ssl_errors/error_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/signed_certificate_timestamp_store.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/ssl_status.h"
#include "grit/browser_resources.h"
#include "grit/components_strings.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;
using base::TimeTicks;
using content::InterstitialPage;
using content::InterstitialPageDelegate;
using content::NavigationController;
using content::NavigationEntry;

namespace {

// URL for help page.
const char kHelpURL[] = "https://support.google.com/chrome/answer/4454607";

// Constants for the Experience Sampling instrumentation.
const char kEventNameBase[] = "ssl_interstitial_";
const char kEventNotOverridable[] = "notoverridable_";
const char kEventOverridable[] = "overridable_";

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
        "interstitial.ssl.expiration_and_decision.overridable",
        event,
        END_OF_SSL_EXPIRATION_AND_DECISION);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "interstitial.ssl.expiration_and_decision.nonoverridable",
        event,
        END_OF_SSL_EXPIRATION_AND_DECISION);
  }
}

}  // namespace

// static
InterstitialPageDelegate::TypeID SSLBlockingPage::kTypeForTesting =
    &SSLBlockingPage::kTypeForTesting;

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
SSLBlockingPage::SSLBlockingPage(content::WebContents* web_contents,
                                 int cert_error,
                                 const net::SSLInfo& ssl_info,
                                 const GURL& request_url,
                                 int options_mask,
                                 const base::Time& time_triggered,
                                 scoped_ptr<SSLCertReporter> ssl_cert_reporter,
                                 const base::Callback<void(bool)>& callback)
    : SecurityInterstitialPage(web_contents, request_url),
      callback_(callback),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      overridable_(IsOverridable(
          options_mask,
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      danger_overridable_(DoesPolicyAllowDangerOverride(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      strict_enforcement_((options_mask & STRICT_ENFORCEMENT) != 0),
      expired_but_previously_allowed_(
          (options_mask & EXPIRED_BUT_PREVIOUSLY_ALLOWED) != 0),
      time_triggered_(time_triggered) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = GetUmaHistogramPrefix();
  reporting_info.rappor_prefix = kSSLRapporPrefix;
  reporting_info.rappor_report_type = rappor::UMA_RAPPOR_TYPE;
  scoped_ptr<ChromeMetricsHelper> chrome_metrics_helper(new ChromeMetricsHelper(
      web_contents, request_url, reporting_info, GetSamplingEventName()));
  chrome_metrics_helper->StartRecordingCaptivePortalMetrics(overridable_);
  set_metrics_helper(chrome_metrics_helper.Pass());
  metrics_helper()->RecordUserDecision(
      security_interstitials::MetricsHelper::SHOW);
  metrics_helper()->RecordUserInteraction(
      security_interstitials::MetricsHelper::TOTAL_VISITS);

  cert_report_helper_.reset(new CertReportHelper(
      ssl_cert_reporter.Pass(), web_contents, request_url, ssl_info,
      certificate_reporting::ErrorReport::INTERSTITIAL_SSL, overridable_,
      metrics_helper()));

  ssl_errors::RecordUMAStatistics(overridable_, time_triggered_, request_url,
                                  cert_error_, *ssl_info_.cert.get());

  // Creating an interstitial without showing (e.g. from chrome://interstitials)
  // it leaks memory, so don't create it here.
}

bool SSLBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

InterstitialPageDelegate::TypeID SSLBlockingPage::GetTypeForTesting() const {
  return SSLBlockingPage::kTypeForTesting;
}

SSLBlockingPage::~SSLBlockingPage() {
  metrics_helper()->RecordShutdownMetrics();
  if (!callback_.is_null()) {
    // The page is closed without the user having chosen what to do, default to
    // deny.
    metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
    RecordSSLExpirationPageEventState(
        expired_but_previously_allowed_, false, overridable_);
    NotifyDenyCertificate();
  }
}

void SSLBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  CHECK(load_time_data);
  base::string16 url(GetFormattedHostName());
  // Shared values for both the overridable and non-overridable versions.
  load_time_data->SetString("type", "SSL");
  load_time_data->SetBoolean("bad_clock", false);

  // Shared UI configuration for all SSL interstitials.
  load_time_data->SetString("errorCode", net::ErrorToString(cert_error_));
  load_time_data->SetString(
      "openDetails",
      l10n_util::GetStringUTF16(IDS_SSL_OPEN_DETAILS_BUTTON));
  load_time_data->SetString(
      "closeDetails",
      l10n_util::GetStringUTF16(IDS_SSL_CLOSE_DETAILS_BUTTON));

  load_time_data->SetString("tabTitle",
                            l10n_util::GetStringUTF16(IDS_SSL_V2_TITLE));
  load_time_data->SetString("heading",
                            l10n_util::GetStringUTF16(IDS_SSL_V2_HEADING));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(IDS_SSL_V2_PRIMARY_PARAGRAPH, url));

  if (overridable_) {
    load_time_data->SetBoolean("overridable", true);

    ssl_errors::ErrorInfo error_info = ssl_errors::ErrorInfo::CreateError(
        ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error_),
        ssl_info_.cert.get(), request_url());
    load_time_data->SetString("explanationParagraph", error_info.details());
    load_time_data->SetString(
        "primaryButtonText",
        l10n_util::GetStringUTF16(IDS_SSL_OVERRIDABLE_SAFETY_BUTTON));
    load_time_data->SetString(
        "finalParagraph",
        l10n_util::GetStringFUTF16(IDS_SSL_OVERRIDABLE_PROCEED_PARAGRAPH, url));
  } else {
    load_time_data->SetBoolean("overridable", false);

    ssl_errors::ErrorInfo::ErrorType type =
        ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error_);
    load_time_data->SetString(
        "explanationParagraph",
        l10n_util::GetStringFUTF16(IDS_SSL_NONOVERRIDABLE_MORE, url));
    load_time_data->SetString("primaryButtonText",
                              l10n_util::GetStringUTF16(IDS_SSL_RELOAD));
    // Customize the help link depending on the specific error type.
    // Only mark as HSTS if none of the more specific error types apply,
    // and use INVALID as a fallback if no other string is appropriate.
    load_time_data->SetInteger("errorType", type);
    int help_string = IDS_SSL_NONOVERRIDABLE_INVALID;
    switch (type) {
      case ssl_errors::ErrorInfo::CERT_REVOKED:
        help_string = IDS_SSL_NONOVERRIDABLE_REVOKED;
        break;
      case ssl_errors::ErrorInfo::CERT_PINNED_KEY_MISSING:
        help_string = IDS_SSL_NONOVERRIDABLE_PINNED;
        break;
      case ssl_errors::ErrorInfo::CERT_INVALID:
        help_string = IDS_SSL_NONOVERRIDABLE_INVALID;
        break;
      default:
        if (strict_enforcement_)
          help_string = IDS_SSL_NONOVERRIDABLE_HSTS;
    }
    load_time_data->SetString("finalParagraph",
                              l10n_util::GetStringFUTF16(help_string, url));
  }

  // Set debugging information at the bottom of the warning.
  load_time_data->SetString(
      "subject", ssl_info_.cert->subject().GetDisplayName());
  load_time_data->SetString(
      "issuer", ssl_info_.cert->issuer().GetDisplayName());
  load_time_data->SetString(
      "expirationDate",
      base::TimeFormatShortDate(ssl_info_.cert->valid_expiry()));
  load_time_data->SetString(
      "currentDate", base::TimeFormatShortDate(time_triggered_));
  std::vector<std::string> encoded_chain;
  ssl_info_.cert->GetPEMEncodedChain(
      &encoded_chain);
  load_time_data->SetString(
      "pem", base::JoinString(encoded_chain, base::StringPiece()));

  cert_report_helper_->PopulateExtendedReportingOption(load_time_data);
}

void SSLBlockingPage::OverrideEntry(NavigationEntry* entry) {
  const int process_id = web_contents()->GetRenderProcessHost()->GetID();
  const int cert_id = content::CertStore::GetInstance()->StoreCert(
      ssl_info_.cert.get(), process_id);
  DCHECK(cert_id);

  content::SignedCertificateTimestampStore* sct_store(
      content::SignedCertificateTimestampStore::GetInstance());
  content::SignedCertificateTimestampIDStatusList sct_ids;
  for (const auto& sct_and_status : ssl_info_.signed_certificate_timestamps) {
    const int sct_id(sct_store->Store(sct_and_status.sct.get(), process_id));
    DCHECK(sct_id);
    sct_ids.push_back(content::SignedCertificateTimestampIDAndStatus(
        sct_id, sct_and_status.status));
  }

  entry->GetSSL() =
      content::SSLStatus(content::SECURITY_STYLE_AUTHENTICATION_BROKEN, cert_id,
                         sct_ids, ssl_info_);
}

void SSLBlockingPage::SetSSLCertReporterForTesting(
    scoped_ptr<SSLCertReporter> ssl_cert_reporter) {
  cert_report_helper_->SetSSLCertReporterForTesting(ssl_cert_reporter.Pass());
}

// This handles the commands sent from the interstitial JavaScript.
// DO NOT reorder or change this logic without also changing the JavaScript!
void SSLBlockingPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);
  switch (cmd) {
    case CMD_DONT_PROCEED: {
      interstitial_page()->DontProceed();
      break;
    }
    case CMD_PROCEED: {
      if (danger_overridable_) {
        interstitial_page()->Proceed();
      }
      break;
    }
    case CMD_DO_REPORT: {
      SetReportingPreference(true);
      break;
    }
    case CMD_DONT_REPORT: {
      SetReportingPreference(false);
      break;
    }
    case CMD_SHOW_MORE_SECTION: {
      metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_ADVANCED);
      break;
    }
    case CMD_OPEN_HELP_CENTER: {
      metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_LEARN_MORE);
      content::NavigationController::LoadURLParams help_page_params(
          google_util::AppendGoogleLocaleParam(
              GURL(kHelpURL), g_browser_process->GetApplicationLocale()));
      web_contents()->GetController().LoadURLWithParams(help_page_params);
      break;
    }
    case CMD_RELOAD: {
      metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::RELOAD);
      // The interstitial can't refresh itself.
      web_contents()->GetController().Reload(true);
      break;
    }
    case CMD_OPEN_REPORTING_PRIVACY:
      OpenExtendedReportingPrivacyPolicy();
      break;
    case CMD_OPEN_DATE_SETTINGS:
    case CMD_OPEN_DIAGNOSTIC:
      // Commands not supported by the SSL interstitial.
      NOTREACHED() << "Unexpected command: " << command;
  }
}

void SSLBlockingPage::OverrideRendererPrefs(
      content::RendererPreferences* prefs) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(
      prefs, profile, web_contents());
}

void SSLBlockingPage::OnProceed() {
  metrics_helper()->RecordUserDecision(
      security_interstitials::MetricsHelper::PROCEED);

  // Finish collecting information about invalid certificates, if the
  // user opted in to.
  cert_report_helper_->FinishCertCollection(
      certificate_reporting::ErrorReport::USER_PROCEEDED);

  RecordSSLExpirationPageEventState(
      expired_but_previously_allowed_, true, overridable_);
  // Accepting the certificate resumes the loading of the page.
  NotifyAllowCertificate();
}

void SSLBlockingPage::OnDontProceed() {
  metrics_helper()->RecordUserDecision(
      security_interstitials::MetricsHelper::DONT_PROCEED);

  // Finish collecting information about invalid certificates, if the
  // user opted in to.
  cert_report_helper_->FinishCertCollection(
      certificate_reporting::ErrorReport::USER_DID_NOT_PROCEED);

  RecordSSLExpirationPageEventState(
      expired_but_previously_allowed_, false, overridable_);
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

std::string SSLBlockingPage::GetUmaHistogramPrefix() const {
  return overridable_ ? "ssl_overridable" : "ssl_nonoverridable";
}

std::string SSLBlockingPage::GetSamplingEventName() const {
  std::string event_name(kEventNameBase);
  if (overridable_)
    event_name.append(kEventOverridable);
  else
    event_name.append(kEventNotOverridable);
  event_name.append(net::ErrorToString(cert_error_));
  return event_name;
}

// static
bool SSLBlockingPage::IsOverridable(int options_mask,
                                    const Profile* const profile) {
  const bool is_overridable =
      (options_mask & SSLBlockingPage::OVERRIDABLE) &&
      !(options_mask & SSLBlockingPage::STRICT_ENFORCEMENT) &&
      profile->GetPrefs()->GetBoolean(prefs::kSSLErrorOverrideAllowed);
  return is_overridable;
}

// static
bool SSLBlockingPage::DoesPolicyAllowDangerOverride(
    const Profile* const profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kSSLErrorOverrideAllowed);
}
