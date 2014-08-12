// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/chromeos/kiosk_apps_handler.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/common/extension.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

// Populates app info dictionary with |app_data|.
void PopulateAppDict(const KioskAppManager::App& app_data,
                     base::DictionaryValue* app_dict) {
  std::string icon_url("chrome://theme/IDR_APP_DEFAULT_ICON");

  // TODO(xiyuan): Replace data url with a URLDataSource.
  if (!app_data.icon.isNull())
    icon_url = webui::GetBitmapDataUrl(*app_data.icon.bitmap());

  app_dict->SetString("id", app_data.app_id);
  app_dict->SetString("name", app_data.name);
  app_dict->SetString("iconURL", icon_url);
  app_dict->SetBoolean(
      "autoLaunch",
      KioskAppManager::Get()->GetAutoLaunchApp() == app_data.app_id &&
      (KioskAppManager::Get()->IsAutoLaunchEnabled() ||
          KioskAppManager::Get()->IsAutoLaunchRequested()));
  app_dict->SetBoolean("isLoading", app_data.is_loading);
}

// Sanitize app id input value and extracts app id out of it.
// Returns false if an app id could not be derived out of the input.
bool ExtractsAppIdFromInput(const std::string& input,
                            std::string* app_id) {
  if (extensions::Extension::IdIsValid(input)) {
    *app_id = input;
    return true;
  }

  GURL webstore_url = GURL(input);
  if (!webstore_url.is_valid())
    return false;

  GURL webstore_base_url =
      GURL(extension_urls::GetWebstoreItemDetailURLPrefix());

  if (webstore_url.scheme() != webstore_base_url.scheme() ||
      webstore_url.host() != webstore_base_url.host() ||
      !StartsWithASCII(
          webstore_url.path(), webstore_base_url.path(), true)) {
    return false;
  }

  const std::string path = webstore_url.path();
  const size_t last_slash = path.rfind('/');
  if (last_slash == std::string::npos)
    return false;

  const std::string candidate_id = path.substr(last_slash + 1);
  if (!extensions::Extension::IdIsValid(candidate_id))
    return false;

  *app_id = candidate_id;
  return true;
}

}  // namespace

KioskAppsHandler::KioskAppsHandler()
    : kiosk_app_manager_(KioskAppManager::Get()),
      initialized_(false),
      is_kiosk_enabled_(false),
      is_auto_launch_enabled_(false),
      weak_ptr_factory_(this) {
  kiosk_app_manager_->AddObserver(this);
}

KioskAppsHandler::~KioskAppsHandler() {
  kiosk_app_manager_->RemoveObserver(this);
}

void KioskAppsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("initializeKioskAppSettings",
      base::Bind(&KioskAppsHandler::HandleInitializeKioskAppSettings,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getKioskAppSettings",
      base::Bind(&KioskAppsHandler::HandleGetKioskAppSettings,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("addKioskApp",
      base::Bind(&KioskAppsHandler::HandleAddKioskApp,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeKioskApp",
      base::Bind(&KioskAppsHandler::HandleRemoveKioskApp,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("enableKioskAutoLaunch",
      base::Bind(&KioskAppsHandler::HandleEnableKioskAutoLaunch,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("disableKioskAutoLaunch",
      base::Bind(&KioskAppsHandler::HandleDisableKioskAutoLaunch,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setDisableBailoutShortcut",
      base::Bind(&KioskAppsHandler::HandleSetDisableBailoutShortcut,
                 base::Unretained(this)));
}

void KioskAppsHandler::GetLocalizedValues(content::WebUIDataSource* source) {
  source->AddString(
      "addKioskAppButton",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_ADD_KIOSK_APP_BUTTON));
  source->AddString(
      "kioskOverlayTitle",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KIOSK_OVERLAY_TITLE));
  source->AddString(
      "addKioskApp",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KIOSK_ADD_APP));
  source->AddString(
      "kioskAppIdEditHint",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KIOSK_ADD_APP_HINT));
  source->AddString(
      "enableAutoLaunchButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KIOSK_ENABLE_AUTO_LAUNCH));
  source->AddString(
      "disableAutoLaunchButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KIOSK_DISABLE_AUTO_LAUNCH));
  source->AddString(
      "autoLaunch",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KIOSK_AUTO_LAUNCH));
  source->AddString(
      "invalidApp",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KIOSK_INVALID_APP));
  source->AddString(
      "kioskDiableBailoutShortcutLabel",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_KIOSK_DISABLE_BAILOUT_SHORTCUT_LABEL));
  source->AddString(
      "kioskDisableBailoutShortcutWarningBold",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_KIOSK_DISABLE_BAILOUT_SHORTCUT_WARNING_BOLD));
  const base::string16 product_os_name =
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME);
  source->AddString(
      "kioskDisableBailoutShortcutWarning",
      l10n_util::GetStringFUTF16(
          IDS_OPTIONS_KIOSK_DISABLE_BAILOUT_SHORTCUT_WARNING_FORMAT,
          product_os_name));
  source->AddString(
      "kioskDisableBailoutShortcutConfirm",
      l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL));
  source->AddString(
      "kioskDisableBailoutShortcutCancel",
      l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL));
  source->AddString("done", l10n_util::GetStringUTF16(IDS_DONE));
  source->AddString("add", l10n_util::GetStringUTF16(IDS_ADD));
}

void KioskAppsHandler::OnKioskAppDataChanged(const std::string& app_id) {
  UpdateApp(app_id);
}

void KioskAppsHandler::OnKioskAppDataLoadFailure(const std::string& app_id) {
  ShowError(app_id);
}

