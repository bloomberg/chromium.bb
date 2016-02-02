// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/bad_clock_blocking_page.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/interstitials/chrome_controller_client.h"
#include "chrome/browser/interstitials/chrome_metrics_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/bad_clock_ui.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/signed_certificate_timestamp_store.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/ssl_status.h"
#include "net/base/net_errors.h"

using content::InterstitialPageDelegate;
using content::NavigationController;
using content::NavigationEntry;

namespace {

const char kMetricsName[] = "bad_clock";

}  // namespace

// static
InterstitialPageDelegate::TypeID BadClockBlockingPage::kTypeForTesting =
    &BadClockBlockingPage::kTypeForTesting;

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
// Creating an interstitial without showing (e.g. from chrome://interstitials)
// it leaks memory, so don't create it here.
BadClockBlockingPage::BadClockBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    const base::Time& time_triggered,
    scoped_ptr<SSLCertReporter> ssl_cert_reporter,
    const base::Callback<void(bool)>& callback)
    : SecurityInterstitialPage(web_contents, request_url),
      callback_(callback),
      ssl_info_(ssl_info),
      time_triggered_(time_triggered),
      controller_(new ChromeControllerClient(web_contents)) {
  // Get the language for the BadClockUI.
  std::string languages;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile)
    languages = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);

  // Set up the metrics helper for the BadClockUI.
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = kMetricsName;
  ChromeMetricsHelper* chrome_metrics_helper = new ChromeMetricsHelper(
      web_contents, request_url, reporting_info, kMetricsName);
  chrome_metrics_helper->StartRecordingCaptivePortalMetrics(false);
  scoped_ptr<security_interstitials::MetricsHelper> metrics_helper(
      chrome_metrics_helper);
  controller_->set_metrics_helper(std::move(metrics_helper));

  cert_report_helper_.reset(new CertReportHelper(
      std::move(ssl_cert_reporter), web_contents, request_url, ssl_info,
      certificate_reporting::ErrorReport::INTERSTITIAL_CLOCK,
      false /* overridable */, controller_->metrics_helper()));

  bad_clock_ui_.reset(new security_interstitials::BadClockUI(
      request_url, cert_error, ssl_info, time_triggered, languages,
      controller_.get()));
}

BadClockBlockingPage::~BadClockBlockingPage() {
  if (!callback_.is_null()) {
    // Deny when the page is closed.
    NotifyDenyCertificate();
  }
}

bool BadClockBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

InterstitialPageDelegate::TypeID BadClockBlockingPage::GetTypeForTesting()
    const {
  return BadClockBlockingPage::kTypeForTesting;
}

void BadClockBlockingPage::AfterShow() {
  controller_->set_interstitial_page(interstitial_page());
}

void BadClockBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  bad_clock_ui_->PopulateStringsForHTML(load_time_data);
  cert_report_helper_->PopulateExtendedReportingOption(load_time_data);
}

void BadClockBlockingPage::OverrideEntry(NavigationEntry* entry) {
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

void BadClockBlockingPage::SetSSLCertReporterForTesting(
    scoped_ptr<SSLCertReporter> ssl_cert_reporter) {
  cert_report_helper_->SetSSLCertReporterForTesting(
      std::move(ssl_cert_reporter));
}

// This handles the commands sent from the interstitial JavaScript.
void BadClockBlockingPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);

  bad_clock_ui_->HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommands>(cmd));
}

void BadClockBlockingPage::OverrideRendererPrefs(
    content::RendererPreferences* prefs) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile,
                                                      web_contents());
}

void BadClockBlockingPage::OnDontProceed() {
  cert_report_helper_->FinishCertCollection(
      certificate_reporting::ErrorReport::USER_DID_NOT_PROCEED);
  NotifyDenyCertificate();
}

void BadClockBlockingPage::NotifyDenyCertificate() {
  // It's possible that callback_ may not exist if the user clicks "Proceed"
  // followed by pressing the back button before the interstitial is hidden.
  // In that case the certificate will still be treated as allowed.
  if (callback_.is_null())
    return;

  base::ResetAndReturn(&callback_).Run(false);
}
