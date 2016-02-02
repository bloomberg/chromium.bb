// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/cert_report_helper.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// Constants for the HTTPSErrorReporter Finch experiment
const char CertReportHelper::kFinchExperimentName[] = "ReportCertificateErrors";
const char CertReportHelper::kFinchGroupShowPossiblySend[] =
    "ShowAndPossiblySend";
const char CertReportHelper::kFinchGroupDontShowDontSend[] =
    "DontShowAndDontSend";
const char CertReportHelper::kFinchParamName[] = "sendingThreshold";

CertReportHelper::CertReportHelper(
    scoped_ptr<SSLCertReporter> ssl_cert_reporter,
    content::WebContents* web_contents,
    const GURL& request_url,
    const net::SSLInfo& ssl_info,
    certificate_reporting::ErrorReport::InterstitialReason interstitial_reason,
    bool overridable,
    security_interstitials::MetricsHelper* metrics_helper)
    : ssl_cert_reporter_(std::move(ssl_cert_reporter)),
      web_contents_(web_contents),
      request_url_(request_url),
      ssl_info_(ssl_info),
      interstitial_reason_(interstitial_reason),
      overridable_(overridable),
      metrics_helper_(metrics_helper) {}

CertReportHelper::~CertReportHelper() {
}

void CertReportHelper::PopulateExtendedReportingOption(
    base::DictionaryValue* load_time_data) {
  // Only show the checkbox if not off-the-record and if this client is
  // part of the respective Finch group, and the feature is not disabled
  // by policy.
  const bool show = ShouldShowCertificateReporterCheckbox();

  load_time_data->SetBoolean(security_interstitials::kDisplayCheckBox, show);
  if (!show)
    return;

  load_time_data->SetBoolean(
      security_interstitials::kBoxChecked,
      IsPrefEnabled(prefs::kSafeBrowsingExtendedReportingEnabled));

  const std::string privacy_link = base::StringPrintf(
      security_interstitials::kPrivacyLinkHtml,
      security_interstitials::CMD_OPEN_REPORTING_PRIVACY,
      l10n_util::GetStringUTF8(IDS_SAFE_BROWSING_PRIVACY_POLICY_PAGE).c_str());

  load_time_data->SetString(
      security_interstitials::kOptInLink,
      l10n_util::GetStringFUTF16(IDS_SAFE_BROWSING_MALWARE_REPORTING_AGREE,
                                 base::UTF8ToUTF16(privacy_link)));
}

void CertReportHelper::FinishCertCollection(
    certificate_reporting::ErrorReport::ProceedDecision user_proceeded) {
  if (!ShouldShowCertificateReporterCheckbox())
    return;

  if (!IsPrefEnabled(prefs::kSafeBrowsingExtendedReportingEnabled))
    return;

  if (metrics_helper_) {
    metrics_helper_->RecordUserInteraction(
        security_interstitials::MetricsHelper::EXTENDED_REPORTING_IS_ENABLED);
  }

  if (!ShouldReportCertificateError())
    return;

  std::string serialized_report;
  certificate_reporting::ErrorReport report(request_url_.host(), ssl_info_);

  report.SetInterstitialInfo(
      interstitial_reason_, user_proceeded,
      overridable_
          ? certificate_reporting::ErrorReport::INTERSTITIAL_OVERRIDABLE
          : certificate_reporting::ErrorReport::INTERSTITIAL_NOT_OVERRIDABLE);

  if (!report.Serialize(&serialized_report)) {
    LOG(ERROR) << "Failed to serialize certificate report.";
    return;
  }

  ssl_cert_reporter_->ReportInvalidCertificateChain(serialized_report);
}

void CertReportHelper::SetSSLCertReporterForTesting(
    scoped_ptr<SSLCertReporter> ssl_cert_reporter) {
  ssl_cert_reporter_ = std::move(ssl_cert_reporter);
}

bool CertReportHelper::ShouldShowCertificateReporterCheckbox() {
#if defined(OS_IOS)
  return false;
#else
  // Only show the checkbox iff the user is part of the respective Finch group
  // and the window is not incognito and the feature is not disabled by policy.
  const bool in_incognito =
      web_contents_->GetBrowserContext()->IsOffTheRecord();
  return base::FieldTrialList::FindFullName(kFinchExperimentName) ==
             kFinchGroupShowPossiblySend &&
         !in_incognito &&
         IsPrefEnabled(prefs::kSafeBrowsingExtendedReportingOptInAllowed);
#endif
}

bool CertReportHelper::ShouldReportCertificateError() {
#if !defined(OS_IOS)
  DCHECK(ShouldShowCertificateReporterCheckbox());
  // Even in case the checkbox was shown, we don't send error reports
  // for all of these users. Check the Finch configuration for a sending
  // threshold and only send reports in case the threshold isn't exceeded.
  const std::string param =
      variations::GetVariationParamValue(kFinchExperimentName, kFinchParamName);
  if (!param.empty()) {
    double sendingThreshold;
    if (base::StringToDouble(param, &sendingThreshold)) {
      if (sendingThreshold >= 0.0 && sendingThreshold <= 1.0)
        return base::RandDouble() <= sendingThreshold;
    }
  }
#endif
  return false;
}

bool CertReportHelper::IsPrefEnabled(const char* pref) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return profile->GetPrefs()->GetBoolean(pref);
}
