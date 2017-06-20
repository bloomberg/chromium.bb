// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/md_settings_ui.h"

#include <stddef.h>

#include <string>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
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
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

#if defined(OS_WIN)
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/webui/settings/chrome_cleanup_handler.h"
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/settings/languages_handler.h"
#endif  // defined(OS_WIN) || defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
#include "ash/system/palette/palette_utils.h"
#include "ash/system/power/power_status.h"
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
#include "chrome/browser/ui/webui/settings/chromeos/internet_handler.h"
#include "chrome/common/chrome_switches.h"
#include "components/arc/arc_util.h"
#else  // !defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/settings/settings_default_browser_handler.h"
#include "chrome/browser/ui/webui/settings/settings_manage_profile_handler.h"
#include "chrome/browser/ui/webui/settings/system_handler.h"
#endif  // defined(OS_CHROMEOS)

#if defined(USE_NSS_CERTS)
#include "chrome/browser/ui/webui/settings/certificates_handler.h"
#elif defined(OS_WIN) || defined(OS_MACOSX)
#include "chrome/browser/ui/webui/settings/native_certificates_handler.h"
#endif  // defined(USE_NSS_CERTS)

namespace settings {

bool IsValidOrigin(const GURL& url) {
  const GURL origin = url.GetOrigin();
  return origin == GURL(chrome::kChromeUISettingsURL).GetOrigin() ||
         origin == GURL(chrome::kChromeUIMdSettingsURL).GetOrigin();
}

// static
void MdSettingsUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kImportDialogAutofillFormData, true);
  registry->RegisterBooleanPref(prefs::kImportDialogBookmarks, true);
  registry->RegisterBooleanPref(prefs::kImportDialogHistory, true);
  registry->RegisterBooleanPref(prefs::kImportDialogSavedPasswords, true);
  registry->RegisterBooleanPref(prefs::kImportDialogSearchEngine, true);
}

MdSettingsUI::MdSettingsUI(content::WebUI* web_ui, const GURL& url)
    : content::WebUIController(web_ui),
      WebContentsObserver(web_ui->GetWebContents()) {
  Profile* profile = Profile::FromWebUI(web_ui);
  AddSettingsPageUIHandler(base::MakeUnique<AppearanceHandler>(web_ui));

#if defined(USE_NSS_CERTS)
  AddSettingsPageUIHandler(base::MakeUnique<CertificatesHandler>(false));
#elif defined(OS_WIN) || defined(OS_MACOSX)
  AddSettingsPageUIHandler(base::MakeUnique<NativeCertificatesHandler>());
#endif  // defined(USE_NSS_CERTS)

  AddSettingsPageUIHandler(base::MakeUnique<BrowserLifetimeHandler>());
  AddSettingsPageUIHandler(base::MakeUnique<ClearBrowsingDataHandler>(web_ui));
  AddSettingsPageUIHandler(base::MakeUnique<CookiesViewHandler>());
  AddSettingsPageUIHandler(base::MakeUnique<DownloadsHandler>(profile));
  AddSettingsPageUIHandler(base::MakeUnique<ExtensionControlHandler>());
  AddSettingsPageUIHandler(base::MakeUnique<FontHandler>(web_ui));
  AddSettingsPageUIHandler(base::MakeUnique<ImportDataHandler>());

#if defined(OS_WIN) || defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(base::MakeUnique<LanguagesHandler>(web_ui));
#endif  // defined(OS_WIN) || defined(OS_CHROMEOS)

  AddSettingsPageUIHandler(
      base::MakeUnique<MediaDevicesSelectionHandler>(profile));
#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(base::MakeUnique<MetricsReportingHandler>());
#endif
  AddSettingsPageUIHandler(base::MakeUnique<OnStartupHandler>());
  AddSettingsPageUIHandler(base::MakeUnique<PeopleHandler>(profile));
  AddSettingsPageUIHandler(base::MakeUnique<ProfileInfoHandler>(profile));
  AddSettingsPageUIHandler(base::MakeUnique<ProtocolHandlersHandler>());
  AddSettingsPageUIHandler(
      base::MakeUnique<SafeBrowsingHandler>(profile->GetPrefs()));
  AddSettingsPageUIHandler(base::MakeUnique<SearchEnginesHandler>(profile));
  AddSettingsPageUIHandler(base::MakeUnique<SiteSettingsHandler>(profile));
  AddSettingsPageUIHandler(base::MakeUnique<StartupPagesHandler>(web_ui));

#if defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::AccessibilityHandler>(web_ui));
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::AndroidAppsHandler>(profile));
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::ChangePictureHandler>());
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::CupsPrintersHandler>(web_ui));
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::FingerprintHandler>(profile));
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::KeyboardHandler>(web_ui));
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::PointerHandler>());
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::StorageHandler>());
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::StylusHandler>());
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::InternetHandler>());
#else
  AddSettingsPageUIHandler(base::MakeUnique<DefaultBrowserHandler>(web_ui));
  AddSettingsPageUIHandler(base::MakeUnique<ManageProfileHandler>(profile));
  AddSettingsPageUIHandler(base::MakeUnique<SystemHandler>());