void KioskAppsHandler::OnKioskExtensionLoadedInCache(
    const std::string& app_id) {
  UpdateApp(app_id);
}

void KioskAppsHandler::OnKioskExtensionDownloadFailed(
    const std::string& app_id) {
  ShowError(app_id);
}

void KioskAppsHandler::OnGetConsumerKioskAutoLaunchStatus(
    chromeos::KioskAppManager::ConsumerKioskAutoLaunchStatus status) {
  initialized_ = true;
  is_kiosk_enabled_ =
      chromeos::UserManager::Get()->IsCurrentUserOwner() ||
      !base::SysInfo::IsRunningOnChromeOS();

  is_auto_launch_enabled_ =
      status == KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_ENABLED ||
      !base::SysInfo::IsRunningOnChromeOS();

  if (is_kiosk_enabled_) {
    base::DictionaryValue kiosk_params;
    kiosk_params.SetBoolean("kioskEnabled", is_kiosk_enabled_);
    kiosk_params.SetBoolean("autoLaunchEnabled", is_auto_launch_enabled_);
    web_ui()->CallJavascriptFunction("extensions.KioskAppsOverlay.enableKiosk",
                                     kiosk_params);
  }
}


void KioskAppsHandler::OnKioskAppsSettingsChanged() {
  SendKioskAppSettings();
}

void KioskAppsHandler::SendKioskAppSettings() {
  if (!initialized_ || !is_kiosk_enabled_)
    return;

  bool enable_bailout_shortcut;
  if (!CrosSettings::Get()->GetBoolean(
          kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled,
          &enable_bailout_shortcut)) {
    enable_bailout_shortcut = true;
  }

  base::DictionaryValue settings;
  settings.SetBoolean("disableBailout", !enable_bailout_shortcut);
  settings.SetBoolean("hasAutoLaunchApp",
                      !kiosk_app_manager_->GetAutoLaunchApp().empty());

  KioskAppManager::Apps apps;
  kiosk_app_manager_->GetApps(&apps);

  scoped_ptr<base::ListValue> apps_list(new base::ListValue);
  for (size_t i = 0; i < apps.size(); ++i) {
    const KioskAppManager::App& app_data = apps[i];

    scoped_ptr<base::DictionaryValue> app_info(new base::DictionaryValue);
    PopulateAppDict(app_data, app_info.get());
    apps_list->Append(app_info.release());
  }
  settings.SetWithoutPathExpansion("apps", apps_list.release());

  web_ui()->CallJavascriptFunction("extensions.KioskAppsOverlay.setSettings",
                                   settings);
}

void KioskAppsHandler::HandleInitializeKioskAppSettings(
    const base::ListValue* args) {
  KioskAppManager::Get()->GetConsumerKioskAutoLaunchStatus(
      base::Bind(&KioskAppsHandler::OnGetConsumerKioskAutoLaunchStatus,
                 weak_ptr_factory_.GetWeakPtr()));
}

void KioskAppsHandler::HandleGetKioskAppSettings(const base::ListValue* args) {
  SendKioskAppSettings();
}


void KioskAppsHandler::HandleAddKioskApp(const base::ListValue* args) {
  if (!initialized_ || !is_kiosk_enabled_)
    return;

  std::string input;
  CHECK(args->GetString(0, &input));

  std::string app_id;
  if (!ExtractsAppIdFromInput(input, &app_id)) {
    OnKioskAppDataLoadFailure(input);
    return;
  }

  kiosk_app_manager_->AddApp(app_id);
}

void KioskAppsHandler::HandleRemoveKioskApp(const base::ListValue* args) {
  if (!initialized_ || !is_kiosk_enabled_)
    return;

  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  kiosk_app_manager_->RemoveApp(app_id);
}

void KioskAppsHandler::HandleEnableKioskAutoLaunch(
    const base::ListValue* args) {
  if (!initialized_ || !is_kiosk_enabled_ || !is_auto_launch_enabled_)
    return;

  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  kiosk_app_manager_->SetAutoLaunchApp(app_id);
}

void KioskAppsHandler::HandleDisableKioskAutoLaunch(
    const base::ListValue* args) {
  if (!initialized_ || !is_kiosk_enabled_ || !is_auto_launch_enabled_)
    return;

  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  std::string startup_app_id = kiosk_app_manager_->GetAutoLaunchApp();
  if (startup_app_id != app_id)
    return;

  kiosk_app_manager_->SetAutoLaunchApp("");
}

void KioskAppsHandler::HandleSetDisableBailoutShortcut(
    const base::ListValue* args) {
  if (!initialized_ || !is_kiosk_enabled_)
    return;

  bool disable_bailout_shortcut;
  CHECK(args->GetBoolean(0, &disable_bailout_shortcut));

  CrosSettings::Get()->SetBoolean(
      kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled,
      !disable_bailout_shortcut);
}

void KioskAppsHandler::UpdateApp(const std::string& app_id) {
  KioskAppManager::App app_data;
  if (!kiosk_app_manager_->GetApp(app_id, &app_data))
    return;

  base::DictionaryValue app_dict;
  PopulateAppDict(app_data, &app_dict);

  web_ui()->CallJavascriptFunction("extensions.KioskAppsOverlay.updateApp",
                                   app_dict);
}

void KioskAppsHandler::ShowError(const std::string& app_id) {
  base::StringValue app_id_value(app_id);
  web_ui()->CallJavascriptFunction("extensions.KioskAppsOverlay.showError",
                                   app_id_value);

  kiosk_app_manager_->RemoveApp(app_id);
}

}  // namespace chromeos
