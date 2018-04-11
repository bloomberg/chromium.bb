// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/md_settings_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/unified_consent_helper.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/settings/about_handler.h"
#include "chrome/browser/ui/webui/settings/appearance_handler.h"
#include "chrome/browser/ui/webui/settings/browser_lifetime_handler.h"
#include "chrome/browser/ui/webui/settings/downloads_handler.h"
#include "chrome/browser/ui/webui/settings/extension_control_handler.h"
#include "chrome/browser/ui/webui/settings/font_handler.h"
#include "chrome/browser/ui/webui/settings/md_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/metrics_reporting_handler.h"
#include "chrome/browser/ui/webui/settings/on_startup_handler.h"
#include "chrome/browser/ui/webui/settings/people_handler.h"
#include "chrome/browser/ui/webui/settings/profile_info_handler.h"
#include "chrome/browser/ui/webui/settings/protocol_handlers_handler.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/ui/webui/settings/safe_browsing_handler.h"
#include "chrome/browser/ui/webui/settings/search_engines_handler.h"
#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"
#include "chrome/browser/ui/webui/settings/settings_cookies_view_handler.h"
#include "chrome/browser/ui/webui/settings/settings_import_data_handler.h"
#include "chrome/browser/ui/webui/settings/settings_media_devices_selection_handler.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/ui/webui/settings/settings_startup_pages_handler.h"
#include "chrome/browser/ui/webui/settings/site_settings_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/settings_resources.h"
#include "chrome/grit/settings_resources_map.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "printing/buildflags/buildflags.h"

#if defined(OS_WIN)
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/webui/settings/chrome_cleanup_handler.h"
#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/conflicts/problematic_programs_updater_win.h"
#include "chrome/browser/conflicts/token_util_win.h"
#include "chrome/browser/ui/webui/settings/incompatible_applications_handler_win.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#endif
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/settings/languages_handler.h"
#include "chrome/browser/ui/webui/settings/tts_handler.h"
#endif  // defined(OS_WIN) || defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/stylus_utils.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/android_apps_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/change_picture_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/date_time_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_keyboard_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_pointer_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_power_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_storage_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_stylus_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/easy_unlock_settings_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/fingerprint_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/google_assistant_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/internet_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/smb_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_util.h"
#else  // !defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/settings/settings_default_browser_handler.h"
#include "chrome/browser/ui/webui/settings/settings_manage_profile_handler.h"
#include "chrome/browser/ui/webui/settings/system_handler.h"
#include "components/signin/core/browser/profile_management_switches.h"
#endif  // defined(OS_CHROMEOS)

#if defined(USE_NSS_CERTS)
#include "chrome/browser/ui/webui/certificates_handler.h"
#elif defined(OS_WIN) || defined(OS_MACOSX)
#include "chrome/browser/ui/webui/settings/native_certificates_handler.h"
#endif  // defined(USE_NSS_CERTS)

#if BUILDFLAG(ENABLE_PRINTING) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/settings/printing_handler.h"
#endif

#if defined(SAFE_BROWSING_DB_LOCAL)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ui/webui/settings/change_password_handler.h"
#endif

namespace settings {

// static
void MdSettingsUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kImportDialogAutofillFormData, true);
  registry->RegisterBooleanPref(prefs::kImportDialogBookmarks, true);
  registry->RegisterBooleanPref(prefs::kImportDialogHistory, true);
  registry->RegisterBooleanPref(prefs::kImportDialogSavedPasswords, true);
  registry->RegisterBooleanPref(prefs::kImportDialogSearchEngine, true);
}

MdSettingsUI::MdSettingsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui),
      WebContentsObserver(web_ui->GetWebContents()) {
#if BUILDFLAG(OPTIMIZE_WEBUI)
  std::vector<std::string> exclude_from_gzip;
#endif

  Profile* profile = Profile::FromWebUI(web_ui);
  AddSettingsPageUIHandler(std::make_unique<AppearanceHandler>(web_ui));

#if defined(USE_NSS_CERTS)
  AddSettingsPageUIHandler(
      std::make_unique<certificate_manager::CertificatesHandler>());
#elif defined(OS_WIN) || defined(OS_MACOSX)
  AddSettingsPageUIHandler(std::make_unique<NativeCertificatesHandler>());
#endif  // defined(USE_NSS_CERTS)

  AddSettingsPageUIHandler(std::make_unique<BrowserLifetimeHandler>());
  AddSettingsPageUIHandler(std::make_unique<ClearBrowsingDataHandler>(web_ui));
  AddSettingsPageUIHandler(std::make_unique<CookiesViewHandler>());
  AddSettingsPageUIHandler(std::make_unique<DownloadsHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ExtensionControlHandler>());
  AddSettingsPageUIHandler(std::make_unique<FontHandler>(web_ui));
  AddSettingsPageUIHandler(std::make_unique<ImportDataHandler>());

#if defined(OS_WIN) || defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(std::make_unique<LanguagesHandler>(web_ui));
#endif  // defined(OS_WIN) || defined(OS_CHROMEOS)

  AddSettingsPageUIHandler(
      std::make_unique<MediaDevicesSelectionHandler>(profile));
#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(std::make_unique<MetricsReportingHandler>());
#endif
  AddSettingsPageUIHandler(std::make_unique<OnStartupHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<PeopleHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ProfileInfoHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ProtocolHandlersHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<SafeBrowsingHandler>(profile->GetPrefs()));
  AddSettingsPageUIHandler(std::make_unique<SearchEnginesHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<SiteSettingsHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<StartupPagesHandler>(web_ui));

#if defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(
      std::make_unique<chromeos::settings::AccessibilityHandler>(web_ui));
  AddSettingsPageUIHandler(
      std::make_unique<chromeos::settings::AndroidAppsHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<chromeos::settings::ChangePictureHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<chromeos::settings::CupsPrintersHandler>(web_ui));
  AddSettingsPageUIHandler(
      std::make_unique<chromeos::settings::FingerprintHandler>(profile));
  if (chromeos::switches::IsVoiceInteractionEnabled()) {
    AddSettingsPageUIHandler(
        std::make_unique<chromeos::settings::GoogleAssistantHandler>(profile));
  }
  AddSettingsPageUIHandler(
      std::make_unique<chromeos::settings::KeyboardHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<chromeos::settings::PointerHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<chromeos::settings::SmbHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<chromeos::settings::StorageHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<chromeos::settings::StylusHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<chromeos::settings::InternetHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<TtsHandler>());
