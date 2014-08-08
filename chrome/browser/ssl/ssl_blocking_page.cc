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
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ssl/ssl_error_classification.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/common/chrome_switches.h"
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
#include "grit/app_locale_settings.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling.h"
#endif

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/win/windows_version.h"
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

#if defined(ENABLE_EXTENSIONS)
using extensions::ExperienceSamplingEvent;
#endif

namespace {

// Constants for the Experience Sampling instrumentation.
#if defined(ENABLE_EXTENSIONS)
const char kEventNameBase[] = "ssl_interstitial_";
const char kEventNotOverridable[] = "notoverridable_";
const char kEventOverridable[] = "overridable_";
#endif

// Events for UMA. Do not reorder or change!
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
  SHOW_UNDERSTAND,  // Used by the summer 2013 Finch trial. Deprecated.
  SHOW_INTERNAL_HOSTNAME,
  PROCEED_INTERNAL_HOSTNAME,
  SHOW_NEW_SITE,
  PROCEED_NEW_SITE,
  PROCEED_MANUAL_NONOVERRIDABLE,
  CAPTIVE_PORTAL_DETECTION_ENABLED,
  CAPTIVE_PORTAL_DETECTION_ENABLED_OVERRIDABLE,
  CAPTIVE_PORTAL_PROBE_COMPLETED,
  CAPTIVE_PORTAL_PROBE_COMPLETED_OVERRIDABLE,
  CAPTIVE_PORTAL_NO_RESPONSE,
  CAPTIVE_PORTAL_NO_RESPONSE_OVERRIDABLE,
  CAPTIVE_PORTAL_DETECTED,
  CAPTIVE_PORTAL_DETECTED_OVERRIDABLE,
  UNUSED_BLOCKING_PAGE_EVENT,
};

void RecordSSLBlockingPageEventStats(SSLBlockingPageEvent event) {
  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl",
                            event,
                            UNUSED_BLOCKING_PAGE_EVENT);
}

void RecordSSLBlockingPageDetailedStats(
    bool proceed,
    int cert_error,
    bool overridable,
    bool internal,
    int num_visits,
    bool captive_portal_detection_enabled,
    bool captive_portal_probe_completed,
    bool captive_portal_no_response,
    bool captive_portal_detected) {
  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl_error_type",
      SSLErrorInfo::NetErrorToErrorType(cert_error), SSLErrorInfo::END_OF_ENUM);
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  if (captive_portal_detection_enabled)
    RecordSSLBlockingPageEventStats(
        overridable ?
        CAPTIVE_PORTAL_DETECTION_ENABLED_OVERRIDABLE :
        CAPTIVE_PORTAL_DETECTION_ENABLED);
  if (captive_portal_probe_completed)
    RecordSSLBlockingPageEventStats(
        overridable ?
        CAPTIVE_PORTAL_PROBE_COMPLETED_OVERRIDABLE :
        CAPTIVE_PORTAL_PROBE_COMPLETED);
  // Log only one of portal detected and no response results.
  if (captive_portal_detected)
    RecordSSLBlockingPageEventStats(
        overridable ?
        CAPTIVE_PORTAL_DETECTED_OVERRIDABLE :
        CAPTIVE_PORTAL_DETECTED);
  else if (captive_portal_no_response)
    RecordSSLBlockingPageEventStats(
        overridable ?
        CAPTIVE_PORTAL_NO_RESPONSE_OVERRIDABLE :
        CAPTIVE_PORTAL_NO_RESPONSE);
