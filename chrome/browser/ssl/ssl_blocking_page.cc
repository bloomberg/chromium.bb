// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_blocking_page.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/chrome_controller_client.h"
#include "chrome/browser/interstitials/chrome_metrics_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/signed_certificate_timestamp_store.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/ssl_status.h"
#include "net/base/net_errors.h"

using base::TimeTicks;
using content::InterstitialPage;
using content::InterstitialPageDelegate;
using content::NavigationEntry;
using security_interstitials::SSLErrorUI;

namespace {

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
const char kDeprecatedSSLRapporPrefix[] = "ssl2";
const char kSSLRapporPrefix[] = "ssl3";

std::string GetSamplingEventName(const bool overridable, const int cert_error) {
  std::string event_name(kEventNameBase);
  if (overridable)
    event_name.append(kEventOverridable);
  else
    event_name.append(kEventNotOverridable);
  event_name.append(net::ErrorToString(cert_error));
  return event_name;
}

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
SSLBlockingPage::SSLBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const base::Callback<void(bool)>& callback)
    : SecurityInterstitialPage(web_contents, request_url),
      callback_(callback),
      ssl_info_(ssl_info),
      overridable_(IsOverridable(
          options_mask,
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      expired_but_previously_allowed_(
          (options_mask & SSLErrorUI::EXPIRED_BUT_PREVIOUSLY_ALLOWED) != 0),
      controller_(new ChromeControllerClient(web_contents)) {
  // Override prefs for the SSLErrorUI.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile &&
      !profile->GetPrefs()->GetBoolean(prefs::kSSLErrorOverrideAllowed)) {
    options_mask |= SSLErrorUI::HARD_OVERRIDE_DISABLED;
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
  reporting_info.deprecated_rappor_prefix = kDeprecatedSSLRapporPrefix;
  reporting_info.rappor_report_type = rappor::LOW_FREQUENCY_UMA_RAPPOR_TYPE;
  reporting_info.deprecated_rappor_report_type = rappor::UMA_RAPPOR_TYPE;
  ChromeMetricsHelper* chrome_metrics_helper =
      new ChromeMetricsHelper(web_contents, request_url, reporting_info,
                              GetSamplingEventName(overridable_, cert_error));
  chrome_metrics_helper->StartRecordingCaptivePortalMetrics(overridable_);
  controller_->set_metrics_helper(base::WrapUnique(chrome_metrics_helper));

  cert_report_helper_.reset(new CertReportHelper(
      std::move(ssl_cert_reporter), web_contents, request_url, ssl_info,
      certificate_reporting::ErrorReport::INTERSTITIAL_SSL, overridable_,
      controller_->metrics_helper()));

  ssl_error_ui_.reset(new SSLErrorUI(request_url, cert_error, ssl_info,
                                     options_mask, time_triggered,
                                     controller_.get()));

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
  if (!callback_.is_null()) {
    // The page is closed without the user having chosen what to do, default to
    // deny.
    RecordSSLExpirationPageEventState(
        expired_but_previously_allowed_, false, overridable_);
    NotifyDenyCertificate();
  }
}

void SSLBlockingPage::AfterShow() {
  controller_->set_interstitial_page(interstitial_page());
}

void SSLBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  ssl_error_ui_->PopulateStringsForHTML(load_time_data);
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
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter) {
  cert_report_helper_->SetSSLCertReporterForTesting(
      std::move(ssl_cert_reporter));
}

// This handles the commands sent from the interstitial JavaScript.
void SSLBlockingPage::CommandReceived(const std::string& command) {
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

void SSLBlockingPage::OverrideRendererPrefs(
      content::RendererPreferences* prefs) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(
      prefs, profile, web_contents());
}

void SSLBlockingPage::OnProceed() {
  // Finish collecting metrics, if the user opted into it.
  cert_report_helper_->FinishCertCollection(
      certificate_reporting::ErrorReport::USER_PROCEEDED);
  RecordSSLExpirationPageEventState(
      expired_but_previously_allowed_, true, overridable_);

  // Accepting the certificate resumes the loading of the page.
  DCHECK(!callback_.is_null());
  callback_.Run(true);
  callback_.Reset();
}

void SSLBlockingPage::OnDontProceed() {
  // Finish collecting metrics, if the user opted into it.
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

// static
bool SSLBlockingPage::IsOverridable(int options_mask,
                                    const Profile* const profile) {
  const bool is_overridable =
      (options_mask & SSLErrorUI::SOFT_OVERRIDE_ENABLED) &&
      !(options_mask & SSLErrorUI::STRICT_ENFORCEMENT) &&
      profile->GetPrefs()->GetBoolean(prefs::kSSLErrorOverrideAllowed);
  return is_overridable;
}