#else
  AddSettingsPageUIHandler(std::make_unique<DefaultBrowserHandler>(web_ui));
  AddSettingsPageUIHandler(std::make_unique<ManageProfileHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<SystemHandler>());
#endif

#if BUILDFLAG(ENABLE_PRINTING) && !defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(std::make_unique<PrintingHandler>());
#endif

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUISettingsHost);

#if defined(OS_WIN)
  bool chromeCleanupEnabled = false;
  bool userInitiatedCleanupsEnabled = false;

  AddSettingsPageUIHandler(std::make_unique<ChromeCleanupHandler>(profile));

  safe_browsing::ChromeCleanerController* cleaner_controller =
      safe_browsing::ChromeCleanerController::GetInstance();
  chromeCleanupEnabled = cleaner_controller->ShouldShowCleanupInSettingsUI();
  userInitiatedCleanupsEnabled = safe_browsing::UserInitiatedCleanupsEnabled();

#if defined(GOOGLE_CHROME_BUILD)
  html_source->AddResourcePath("partner-logo.svg", IDR_CHROME_CLEANUP_PARTNER);
#if BUILDFLAG(OPTIMIZE_WEBUI)
  exclude_from_gzip.push_back("partner-logo.svg");
#endif
#endif  // defined(GOOGLE_CHROME_BUILD)

  html_source->AddBoolean("chromeCleanupEnabled", chromeCleanupEnabled);
  // Don't need to save this variable in UpdateCleanupDataSource() because it
  // should never change while Chrome is open.
  html_source->AddBoolean("userInitiatedCleanupsEnabled",
                          userInitiatedCleanupsEnabled);
#endif  // defined(OS_WIN)

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  bool has_incompatible_applications =
      ProblematicProgramsUpdater::IsIncompatibleApplicationsWarningEnabled() &&
      ProblematicProgramsUpdater::HasCachedPrograms();
  html_source->AddBoolean("showIncompatibleApplications",
                          has_incompatible_applications);
  html_source->AddBoolean("hasAdminRights", HasAdminRights());

  if (has_incompatible_applications)
    AddSettingsPageUIHandler(
        std::make_unique<IncompatibleApplicationsHandler>());
#endif  // OS_WIN && defined(GOOGLE_CHROME_BUILD)

  bool password_protection_available = false;
#if defined(SAFE_BROWSING_DB_LOCAL)
  safe_browsing::ChromePasswordProtectionService* password_protection =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(profile);
  password_protection_available = !!password_protection;
  if (password_protection) {
    AddSettingsPageUIHandler(
        std::make_unique<ChangePasswordHandler>(profile, password_protection));
  }
#endif
  html_source->AddBoolean("passwordProtectionAvailable",
                          password_protection_available);

