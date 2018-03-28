// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/mitm_software_blocking_page.h"

#include <utility>

#include "base/callback_helpers.h"
#include "chrome/browser/interstitials/chrome_metrics_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/browser/ssl/ssl_error_controller_client.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/mitm_software_ui.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "net/base/net_errors.h"

using content::InterstitialPageDelegate;
using content::NavigationController;
using content::NavigationEntry;

namespace {

const char kMitmSoftwareMetricsName[] = "mitm_software";

std::unique_ptr<ChromeMetricsHelper> CreateMitmSoftwareMetricsHelper(
    content::WebContents* web_contents,
    const GURL& request_url) {
  // Set up the metrics helper for the MITMSoftwareUI.
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = kMitmSoftwareMetricsName;
  std::unique_ptr<ChromeMetricsHelper> metrics_helper =
      std::make_unique<ChromeMetricsHelper>(web_contents, request_url,
                                            reporting_info);
  metrics_helper.get()->StartRecordingCaptivePortalMetrics(false);
  return metrics_helper;
}

}  // namespace

// static
const InterstitialPageDelegate::TypeID
    MITMSoftwareBlockingPage::kTypeForTesting =
        &MITMSoftwareBlockingPage::kTypeForTesting;

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
// Creating an interstitial without showing (e.g. from chrome://interstitials)
// it leaks memory, so don't create it here.
MITMSoftwareBlockingPage::MITMSoftwareBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const net::SSLInfo& ssl_info,
    const std::string& mitm_software_name,
    bool is_enterprise_managed,
    const base::Callback<void(content::CertificateRequestResultType)>& callback)
    : SSLBlockingPageBase(
          web_contents,
          CertificateErrorReport::INTERSTITIAL_MITM_SOFTWARE,
          ssl_info,
          request_url,
          std::move(ssl_cert_reporter),
          false /* overridable */,
          base::Time::Now(),
          std::make_unique<SSLErrorControllerClient>(
              web_contents,
              ssl_info,
              request_url,
              CreateMitmSoftwareMetricsHelper(web_contents, request_url))),
      callback_(callback),
      ssl_info_(ssl_info),
      mitm_software_ui_(
          new security_interstitials::MITMSoftwareUI(request_url,
                                                     cert_error,
                                                     ssl_info,
                                                     mitm_software_name,
                                                     is_enterprise_managed,
                                                     controller())) {}

MITMSoftwareBlockingPage::~MITMSoftwareBlockingPage() {
  if (!callback_.is_null()) {
    // Deny when the page is closed.
    NotifyDenyCertificate();
  }
}

bool MITMSoftwareBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

InterstitialPageDelegate::TypeID MITMSoftwareBlockingPage::GetTypeForTesting()
    const {
  return MITMSoftwareBlockingPage::kTypeForTesting;
}

void MITMSoftwareBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  mitm_software_ui_->PopulateStringsForHTML(load_time_data);
  cert_report_helper()->PopulateExtendedReportingOption(load_time_data);
}

void MITMSoftwareBlockingPage::OverrideEntry(NavigationEntry* entry) {
  entry->GetSSL() = content::SSLStatus(ssl_info_);
}

// This handles the commands sent from the interstitial JavaScript.
void MITMSoftwareBlockingPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);

  // Let the CertReportHelper handle commands first, This allows it to get set
  // up to send reports, so that the report is populated properly if
  // MITMSoftwareUI's command handling triggers a report to be sent.
  cert_report_helper()->HandleReportingCommands(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd),
      controller()->GetPrefService());
  mitm_software_ui_->HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd));
}

void MITMSoftwareBlockingPage::OverrideRendererPrefs(
    content::RendererPreferences* prefs) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile,
                                                      web_contents());
}

void MITMSoftwareBlockingPage::OnDontProceed() {
  OnInterstitialClosing();
  NotifyDenyCertificate();
}

void MITMSoftwareBlockingPage::NotifyDenyCertificate() {
  // It's possible that callback_ may not exist if the user clicks "Proceed"
  // followed by pressing the back button before the interstitial is hidden.
  // In that case the certificate will still be treated as allowed.
  if (callback_.is_null()) {
    return;
  }

  base::ResetAndReturn(&callback_)
      .Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
}
