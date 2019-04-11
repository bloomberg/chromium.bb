// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_ui.h"

#include <memory>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/stylus_utils.h"
#include "chrome/browser/chromeos/android_sms/android_sms_app_manager.h"
#include "chrome/browser/chromeos/android_sms/android_sms_service_factory.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/android_apps_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/crostini_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/date_time_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_keyboard_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_pointer_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_power_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_storage_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_stylus_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/fingerprint_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/google_assistant_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/internet_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/multidevice_handler.h"
#include "chrome/browser/ui/webui/settings/tts_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/os_settings_resources.h"
#include "chrome/grit/os_settings_resources_map.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/arc/arc_util.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/ui_base_features.h"

namespace chromeos {
namespace settings {

OSSettingsUI::OSSettingsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
#if !BUILDFLAG(OPTIMIZE_WEBUI)
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIOSSettingsHost);

  InitWebUIHandlers(profile, web_ui, html_source);

  html_source->SetDefaultResource(IDR_SETTINGS_OS_SETTINGS_HTML);

  content::WebUIDataSource::Add(profile, html_source);
#endif  //! BUILDFLAG(OPTIMIZE_WEBUI)

  // TODO(maybelle): Add optimized build
}

OSSettingsUI::~OSSettingsUI() = default;

// static
void OSSettingsUI::InitWebUIHandlers(Profile* profile,
                                     content::WebUI* web_ui,
                                     content::WebUIDataSource* html_source) {
  web_ui->AddMessageHandler(std::make_unique<AccessibilityHandler>(web_ui));
  web_ui->AddMessageHandler(std::make_unique<AndroidAppsHandler>(profile));
  if (crostini::IsCrostiniUIAllowedForProfile(profile,
                                              false /* check_policy */)) {
    web_ui->AddMessageHandler(std::make_unique<CrostiniHandler>(profile));
  }
  web_ui->AddMessageHandler(CupsPrintersHandler::Create(web_ui));
  web_ui->AddMessageHandler(
      base::WrapUnique(DateTimeHandler::Create(html_source)));
  web_ui->AddMessageHandler(std::make_unique<FingerprintHandler>(profile));
  if (chromeos::switches::IsAssistantEnabled()) {
    web_ui->AddMessageHandler(
        std::make_unique<GoogleAssistantHandler>(profile));
  }
  web_ui->AddMessageHandler(std::make_unique<KeyboardHandler>());
  web_ui->AddMessageHandler(std::make_unique<PointerHandler>());
  web_ui->AddMessageHandler(std::make_unique<StorageHandler>(profile));
  web_ui->AddMessageHandler(std::make_unique<StylusHandler>());
  web_ui->AddMessageHandler(std::make_unique<InternetHandler>(profile));
  web_ui->AddMessageHandler(std::make_unique<::settings::TtsHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::smb_dialog::SmbHandler>(profile));

  if (!profile->IsGuestSession()) {
    chromeos::android_sms::AndroidSmsService* android_sms_service =
        chromeos::android_sms::AndroidSmsServiceFactory::GetForBrowserContext(
            profile);
    web_ui->AddMessageHandler(std::make_unique<MultideviceHandler>(
        profile->GetPrefs(),
        chromeos::multidevice_setup::MultiDeviceSetupClientFactory::
            GetForProfile(profile),
        android_sms_service
            ? android_sms_service->android_sms_pairing_state_tracker()
            : nullptr,
        android_sms_service ? android_sms_service->android_sms_app_manager()
                            : nullptr));
  }

  html_source->AddBoolean(
      "multideviceAllowedByPolicy",
      chromeos::multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(
          profile->GetPrefs()));
  html_source->AddBoolean(
      "quickUnlockEnabled",
      chromeos::quick_unlock::IsPinEnabled(profile->GetPrefs()));
  html_source->AddBoolean(
      "quickUnlockDisabledByPolicy",
      chromeos::quick_unlock::IsPinDisabledByPolicy(profile->GetPrefs()));
  const bool fingerprint_unlock_enabled =
      chromeos::quick_unlock::IsFingerprintEnabled(profile);
  html_source->AddBoolean("fingerprintUnlockEnabled",
                          fingerprint_unlock_enabled);
  if (fingerprint_unlock_enabled) {
    html_source->AddBoolean(
        "isFingerprintReaderOnKeyboard",
        chromeos::quick_unlock::IsFingerprintReaderOnKeyboard());
  }
  html_source->AddBoolean("lockScreenNotificationsEnabled",
                          ash::features::IsLockScreenNotificationsEnabled());
  html_source->AddBoolean(
      "lockScreenHideSensitiveNotificationsSupported",
      ash::features::IsLockScreenHideSensitiveNotificationsSupported());
  html_source->AddBoolean("showTechnologyBadge",
                          !ash::features::IsSeparateNetworkIconsEnabled());
  html_source->AddBoolean("hasInternalStylus",
                          ash::stylus_utils::HasInternalStylus());

  html_source->AddBoolean(
      "showKioskNextShell",
      base::FeatureList::IsEnabled(ash::features::kKioskNextShell));

  html_source->AddBoolean("showCrostini",
                          crostini::IsCrostiniUIAllowedForProfile(
                              profile, false /* check_policy */));

  html_source->AddBoolean("allowCrostini",
                          crostini::IsCrostiniUIAllowedForProfile(profile));

  html_source->AddBoolean("showPluginVm",
                          plugin_vm::IsPluginVmEnabled(profile));

  html_source->AddBoolean("isDemoSession",
                          chromeos::DemoSession::IsDeviceInDemoMode());

  html_source->AddBoolean("assistantEnabled",
                          chromeos::switches::IsAssistantEnabled());

  // We have 2 variants of Android apps settings. Default case, when the Play
  // Store app exists we show expandable section that allows as to
  // enable/disable the Play Store and link to Android settings which is
  // available once settings app is registered in the system.
  // For AOSP images we don't have the Play Store app. In last case we Android
  // apps settings consists only from root link to Android settings and only
  // visible once settings app is registered.
  html_source->AddBoolean("androidAppsVisible",
                          arc::IsArcAllowedForProfile(profile));
  html_source->AddBoolean("havePlayStoreApp", arc::IsPlayStoreAvailable());

  // TODO(mash): Support Chrome power settings in Mash. https://crbug.com/644348
  bool enable_power_settings = !::features::IsMultiProcessMash();
  html_source->AddBoolean("enablePowerSettings", enable_power_settings);
  if (enable_power_settings) {
    web_ui->AddMessageHandler(
        std::make_unique<PowerHandler>(profile->GetPrefs()));
  }
}

}  // namespace settings
}  // namespace chromeos