#if defined(OS_CHROMEOS)
  chromeos::settings::EasyUnlockSettingsHandler* easy_unlock_handler =
      chromeos::settings::EasyUnlockSettingsHandler::Create(html_source,
                                                            profile);
  if (easy_unlock_handler)
    AddSettingsPageUIHandler(base::WrapUnique(easy_unlock_handler));

  AddSettingsPageUIHandler(base::WrapUnique(
      chromeos::settings::DateTimeHandler::Create(html_source)));

  AddSettingsPageUIHandler(
      std::make_unique<chromeos::settings::StylusHandler>());
  html_source->AddBoolean(
      "quickUnlockEnabled",
      chromeos::quick_unlock::IsPinEnabled(profile->GetPrefs()));
  html_source->AddBoolean(
      "quickUnlockDisabledByPolicy",
      chromeos::quick_unlock::IsPinDisabledByPolicy(profile->GetPrefs()));
  html_source->AddBoolean("fingerprintUnlockEnabled",
                          chromeos::quick_unlock::IsFingerprintEnabled());
  html_source->AddBoolean("hasInternalStylus",
                          ash::stylus_utils::HasInternalStylus());

  // We have 2 variants of Android apps settings. Default case, when the Play
  // Store app exists we show expandable section that allows as to
  // enable/disable the Play Store and link to Android settings which is
  // available once settings app is registered in the system.
  // For AOSP images we don't have the Play Store app. In last case we Android
  // apps settings consists only from root link to Android settings and only
  // visible once settings app is registered.
  const bool androidAppsVisible = arc::IsArcAllowedForProfile(profile) &&
                                  !arc::IsArcOptInVerificationDisabled();
  html_source->AddBoolean("androidAppsVisible", androidAppsVisible);
  html_source->AddBoolean("havePlayStoreApp", arc::IsPlayStoreAvailable());

  // TODO(mash): Support Chrome power settings in Mash. crbug.com/644348
  bool enable_power_settings = !ash_util::IsRunningInMash();
  html_source->AddBoolean("enablePowerSettings", enable_power_settings);
  if (enable_power_settings) {
    AddSettingsPageUIHandler(std::make_unique<chromeos::settings::PowerHandler>(
        profile->GetPrefs()));
  }
#else   // !defined(OS_CHROMEOS)
  html_source->AddBoolean("diceEnabled",
                          signin::IsDiceEnabledForProfile(profile->GetPrefs()));
#endif  // defined(OS_CHROMEOS)

  html_source->AddBoolean("unifiedConsentEnabled",
                          IsUnifiedConsentEnabled(profile));

  html_source->AddBoolean("showExportPasswords",
                          base::FeatureList::IsEnabled(
                              password_manager::features::kPasswordExport));

  html_source->AddBoolean("showImportPasswords",
                          base::FeatureList::IsEnabled(
                              password_manager::features::kPasswordImport));

  AddSettingsPageUIHandler(
      base::WrapUnique(AboutHandler::Create(html_source, profile)));
  AddSettingsPageUIHandler(
      base::WrapUnique(ResetSettingsHandler::Create(html_source, profile)));

  // Add the metrics handler to write uma stats.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

#if BUILDFLAG(OPTIMIZE_WEBUI)
  html_source->AddResourcePath("crisper.js", IDR_MD_SETTINGS_CRISPER_JS);
  html_source->AddResourcePath("lazy_load.crisper.js",
                               IDR_MD_SETTINGS_LAZY_LOAD_CRISPER_JS);
  html_source->AddResourcePath("lazy_load.html",
                               IDR_MD_SETTINGS_LAZY_LOAD_VULCANIZED_HTML);
  html_source->SetDefaultResource(IDR_MD_SETTINGS_VULCANIZED_HTML);
  html_source->UseGzip(exclude_from_gzip);
#else
  // Add all settings resources.
  for (size_t i = 0; i < kSettingsResourcesSize; ++i) {
    html_source->AddResourcePath(kSettingsResources[i].name,
                                 kSettingsResources[i].value);
  }
  html_source->SetDefaultResource(IDR_SETTINGS_SETTINGS_HTML);
#endif

  AddLocalizedStrings(html_source, profile);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source);

#if defined(OS_WIN)
  // This needs to be below content::WebUIDataSource::Add to make sure there
  // is a WebUIDataSource to update if the observer is immediately notified.
  cleanup_observer_.reset(
      new safe_browsing::ChromeCleanerStateChangeObserver(base::Bind(
          &MdSettingsUI::UpdateCleanupDataSource, base::Unretained(this))));
#endif  // defined(OS_WIN)
}

MdSettingsUI::~MdSettingsUI() {
}

void MdSettingsUI::AddSettingsPageUIHandler(
    std::unique_ptr<content::WebUIMessageHandler> handler) {
  DCHECK(handler);
  web_ui()->AddMessageHandler(std::move(handler));
}

void MdSettingsUI::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument())
    return;

  load_start_time_ = base::Time::Now();
}

void MdSettingsUI::DocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  UMA_HISTOGRAM_TIMES("Settings.LoadDocumentTime.MD",
                      base::Time::Now() - load_start_time_);
}

void MdSettingsUI::DocumentOnLoadCompletedInMainFrame() {
  UMA_HISTOGRAM_TIMES("Settings.LoadCompletedTime.MD",
                      base::Time::Now() - load_start_time_);
}

#if defined(OS_WIN)
void MdSettingsUI::UpdateCleanupDataSource(bool cleanupEnabled) {
  DCHECK(web_ui());
  Profile* profile = Profile::FromWebUI(web_ui());

  std::unique_ptr<base::DictionaryValue> update(new base::DictionaryValue);
  update->SetBoolean("chromeCleanupEnabled", cleanupEnabled);

  content::WebUIDataSource::Update(profile, chrome::kChromeUISettingsHost,
                                   std::move(update));
}
#endif  // defined(OS_WIN)

}  // namespace settings
