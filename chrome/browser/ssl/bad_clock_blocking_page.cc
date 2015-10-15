// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/bad_clock_blocking_page.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/build_time.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/chrome_metrics_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/browser/ssl/ssl_error_classification.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "content/public/browser/browser_thread.h"
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
#include "grit/browser_resources.h"
#include "grit/components_strings.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/intent_helper.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#endif

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/win/windows_version.h"
#endif

using base::ASCIIToUTF16;
using base::TimeTicks;
using content::InterstitialPage;
using content::InterstitialPageDelegate;
using content::NavigationController;
using content::NavigationEntry;

namespace {

const char kMetricsName[] = "bad_clock";

void LaunchDateAndTimeSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
// The code for each OS is completely separate, in order to avoid bugs like
// https://crbug.com/430877 .
#if defined(OS_ANDROID)
  chrome::android::OpenDateAndTimeSettings();

#elif defined(OS_CHROMEOS)
  std::string sub_page =
      std::string(chrome::kSearchSubPage) + "#" +
      l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME);
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        sub_page);

#elif defined(OS_IOS)
  // iOS does not have a way to launch the date and time settings.
  NOTREACHED();

#elif defined(OS_LINUX)
  struct ClockCommand {
    const char* pathname;
    const char* argument;
  };
  static const ClockCommand kClockCommands[] = {
      // Unity
      {"/usr/bin/unity-control-center", "datetime"},
      // GNOME
      //
      // NOTE: On old Ubuntu, naming control panels doesn't work, so it
      // opens the overview. This will have to be good enough.
      {"/usr/bin/gnome-control-center", "datetime"},
      {"/usr/local/bin/gnome-control-center", "datetime"},
      {"/opt/bin/gnome-control-center", "datetime"},
      // KDE
      {"/usr/bin/kcmshell4", "clock"},
      {"/usr/local/bin/kcmshell4", "clock"},
      {"/opt/bin/kcmshell4", "clock"},
  };

  base::CommandLine command(base::FilePath(""));
  for (const ClockCommand& cmd : kClockCommands) {
    base::FilePath pathname(cmd.pathname);
    if (base::PathExists(pathname)) {
      command.SetProgram(pathname);
      command.AppendArg(cmd.argument);
      break;
    }
  }
  if (command.GetProgram().empty()) {
    // Alas, there is nothing we can do.
    return;
  }

  base::LaunchOptions options;
  options.wait = false;
  options.allow_new_privs = true;
  base::LaunchProcess(command, options);

#elif defined(OS_MACOSX)
  base::CommandLine command(base::FilePath("/usr/bin/open"));
  command.AppendArg("/System/Library/PreferencePanes/DateAndTime.prefPane");

  base::LaunchOptions options;
  options.wait = false;
  base::LaunchProcess(command, options);

#elif defined(OS_WIN)
  base::FilePath path;
  PathService::Get(base::DIR_SYSTEM, &path);
  static const base::char16 kControlPanelExe[] = L"control.exe";
  path = path.Append(base::string16(kControlPanelExe));
  base::CommandLine command(path);
  command.AppendArg(std::string("/name"));
  command.AppendArg(std::string("Microsoft.DateAndTime"));

  base::LaunchOptions options;
  options.wait = false;
  base::LaunchProcess(command, options);

#else
  NOTREACHED();

#endif
  // Don't add code here! (See the comment at the beginning of the function.)
}

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
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      time_triggered_(time_triggered) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = kMetricsName;
  scoped_ptr<ChromeMetricsHelper> chrome_metrics_helper(new ChromeMetricsHelper(
      web_contents, request_url, reporting_info, kMetricsName));
  chrome_metrics_helper->StartRecordingCaptivePortalMetrics(false);
  set_metrics_helper(chrome_metrics_helper.Pass());
  metrics_helper()->RecordUserInteraction(
      security_interstitials::MetricsHelper::TOTAL_VISITS);

  cert_report_helper_.reset(new CertReportHelper(
      ssl_cert_reporter.Pass(), web_contents, request_url, ssl_info,
      certificate_reporting::ErrorReport::INTERSTITIAL_CLOCK,
      false /* overridable */, metrics_helper()));

  // TODO(felt): Separate the clock statistics from the main ssl statistics.
  SSLErrorClassification classifier(time_triggered_, request_url, cert_error_,
                                    *ssl_info_.cert.get());
  classifier.RecordUMAStatistics(false);
}

