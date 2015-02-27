// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_blocking_page.h"

#include "base/build_time.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ssl/ssl_error_classification.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/ssl_status.h"
#include "grit/browser_resources.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/win/windows_version.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/intent_helper.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#endif

using base::ASCIIToUTF16;
using base::TimeTicks;
using content::InterstitialPage;
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

// Rappor prefix
const char kSSLRapporPrefix[] = "ssl";

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

void LaunchDateAndTimeSettings() {
  // The code for each OS is completely separate, in order to avoid bugs like
  // https://crbug.com/430877 .
#if defined(OS_ANDROID)
  chrome::android::OpenDateAndTimeSettings();

#elif defined(OS_CHROMEOS)
  std::string sub_page = std::string(chrome::kSearchSubPage) + "#" +
      l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME);
  chrome::ShowSettingsSubPageForProfile(
      ProfileManager::GetActiveUserProfile(), sub_page);

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
    { "/usr/bin/unity-control-center", "datetime" },
    // GNOME
    //
    // NOTE: On old Ubuntu, naming control panels doesn't work, so it
    // opens the overview. This will have to be good enough.
    { "/usr/bin/gnome-control-center", "datetime" },
    { "/usr/local/bin/gnome-control-center", "datetime" },
    { "/opt/bin/gnome-control-center", "datetime" },
    // KDE
    { "/usr/bin/kcmshell4", "clock" },
    { "/usr/local/bin/kcmshell4", "clock" },
    { "/opt/bin/kcmshell4", "clock" },
  };

  base::CommandLine command(base::FilePath(""));
  for (size_t i = 0; i < arraysize(kClockCommands); ++i) {
    base::FilePath pathname(kClockCommands[i].pathname);
    if (base::PathExists(pathname)) {
      command.SetProgram(pathname);
      command.AppendArg(kClockCommands[i].argument);
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

bool IsErrorDueToBadClock(const base::Time& now, int error) {
  if (SSLErrorInfo::NetErrorToErrorType(error) !=
          SSLErrorInfo::CERT_DATE_INVALID) {
    return false;
  }
  return SSLErrorClassification::IsUserClockInThePast(now) ||
      SSLErrorClassification::IsUserClockInTheFuture(now);
}

}  // namespace

// static
const void* SSLBlockingPage::kTypeForTesting =
    &SSLBlockingPage::kTypeForTesting;

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
SSLBlockingPage::SSLBlockingPage(content::WebContents* web_contents,
                                 int cert_error,
                                 const net::SSLInfo& ssl_info,
                                 const GURL& request_url,
                                 int options_mask,
                                 const base::Time& time_triggered,
                                 const base::Callback<void(bool)>& callback)
    : SecurityInterstitialPage(web_contents, request_url),
      callback_(callback),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      overridable_(IsOptionsOverridable(options_mask)),
      danger_overridable_(true),
      strict_enforcement_((options_mask & STRICT_ENFORCEMENT) != 0),
      expired_but_previously_allowed_(
          (options_mask & EXPIRED_BUT_PREVIOUSLY_ALLOWED) != 0),
      time_triggered_(time_triggered) {
  interstitial_reason_ =
      IsErrorDueToBadClock(time_triggered_, cert_error_) ?
      SSL_REASON_BAD_CLOCK : SSL_REASON_SSL;

  // We collapse the Rappor metric name to just "ssl" so we don't leak
  // the "overridable" bit.  We skip Rappor altogether for bad clocks.
  // This must be done after calculating |interstitial_reason_| above.
  metrics_helper_.reset(new SecurityInterstitialMetricsHelper(
      web_contents, request_url, GetUmaHistogramPrefix(), kSSLRapporPrefix,
      (interstitial_reason_ == SSL_REASON_BAD_CLOCK
           ? SecurityInterstitialMetricsHelper::SKIP_RAPPOR
           : SecurityInterstitialMetricsHelper::REPORT_RAPPOR),
      GetSamplingEventName()));

  metrics_helper_->RecordUserDecision(SecurityInterstitialMetricsHelper::SHOW);
  metrics_helper_->RecordUserInteraction(
      SecurityInterstitialMetricsHelper::TOTAL_VISITS);

  ssl_error_classification_.reset(new SSLErrorClassification(
      web_contents,
      time_triggered_,
      request_url,
      cert_error_,
      *ssl_info_.cert.get()));
  ssl_error_classification_->RecordUMAStatistics(overridable_);

  // Creating an interstitial without showing (e.g. from chrome://interstitials)
  // it leaks memory, so don't create it here.
}

bool SSLBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

const void* SSLBlockingPage::GetTypeForTesting() const {
  return SSLBlockingPage::kTypeForTesting;
}

SSLBlockingPage::~SSLBlockingPage() {
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // Captive portal detection results can arrive anytime during the interstitial
  // is being displayed, so record it when the interstitial is going away.
  ssl_error_classification_->RecordCaptivePortalUMAStatistics(overridable_);
#endif
  if (!callback_.is_null()) {
    // The page is closed without the user having chosen what to do, default to
    // deny.
    metrics_helper_->RecordUserDecision(
        SecurityInterstitialMetricsHelper::DONT_PROCEED);
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

  // Shared UI configuration for all SSL interstitials.
  load_time_data->SetString("errorCode", net::ErrorToString(cert_error_));
  load_time_data->SetString(
      "openDetails",
      l10n_util::GetStringUTF16(IDS_SSL_V2_OPEN_DETAILS_BUTTON));
  load_time_data->SetString(
      "closeDetails",
      l10n_util::GetStringUTF16(IDS_SSL_V2_CLOSE_DETAILS_BUTTON));

  // Conditional UI configuration.
  if (interstitial_reason_ == SSL_REASON_BAD_CLOCK) {
    load_time_data->SetBoolean("bad_clock", true);
    load_time_data->SetBoolean("overridable", false);

#if defined(OS_IOS)
    load_time_data->SetBoolean("hide_primary_button", true);
#else
    load_time_data->SetBoolean("hide_primary_button", false);
#endif

    // We're showing the SSL clock warning to be helpful, but we haven't warned
    // them about the risks. (And there might still be an SSL error after they
    // fix their clock.) Thus, we don't allow the "danger" override in this
    // case.
    danger_overridable_ = false;

    int heading_string =
        SSLErrorClassification::IsUserClockInTheFuture(time_triggered_) ?
        IDS_SSL_V2_CLOCK_AHEAD_HEADING :
        IDS_SSL_V2_CLOCK_BEHIND_HEADING;

    load_time_data->SetString(
        "tabTitle",
        l10n_util::GetStringUTF16(IDS_SSL_V2_CLOCK_TITLE));
    load_time_data->SetString(
        "heading",
        l10n_util::GetStringUTF16(heading_string));
    load_time_data->SetString(
        "primaryParagraph",
        l10n_util::GetStringFUTF16(
            IDS_SSL_V2_CLOCK_PRIMARY_PARAGRAPH,
            url,
            base::TimeFormatFriendlyDateAndTime(time_triggered_)));

    load_time_data->SetString(
        "primaryButtonText",
        l10n_util::GetStringUTF16(IDS_SSL_V2_CLOCK_UPDATE_DATE_AND_TIME));
    load_time_data->SetString(
        "explanationParagraph",
        l10n_util::GetStringUTF16(IDS_SSL_V2_CLOCK_EXPLANATION));

    // The interstitial template expects this string, but we're not using it. So
    // we send a blank string for now.
    load_time_data->SetString("finalParagraph", std::string());
  } else {
    load_time_data->SetBoolean("bad_clock", false);

    load_time_data->SetString(
        "tabTitle", l10n_util::GetStringUTF16(IDS_SSL_V2_TITLE));
    load_time_data->SetString(
        "heading", l10n_util::GetStringUTF16(IDS_SSL_V2_HEADING));
    load_time_data->SetString(
        "primaryParagraph",
        l10n_util::GetStringFUTF16(IDS_SSL_V2_PRIMARY_PARAGRAPH, url));

    if (overridable_) {
      load_time_data->SetBoolean("overridable", true);

      SSLErrorInfo error_info =
          SSLErrorInfo::CreateError(
              SSLErrorInfo::NetErrorToErrorType(cert_error_),
              ssl_info_.cert.get(),
              request_url());
      load_time_data->SetString("explanationParagraph", error_info.details());
      load_time_data->SetString(
          "primaryButtonText",
          l10n_util::GetStringUTF16(IDS_SSL_OVERRIDABLE_SAFETY_BUTTON));
      load_time_data->SetString(
          "finalParagraph",
          l10n_util::GetStringFUTF16(IDS_SSL_OVERRIDABLE_PROCEED_PARAGRAPH,
                                   url));
    } else {
      load_time_data->SetBoolean("overridable", false);

      SSLErrorInfo::ErrorType type =
          SSLErrorInfo::NetErrorToErrorType(cert_error_);
      if (type == SSLErrorInfo::CERT_INVALID && SSLErrorClassification::
          MaybeWindowsLacksSHA256Support()) {
        load_time_data->SetString(
            "explanationParagraph",
            l10n_util::GetStringFUTF16(
                IDS_SSL_NONOVERRIDABLE_MORE_INVALID_SP3, url));
      } else {
        load_time_data->SetString("explanationParagraph",
                                 l10n_util::GetStringFUTF16(
                                     IDS_SSL_NONOVERRIDABLE_MORE, url));
      }
      load_time_data->SetString(
          "primaryButtonText",
          l10n_util::GetStringUTF16(IDS_SSL_RELOAD));
      // Customize the help link depending on the specific error type.
      // Only mark as HSTS if none of the more specific error types apply,
      // and use INVALID as a fallback if no other string is appropriate.
      load_time_data->SetInteger("errorType", type);
      int help_string = IDS_SSL_NONOVERRIDABLE_INVALID;
      switch (type) {
        case SSLErrorInfo::CERT_REVOKED:
          help_string = IDS_SSL_NONOVERRIDABLE_REVOKED;
          break;
        case SSLErrorInfo::CERT_PINNED_KEY_MISSING:
          help_string = IDS_SSL_NONOVERRIDABLE_PINNED;
          break;
        case SSLErrorInfo::CERT_INVALID:
          help_string = IDS_SSL_NONOVERRIDABLE_INVALID;
          break;
        default:
          if (strict_enforcement_)
            help_string = IDS_SSL_NONOVERRIDABLE_HSTS;
      }
      load_time_data->SetString(
          "finalParagraph", l10n_util::GetStringFUTF16(help_string, url));
    }
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
      "pem", JoinString(encoded_chain, std::string()));
}

void SSLBlockingPage::OverrideEntry(NavigationEntry* entry) {
  int cert_id = content::CertStore::GetInstance()->StoreCert(
      ssl_info_.cert.get(), web_contents()->GetRenderProcessHost()->GetID());
  DCHECK(cert_id);

  entry->GetSSL().security_style =
      content::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  entry->GetSSL().cert_id = cert_id;
  entry->GetSSL().cert_status = ssl_info_.cert_status;
  entry->GetSSL().security_bits = ssl_info_.security_bits;
}

// This handles the commands sent from the interstitial JavaScript. They are
// defined in chrome/browser/resources/ssl/ssl_errors_common.js.
// DO NOT reorder or change this logic without also changing the JavaScript!
void SSLBlockingPage::CommandReceived(const std::string& command) {
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
    case CMD_MORE: {
      metrics_helper_->RecordUserInteraction(
          SecurityInterstitialMetricsHelper::SHOW_ADVANCED);
      break;
    }
    case CMD_RELOAD: {
      metrics_helper_->RecordUserInteraction(
          SecurityInterstitialMetricsHelper::RELOAD);
      // The interstitial can't refresh itself.
      web_contents()->GetController().Reload(true);
      break;
    }
    case CMD_HELP: {
      metrics_helper_->RecordUserInteraction(
          SecurityInterstitialMetricsHelper::SHOW_LEARN_MORE);
      content::NavigationController::LoadURLParams help_page_params(
          google_util::AppendGoogleLocaleParam(
              GURL(kHelpURL), g_browser_process->GetApplicationLocale()));
      web_contents()->GetController().LoadURLWithParams(help_page_params);
      break;
    }
    case CMD_CLOCK: {
      metrics_helper_->RecordUserInteraction(
          SecurityInterstitialMetricsHelper::OPEN_TIME_SETTINGS);
      LaunchDateAndTimeSettings();
      break;
    }
    default: {
      NOTREACHED();
    }
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
  metrics_helper_->RecordUserDecision(
      SecurityInterstitialMetricsHelper::PROCEED);
  RecordSSLExpirationPageEventState(
      expired_but_previously_allowed_, true, overridable_);
  // Accepting the certificate resumes the loading of the page.
  NotifyAllowCertificate();
}

void SSLBlockingPage::OnDontProceed() {
  metrics_helper_->RecordUserDecision(
      SecurityInterstitialMetricsHelper::DONT_PROCEED);
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
  switch (interstitial_reason_) {
    case SSL_REASON_SSL:
      if (overridable_)
        return "ssl_overridable";
      else
        return "ssl_nonoverridable";
    case SSL_REASON_BAD_CLOCK:
      return "bad_clock";
  }
  NOTREACHED();
  return std::string();
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
bool SSLBlockingPage::IsOptionsOverridable(int options_mask) {
  return (options_mask & SSLBlockingPage::OVERRIDABLE) &&
         !(options_mask & SSLBlockingPage::STRICT_ENFORCEMENT);
}