#endif
  if (!overridable) {
    if (proceed) {
      RecordSSLBlockingPageEventStats(PROCEED_MANUAL_NONOVERRIDABLE);
    }
    // Overridable is false if the user didn't have any option except to turn
    // back. If that's the case, don't record some of the metrics.
    return;
  }
  if (num_visits == 0)
    RecordSSLBlockingPageEventStats(SHOW_NEW_SITE);
  if (proceed) {
    RecordSSLBlockingPageEventStats(PROCEED_OVERRIDABLE);
    if (internal)
      RecordSSLBlockingPageEventStats(PROCEED_INTERNAL_HOSTNAME);
    if (num_visits == 0)
      RecordSSLBlockingPageEventStats(PROCEED_NEW_SITE);
  } else if (!proceed) {
    RecordSSLBlockingPageEventStats(DONT_PROCEED_OVERRIDABLE);
  }
  SSLErrorInfo::ErrorType type = SSLErrorInfo::NetErrorToErrorType(cert_error);
  switch (type) {
    case SSLErrorInfo::CERT_COMMON_NAME_INVALID: {
      if (proceed)
        RecordSSLBlockingPageEventStats(PROCEED_NAME);
      else
        RecordSSLBlockingPageEventStats(DONT_PROCEED_NAME);
      break;
    }
    case SSLErrorInfo::CERT_DATE_INVALID: {
      if (proceed)
        RecordSSLBlockingPageEventStats(PROCEED_DATE);
      else
        RecordSSLBlockingPageEventStats(DONT_PROCEED_DATE);
      break;
    }
    case SSLErrorInfo::CERT_AUTHORITY_INVALID: {
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

void LaunchDateAndTimeSettings() {
#if defined(OS_CHROMEOS)
  std::string sub_page = std::string(chrome::kSearchSubPage) + "#" +
      l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME);
  chrome::ShowSettingsSubPageForProfile(
      ProfileManager::GetActiveUserProfile(), sub_page);
  return;
#elif defined(OS_ANDROID)
  CommandLine command(base::FilePath("/system/bin/am"));
  command.AppendArg("start");
  command.AppendArg(
      "'com.android.settings/.Settings$DateTimeSettingsActivity'");
#elif defined(OS_IOS)
  // Apparently, iOS really does not have a way to launch the date and time
  // settings. Weird. TODO(palmer): Do something more graceful than ignoring
  // the user's click! crbug.com/394993
  return;
#elif defined(OS_LINUX)
  struct ClockCommand {
    const char* pathname;
    const char* argument;
  };
  static const ClockCommand kClockCommands[] = {
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

  CommandLine command(base::FilePath(""));
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
#elif defined(OS_MACOSX)
  CommandLine command(base::FilePath("/usr/bin/open"));
  command.AppendArg("/System/Library/PreferencePanes/DateAndTime.prefPane");
#elif defined(OS_WIN)
  base::FilePath path;
  PathService::Get(base::DIR_SYSTEM, &path);
  static const base::char16 kControlPanelExe[] = L"control.exe";
  path = path.Append(base::string16(kControlPanelExe));
  CommandLine command(path);
  command.AppendArg(std::string("/name"));
  command.AppendArg(std::string("Microsoft.DateAndTime"));
#else
  return;
#endif

#if !defined(OS_CHROMEOS)
  base::LaunchOptions options;
  options.wait = false;
#if defined(OS_LINUX)
  options.allow_new_privs = true;
#endif
  base::LaunchProcess(command, options, NULL);
#endif
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
      strict_enforcement_(strict_enforcement),
      interstitial_page_(NULL),
      internal_(false),
      num_visits_(-1),
      captive_portal_detection_enabled_(false),
      captive_portal_probe_completed_(false),
      captive_portal_no_response_(false),
      captive_portal_detected_(false) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents->GetBrowserContext());
  // For UMA stats.
  if (net::IsHostnameNonUnique(request_url_.HostNoBrackets()))
    internal_ = true;
  RecordSSLBlockingPageEventStats(SHOW_ALL);
  if (overridable_ && !strict_enforcement_) {
    RecordSSLBlockingPageEventStats(SHOW_OVERRIDABLE);
    if (internal_)
      RecordSSLBlockingPageEventStats(SHOW_INTERNAL_HOSTNAME);
    HistoryService* history_service = HistoryServiceFactory::GetForProfile(
        profile, Profile::EXPLICIT_ACCESS);
    if (history_service) {
      history_service->GetVisibleVisitCountToHost(
          request_url_,
          base::Bind(&SSLBlockingPage::OnGotHistoryCount,
                     base::Unretained(this)),
          &request_tracker_);
    }
  }

  SSLErrorClassification ssl_error_classification(
      base::Time::NowFromSystemTime(),
      request_url_,
      *ssl_info_.cert.get());
  ssl_error_classification.RecordUMAStatistics(
      overridable_ && !strict_enforcement_, cert_error_);

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(profile);
  captive_portal_detection_enabled_ = captive_portal_service ->enabled();
  captive_portal_service ->DetectCaptivePortal();
  registrar_.Add(this,
                 chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
                 content::Source<Profile>(profile));
#endif

#if defined(ENABLE_EXTENSIONS)
  // ExperienceSampling: Set up new sampling event for this interstitial.
  std::string event_name(kEventNameBase);
  if (overridable_ && !strict_enforcement_)
    event_name.append(kEventOverridable);
  else
    event_name.append(kEventNotOverridable);
  event_name.append(net::ErrorToString(cert_error_));
  sampling_event_.reset(new ExperienceSamplingEvent(
      event_name,
      request_url_,
      web_contents_->GetLastCommittedURL(),
      web_contents_->GetBrowserContext()));
#endif

  // Creating an interstitial without showing (e.g. from chrome://interstitials)
  // it leaks memory, so don't create it here.
}

SSLBlockingPage::~SSLBlockingPage() {
  if (!callback_.is_null()) {
    RecordSSLBlockingPageDetailedStats(false,
                                       cert_error_,
                                       overridable_ && !strict_enforcement_,
                                       internal_,
                                       num_visits_,
                                       captive_portal_detection_enabled_,
                                       captive_portal_probe_completed_,
                                       captive_portal_no_response_,
                                       captive_portal_detected_);
    // The page is closed without the user having chosen what to do, default to
    // deny.
    NotifyDenyCertificate();
  }
}

void SSLBlockingPage::Show() {
  DCHECK(!interstitial_page_);
  interstitial_page_ = InterstitialPage::Create(
      web_contents_, true, request_url_, this);
  interstitial_page_->Show();
}

std::string SSLBlockingPage::GetHTMLContents() {
  base::DictionaryValue load_time_data;
  base::string16 url(ASCIIToUTF16(request_url_.host()));
  if (base::i18n::IsRTL())
    base::i18n::WrapStringWithLTRFormatting(&url);
  webui::SetFontAndTextDirection(&load_time_data);

  // Shared values for both the overridable and non-overridable versions.
  load_time_data.SetBoolean("ssl", true);
  load_time_data.SetBoolean(
      "overridable", overridable_ && !strict_enforcement_);
  load_time_data.SetString(
      "tabTitle", l10n_util::GetStringUTF16(IDS_SSL_V2_TITLE));
  load_time_data.SetString(
      "heading", l10n_util::GetStringUTF16(IDS_SSL_V2_HEADING));
  if ((SSLErrorClassification::IsUserClockInThePast(
      base::Time::NowFromSystemTime()))
      && (SSLErrorInfo::NetErrorToErrorType(cert_error_) ==
          SSLErrorInfo::CERT_DATE_INVALID)) {
    load_time_data.SetString("primaryParagraph",
                             l10n_util::GetStringFUTF16(
                                 IDS_SSL_CLOCK_ERROR,
                                 url,
                                 base::TimeFormatShortDate(base::Time::Now())));
  } else {
    load_time_data.SetString(
        "primaryParagraph",
        l10n_util::GetStringFUTF16(IDS_SSL_V2_PRIMARY_PARAGRAPH, url));
  }
  load_time_data.SetString(
     "openDetails",
     l10n_util::GetStringUTF16(IDS_SSL_V2_OPEN_DETAILS_BUTTON));
  load_time_data.SetString(
     "closeDetails",
     l10n_util::GetStringUTF16(IDS_SSL_V2_CLOSE_DETAILS_BUTTON));
  load_time_data.SetString("errorCode", net::ErrorToString(cert_error_));

  if (overridable_ && !strict_enforcement_) {  // Overridable.
    SSLErrorInfo error_info =
        SSLErrorInfo::CreateError(
            SSLErrorInfo::NetErrorToErrorType(cert_error_),
            ssl_info_.cert.get(),
            request_url_);
    load_time_data.SetString(
        "explanationParagraph", error_info.details());
    load_time_data.SetString(
        "primaryButtonText",
        l10n_util::GetStringUTF16(IDS_SSL_OVERRIDABLE_SAFETY_BUTTON));
    load_time_data.SetString(
        "finalParagraph",
        l10n_util::GetStringFUTF16(IDS_SSL_OVERRIDABLE_PROCEED_PARAGRAPH, url));
  } else {  // Non-overridable.
    load_time_data.SetBoolean("overridable", false);
    SSLErrorInfo::ErrorType type =
        SSLErrorInfo::NetErrorToErrorType(cert_error_);
    if (type == SSLErrorInfo::CERT_INVALID && SSLErrorClassification::
        IsWindowsVersionSP3OrLower()) {
      load_time_data.SetString(
          "explanationParagraph",
          l10n_util::GetStringFUTF16(
              IDS_SSL_NONOVERRIDABLE_MORE_INVALID_SP3, url));
    } else {
      load_time_data.SetString("explanationParagraph",
                               l10n_util::GetStringFUTF16(
                                   IDS_SSL_NONOVERRIDABLE_MORE, url));
    }
    load_time_data.SetString(
        "primaryButtonText",
        l10n_util::GetStringUTF16(IDS_SSL_NONOVERRIDABLE_RELOAD_BUTTON));
    // Customize the help link depending on the specific error type.
    // Only mark as HSTS if none of the more specific error types apply, and use
    // INVALID as a fallback if no other string is appropriate.
    load_time_data.SetInteger("errorType", type);
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
    load_time_data.SetString(
        "finalParagraph", l10n_util::GetStringFUTF16(help_string, url));
  }

  base::StringPiece html(
     ResourceBundle::GetSharedInstance().GetRawDataResource(
         IRD_SSL_INTERSTITIAL_V2_HTML));
  webui::UseVersion2 version;
  return webui::GetI18nTemplateHtml(html, &load_time_data);
}

void SSLBlockingPage::OverrideEntry(NavigationEntry* entry) {
  int cert_id = content::CertStore::GetInstance()->StoreCert(
      ssl_info_.cert.get(), web_contents_->GetRenderProcessHost()->GetID());
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
      interstitial_page_->DontProceed();
      break;
    }
    case CMD_PROCEED: {
      interstitial_page_->Proceed();
      break;
    }
    case CMD_MORE: {
      RecordSSLBlockingPageEventStats(MORE);
#if defined(ENABLE_EXTENSIONS)
      if (sampling_event_.get())
        sampling_event_->set_has_viewed_details(true);
#endif
      break;
    }
    case CMD_RELOAD: {
      // The interstitial can't refresh itself.
      web_contents_->GetController().Reload(true);
      break;
    }
    case CMD_HELP: {
      content::NavigationController::LoadURLParams help_page_params(GURL(
          "https://support.google.com/chrome/answer/4454607"));
#if defined(ENABLE_EXTENSIONS)
      if (sampling_event_.get())
        sampling_event_->set_has_viewed_learn_more(true);
#endif
      web_contents_->GetController().LoadURLWithParams(help_page_params);
      break;
    }
    case CMD_CLOCK: {
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
      web_contents_->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile);
}