bool BadClockBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

InterstitialPageDelegate::TypeID BadClockBlockingPage::GetTypeForTesting()
    const {
  return BadClockBlockingPage::kTypeForTesting;
}

BadClockBlockingPage::~BadClockBlockingPage() {
  metrics_helper()->RecordShutdownMetrics();
  if (!callback_.is_null()) {
    // Deny when the page is closed.
    NotifyDenyCertificate();
  }
}

void BadClockBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  CHECK(load_time_data);
  base::string16 url(GetFormattedHostName());

  // Values that are currently still shared with the SSL interstitial.
  load_time_data->SetString("type", "SSL");
  load_time_data->SetString("errorCode", net::ErrorToString(cert_error_));
  load_time_data->SetString(
      "openDetails", l10n_util::GetStringUTF16(IDS_SSL_OPEN_DETAILS_BUTTON));
  load_time_data->SetString(
      "closeDetails",
      l10n_util::GetStringUTF16(IDS_SSL_CLOSE_DETAILS_BUTTON));

  // Strings for the bad clock warning specifically.
  load_time_data->SetBoolean("bad_clock", true);
  load_time_data->SetBoolean("overridable", false);
#if defined(OS_IOS)
  load_time_data->SetBoolean("hide_primary_button", true);
#else
  load_time_data->SetBoolean("hide_primary_button", false);
#endif

  int heading_string =
      SSLErrorClassification::IsUserClockInTheFuture(time_triggered_)
          ? IDS_CLOCK_ERROR_AHEAD_HEADING
          : IDS_CLOCK_ERROR_BEHIND_HEADING;

  load_time_data->SetString("tabTitle",
                            l10n_util::GetStringUTF16(IDS_CLOCK_ERROR_TITLE));
  load_time_data->SetString("heading",
                            l10n_util::GetStringUTF16(heading_string));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(
          IDS_CLOCK_ERROR_PRIMARY_PARAGRAPH, url,
          base::TimeFormatFriendlyDateAndTime(time_triggered_)));

  load_time_data->SetString(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_CLOCK_ERROR_UPDATE_DATE_AND_TIME));
  load_time_data->SetString(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_CLOCK_ERROR_EXPLANATION));

  // The interstitial template expects this string, but we're not using it.
  load_time_data->SetString("finalParagraph", std::string());

  // Set debugging information at the bottom of the warning.
  load_time_data->SetString("subject",
                            ssl_info_.cert->subject().GetDisplayName());
  load_time_data->SetString("issuer",
                            ssl_info_.cert->issuer().GetDisplayName());
  load_time_data->SetString(
      "expirationDate",
      base::TimeFormatShortDate(ssl_info_.cert->valid_expiry()));
  load_time_data->SetString("currentDate",
                            base::TimeFormatShortDate(time_triggered_));
  std::vector<std::string> encoded_chain;
  ssl_info_.cert->GetPEMEncodedChain(&encoded_chain);
  load_time_data->SetString(
      "pem", base::JoinString(encoded_chain, base::StringPiece()));

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
  cert_report_helper_->SetSSLCertReporterForTesting(ssl_cert_reporter.Pass());
}

// This handles the commands sent from the interstitial JavaScript.
// DO NOT reorder or change this logic without also changing the JavaScript!
void BadClockBlockingPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);
  switch (cmd) {
    case CMD_DONT_PROCEED:
      interstitial_page()->DontProceed();
      break;
    case CMD_DO_REPORT:
      SetReportingPreference(true);
      break;
    case CMD_DONT_REPORT:
      SetReportingPreference(false);
      break;
    case CMD_SHOW_MORE_SECTION:
      metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::SHOW_ADVANCED);
      break;
    case CMD_OPEN_DATE_SETTINGS:
      metrics_helper()->RecordUserInteraction(
          security_interstitials::MetricsHelper::OPEN_TIME_SETTINGS);
      content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
                                       base::Bind(&LaunchDateAndTimeSettings));
      break;
    case CMD_OPEN_REPORTING_PRIVACY:
      OpenExtendedReportingPrivacyPolicy();
      break;
    case CMD_PROCEED:
    case CMD_OPEN_HELP_CENTER:
    case CMD_RELOAD:
    case CMD_OPEN_DIAGNOSTIC:
      // Not supported for the bad clock interstitial.
      NOTREACHED() << "Unexpected command: " << command;
  }
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
