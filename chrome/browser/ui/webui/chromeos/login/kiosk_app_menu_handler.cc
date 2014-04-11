// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/kiosk_app_menu_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {

namespace {

// JS functions that define new and old kiosk UI API.
const char kKioskSetAppsNewAPI[] = "login.AccountPickerScreen.setApps";
const char kKioskSetAppsOldAPI[] = "login.AppsMenuButton.setApps";
const char kKioskShowErrorNewAPI[] = "login.AccountPickerScreen.showAppError";
const char kKioskShowErrorOldAPI[] = "login.AppsMenuButton.showError";

// Default app icon size.
const char kDefaultAppIconSizeString[] = "96px";
const int kMaxAppIconSize = 160;

}  // namespace

KioskAppMenuHandler::KioskAppMenuHandler()
    : weak_ptr_factory_(this),
      is_webui_initialized_(false) {
  KioskAppManager::Get()->AddObserver(this);
}

KioskAppMenuHandler::~KioskAppMenuHandler() {
  KioskAppManager::Get()->RemoveObserver(this);
}

void KioskAppMenuHandler::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString(
      "showApps",
      l10n_util::GetStringUTF16(IDS_KIOSK_APPS_BUTTON));
  localized_strings->SetString(
      "confirmKioskAppDiagnosticModeFormat",
      l10n_util::GetStringUTF16(IDS_LOGIN_CONFIRM_KIOSK_DIAGNOSTIC_FORMAT));
  localized_strings->SetString(
      "confirmKioskAppDiagnosticModeYes",
      l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL));
  localized_strings->SetString(
      "confirmKioskAppDiagnosticModeNo",
      l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL));
}

void KioskAppMenuHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("initializeKioskApps",
      base::Bind(&KioskAppMenuHandler::HandleInitializeKioskApps,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("kioskAppsLoaded",
      base::Bind(&KioskAppMenuHandler::HandleKioskAppsLoaded,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("checkKioskAppLaunchError",
      base::Bind(&KioskAppMenuHandler::HandleCheckKioskAppLaunchError,
                 base::Unretained(this)));
}

// static
bool KioskAppMenuHandler::EnableNewKioskUI() {
  bool show_user_pods;
  CrosSettings::Get()->GetBoolean(kAccountsPrefShowUserNamesOnSignIn,
                                  &show_user_pods);
  return !CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableNewKioskUI) && show_user_pods;
}

void KioskAppMenuHandler::SendKioskApps() {
  if (!is_webui_initialized_)
    return;

  KioskAppManager::Apps apps;
  KioskAppManager::Get()->GetApps(&apps);

  base::ListValue apps_list;
  for (size_t i = 0; i < apps.size(); ++i) {
    const KioskAppManager::App& app_data = apps[i];

    scoped_ptr<base::DictionaryValue> app_info(new base::DictionaryValue);
    app_info->SetBoolean("isApp", true);
    app_info->SetString("id", app_data.app_id);
    app_info->SetString("label", app_data.name);

    // TODO(xiyuan): Replace data url with a URLDataSource.
    std::string icon_url("chrome://theme/IDR_APP_DEFAULT_ICON");

    if (!app_data.icon.isNull()) {
      icon_url = webui::GetBitmapDataUrl(*app_data.icon.bitmap());
      int width = app_data.icon.width();
      int height = app_data.icon.height();

      // If app icon size is larger than default 160x160 then don't provide
      // size at all since it's already limited on the css side.
      if (width <= kMaxAppIconSize && height <= kMaxAppIconSize) {
        app_info->SetString("iconWidth", base::IntToString(width) + "px");
        app_info->SetString("iconHeight", base::IntToString(height) + "px");
      }
    } else {
      app_info->SetString("iconWidth", kDefaultAppIconSizeString);
      app_info->SetString("iconHeight", kDefaultAppIconSizeString);
    }
    app_info->SetString("iconUrl", icon_url);

    apps_list.Append(app_info.release());
  }


  web_ui()->CallJavascriptFunction(EnableNewKioskUI() ?
      kKioskSetAppsNewAPI : kKioskSetAppsOldAPI,
      apps_list);
}

void KioskAppMenuHandler::HandleInitializeKioskApps(
    const base::ListValue* args) {
  is_webui_initialized_ = true;
  SendKioskApps();
}

void KioskAppMenuHandler::HandleKioskAppsLoaded(
    const base::ListValue* args) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_KIOSK_APPS_LOADED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void KioskAppMenuHandler::HandleCheckKioskAppLaunchError(
    const base::ListValue* args) {
  KioskAppLaunchError::Error error = KioskAppLaunchError::Get();
  if (error == KioskAppLaunchError::NONE)
    return;
  KioskAppLaunchError::Clear();

  const std::string error_message = KioskAppLaunchError::GetErrorMessage(error);
  bool new_kiosk_ui = EnableNewKioskUI();
  web_ui()->CallJavascriptFunction(new_kiosk_ui ?
      kKioskShowErrorNewAPI : kKioskShowErrorOldAPI,
      base::StringValue(error_message));
}

void KioskAppMenuHandler::OnKioskAppsSettingsChanged() {
  SendKioskApps();
}

void KioskAppMenuHandler::OnKioskAppDataChanged(const std::string& app_id) {
  SendKioskApps();
}

}  // namespace chromeos