void SSLBlockingPage::OnProceed() {
  RecordSSLBlockingPageDetailedStats(true,
                                     cert_error_,
                                     overridable_ && !strict_enforcement_,
                                     internal_,
                                     num_visits_,
                                     captive_portal_detection_enabled_,
                                     captive_portal_probe_completed_,
                                     captive_portal_no_response_,
                                     captive_portal_detected_);
#if defined(ENABLE_EXTENSIONS)
  // ExperienceSampling: Notify that user decided to proceed.
  if (sampling_event_.get())
    sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kProceed);
#endif
  // Accepting the certificate resumes the loading of the page.
  NotifyAllowCertificate();
}

void SSLBlockingPage::OnDontProceed() {
  RecordSSLBlockingPageDetailedStats(false,
                                     cert_error_,
                                     overridable_ && !strict_enforcement_,
                                     internal_,
                                     num_visits_,
                                     captive_portal_detection_enabled_,
                                     captive_portal_probe_completed_,
                                     captive_portal_no_response_,
                                     captive_portal_detected_);
#if defined(ENABLE_EXTENSIONS)
  // ExperienceSampling: Notify that user decided to not proceed.
  // This also occurs if the user navigates away or closes the tab.
  if (sampling_event_.get())
    sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kDeny);
#endif
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
    base::DictionaryValue* strings,
    const std::vector<base::string16>& extra_info) {
  DCHECK_LT(extra_info.size(), 5U);  // We allow 5 paragraphs max.
  const char* keys[5] = {
      "moreInfo1", "moreInfo2", "moreInfo3", "moreInfo4", "moreInfo5"
  };
  int i;
  for (i = 0; i < static_cast<int>(extra_info.size()); i++) {
    strings->SetString(keys[i], extra_info[i]);
  }
  for (; i < 5; i++) {
    strings->SetString(keys[i], std::string());
  }
}