#endif

  // Host must be derived from the visible URL, since this might be serving
  // either chrome://settings or chrome://md-settings.
  CHECK(IsValidOrigin(url));

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(url.host());
  html_source->AddString("hostname", url.host());

#if defined(OS_WIN)
  if (base::FeatureList::IsEnabled(safe_browsing::kInBrowserCleanerUIFeature) &&
      safe_browsing::ChromeCleanerController::ShouldShowCleanupInSettingsUI()) {
    AddSettingsPageUIHandler(base::MakeUnique<ChromeCleanupHandler>(profile));
    html_source->AddBoolean("chromeCleanupEnabled", true);
  }
#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)
  chromeos::settings::EasyUnlockSettingsHandler* easy_unlock_handler =
      chromeos::settings::EasyUnlockSettingsHandler::Create(html_source,
                                                            profile);
  if (easy_unlock_handler)
    AddSettingsPageUIHandler(base::WrapUnique(easy_unlock_handler));

  AddSettingsPageUIHandler(base::WrapUnique(
      chromeos::settings::DateTimeHandler::Create(html_source)));

  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::StylusHandler>());
  html_source->AddBoolean(
      "quickUnlockEnabled",
      chromeos::quick_unlock::IsPinEnabled(profile->GetPrefs()));
  html_source->AddBoolean("fingerprintUnlockEnabled",
                          chromeos::quick_unlock::IsFingerprintEnabled());
  html_source->AddBoolean("androidAppsVisible",
                          arc::IsArcAllowedForProfile(profile) &&
                              !arc::IsArcOptInVerificationDisabled() &&
                              arc::IsPlayStoreAvailable());

  // TODO(mash): Support Chrome power settings in Mash. crbug.com/644348
  bool enable_power_settings = !ash_util::IsRunningInMash();
  html_source->AddBoolean("enablePowerSettings", enable_power_settings);
  if (enable_power_settings) {
    AddSettingsPageUIHandler(base::MakeUnique<chromeos::settings::PowerHandler>(
        profile->GetPrefs()));
  }
#endif

  AddSettingsPageUIHandler(
      base::WrapUnique(AboutHandler::Create(html_source, profile)));
  AddSettingsPageUIHandler(
      base::WrapUnique(ResetSettingsHandler::Create(html_source, profile)));

  // Add the metrics handler to write uma stats.
  web_ui->AddMessageHandler(base::MakeUnique<MetricsHandler>());

#if BUILDFLAG(USE_VULCANIZE)
  html_source->AddResourcePath("crisper.js", IDR_MD_SETTINGS_CRISPER_JS);
  html_source->AddResourcePath("lazy_load.crisper.js",
                               IDR_MD_SETTINGS_LAZY_LOAD_CRISPER_JS);
  html_source->AddResourcePath("lazy_load.html",
                               IDR_MD_SETTINGS_LAZY_LOAD_VULCANIZED_HTML);
  html_source->SetDefaultResource(IDR_MD_SETTINGS_VULCANIZED_HTML);
  html_source->UseGzip(std::unordered_set<std::string>());
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
}

MdSettingsUI::~MdSettingsUI() {
}

void MdSettingsUI::AddSettingsPageUIHandler(
    std::unique_ptr<SettingsPageUIHandler> handler) {
  DCHECK(handler);
  handlers_.insert(handler.get());
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

}  // namespace settings
