// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/kiosk_apps_handler.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/web_ui_util.h"

namespace chromeos {
namespace options {

namespace {

// Populates app info dictionary with |app_data|.
void PopulateAppDict(const KioskAppManager::App& app_data,
                     base::DictionaryValue* app_dict) {
  std::string icon_url("chrome://theme/IDR_APP_DEFAULT_ICON");

  // TODO(xiyuan): Replace data url with a URLDataSource.
  if (!app_data.icon.isNull())
    icon_url = webui::GetBitmapDataUrl(*app_data.icon.bitmap());

  app_dict->SetString("id", app_data.id);
  app_dict->SetString("name", app_data.name);
  app_dict->SetString("iconURL", icon_url);
  app_dict->SetBoolean(
      "autoLaunch",
      KioskAppManager::Get()->GetAutoLaunchApp() == app_data.id);
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
      initialized_(false) {
  kiosk_app_manager_->AddObserver(this);
}

KioskAppsHandler::~KioskAppsHandler() {
  kiosk_app_manager_->RemoveObserver(this);
}

void KioskAppsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("getKioskApps",
      base::Bind(&KioskAppsHandler::HandleGetKioskApps,
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
}

void KioskAppsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings,
                "kioskOverlayTitle",
                IDS_OPTIONS_KIOSK_OVERLAY_TITLE);

  localized_strings->SetString(
      "addKioskApp",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KIOSK_ADD_APP));
  localized_strings->SetString(
      "kioskAppIdEditHint",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KIOSK_ADD_APP_HINT));
  localized_strings->SetString(
      "enableAutoLaunchButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KIOSK_ENABLE_AUTO_LAUNCH));
  localized_strings->SetString(
      "disableAutoLaunchButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KIOSK_DISABLE_AUTO_LAUNCH));
  localized_strings->SetString(
      "autoLaunch",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KIOSK_AUTO_LAUNCH));
  localized_strings->SetString(
      "invalidApp",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KIOSK_INVALID_APP));
  localized_strings->SetString(
      "kioskDiableBailoutShortcutLabel",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_KIOSK_DISABLE_BAILOUT_SHORTCUT_LABEL));
  localized_strings->SetString(
      "kioskDisableBailoutShortcutWarningBold",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_KIOSK_DISABLE_BAILOUT_SHORTCUT_WARNING_BOLD));
  const string16 product_os_name =
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME);
  localized_strings->SetString(
      "kioskDisableBailoutShortcutWarning",
      l10n_util::GetStringFUTF16(
          IDS_OPTIONS_KIOSK_DISABLE_BAILOUT_SHORTCUT_WARNING_FORMAT,
          product_os_name));
  localized_strings->SetString(
      "kioskDisableBailoutShortcutConfirm",
      l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL));
  localized_strings->SetString(
      "kioskDisableBailoutShortcutCancel",
      l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL));
}

void KioskAppsHandler::OnKioskAppDataChanged(const std::string& app_id) {
  KioskAppManager::App app_data;
  if (!kiosk_app_manager_->GetApp(app_id, &app_data))
    return;

  base::DictionaryValue app_dict;
  PopulateAppDict(app_data, &app_dict);

  web_ui()->CallJavascriptFunction("options.KioskAppsOverlay.updateApp",
                                   app_dict);
}

void KioskAppsHandler::OnKioskAppDataLoadFailure(const std::string& app_id) {
  base::StringValue app_id_value(app_id);
  web_ui()->CallJavascriptFunction("options.KioskAppsOverlay.showError",
                                   app_id_value);
}

void KioskAppsHandler::SendKioskApps() {
  if (!initialized_)
    return;

  KioskAppManager::Apps apps;
  kiosk_app_manager_->GetApps(&apps);

  base::ListValue apps_list;
  for (size_t i = 0; i < apps.size(); ++i) {
    const KioskAppManager::App& app_data = apps[i];

    scoped_ptr<base::DictionaryValue> app_info(new base::DictionaryValue);
    PopulateAppDict(app_data, app_info.get());
    apps_list.Append(app_info.release());
  }

  web_ui()->CallJavascriptFunction("options.KioskAppsOverlay.setApps",
                                   apps_list);
}

void KioskAppsHandler::HandleGetKioskApps(const base::ListValue* args) {
  initialized_ = true;
  SendKioskApps();
}

void KioskAppsHandler::HandleAddKioskApp(const base::ListValue* args) {
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
  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  kiosk_app_manager_->RemoveApp(app_id);
}

void KioskAppsHandler::HandleEnableKioskAutoLaunch(
    const base::ListValue* args) {
  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  kiosk_app_manager_->SetAutoLaunchApp(app_id);
}

void KioskAppsHandler::HandleDisableKioskAutoLaunch(
    const base::ListValue* args) {
  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  std::string startup_app_id = kiosk_app_manager_->GetAutoLaunchApp();
  if (startup_app_id != app_id)
    return;

  kiosk_app_manager_->SetAutoLaunchApp("");
}

}  // namespace options
}  // namespace chromeos