void SSLBlockingPage::OnGotHistoryCount(bool success,
                                        int num_visits,
                                        base::Time first_visit) {
  num_visits_ = num_visits;
}

void SSLBlockingPage::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // When detection is disabled, captive portal service always sends
  // RESULT_INTERNET_CONNECTED. Ignore any probe results in that case.
  if (!captive_portal_detection_enabled_)
    return;
  if (type == chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT) {
    captive_portal_probe_completed_ = true;
    CaptivePortalService::Results* results =
        content::Details<CaptivePortalService::Results>(
            details).ptr();
    // If a captive portal was detected at any point when the interstitial was
    // displayed, assume that the interstitial was caused by a captive portal.
    // Example scenario:
    // 1- Interstitial displayed and captive portal detected, setting the flag.
    // 2- Captive portal detection automatically opens portal login page.
    // 3- User logs in on the portal login page.
    // A notification will be received here for RESULT_INTERNET_CONNECTED. Make
    // sure we don't clear the captive portal flag, since the interstitial was
    // potentially caused by the captive portal.
    captive_portal_detected_ = captive_portal_detected_ ||
        (results->result == captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
    // Also keep track of non-HTTP portals and error cases.
    captive_portal_no_response_ = captive_portal_no_response_ ||
        (results->result == captive_portal::RESULT_NO_RESPONSE);
  }
#endif
}
