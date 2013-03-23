// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags_ui.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/user_prefs/pref_registry_syncable.h"
#endif

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateFlagsUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIFlagsHost);

  source->SetUseJsonJSFormatV2();
  source->AddLocalizedString("flagsLongTitle", IDS_FLAGS_LONG_TITLE);
  source->AddLocalizedString("flagsTableTitle", IDS_FLAGS_TABLE_TITLE);
  source->AddLocalizedString("flagsNoExperimentsAvailable",
                             IDS_FLAGS_NO_EXPERIMENTS_AVAILABLE);
  source->AddLocalizedString("flagsWarningHeader", IDS_FLAGS_WARNING_HEADER);
  source->AddLocalizedString("flagsBlurb", IDS_FLAGS_WARNING_TEXT);
  source->AddLocalizedString("flagsNotSupported", IDS_FLAGS_NOT_AVAILABLE);
  source->AddLocalizedString("flagsRestartNotice", IDS_FLAGS_RELAUNCH_NOTICE);
  source->AddLocalizedString("flagsRestartButton", IDS_FLAGS_RELAUNCH_BUTTON);
  source->AddLocalizedString("resetAllButton", IDS_FLAGS_RESET_ALL_BUTTON);
  source->AddLocalizedString("disable", IDS_FLAGS_DISABLE);
  source->AddLocalizedString("enable", IDS_FLAGS_ENABLE);
#if defined(OS_CHROMEOS)
  if (!chromeos::UserManager::Get()->IsCurrentUserOwner() &&
      base::chromeos::IsRunningOnChromeOS()) {
    // Set the strings to show which user can actually change the flags.
    std::string owner;
    chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);
    source->AddString("ownerWarning",
                      l10n_util::GetStringFUTF16(IDS_SYSTEM_FLAGS_OWNER_ONLY,
                                                 UTF8ToUTF16(owner)));
  } else {
    source->AddString("ownerWarning", string16());
  }
#endif

  source->SetJsonPath("strings.js");
  source->AddResourcePath("flags.js", IDR_FLAGS_JS);
  source->SetDefaultResource(IDR_FLAGS_HTML);
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// FlagsDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the about:flags page.
class FlagsDOMHandler : public WebUIMessageHandler {
 public:
  explicit FlagsDOMHandler(PrefService* prefs) : prefs_(prefs) {}
  virtual ~FlagsDOMHandler() {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "requestFlagsExperiments" message.
  void HandleRequestFlagsExperiments(const ListValue* args);

  // Callback for the "enableFlagsExperiment" message.
  void HandleEnableFlagsExperimentMessage(const ListValue* args);

  // Callback for the "restartBrowser" message. Restores all tabs on restart.
  void HandleRestartBrowser(const ListValue* args);

  // Callback for the "resetAllFlags" message.
  void HandleResetAllFlags(const ListValue* args);

 private:
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(FlagsDOMHandler);
};

void FlagsDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestFlagsExperiments",
      base::Bind(&FlagsDOMHandler::HandleRequestFlagsExperiments,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("enableFlagsExperiment",
      base::Bind(&FlagsDOMHandler::HandleEnableFlagsExperimentMessage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("restartBrowser",
      base::Bind(&FlagsDOMHandler::HandleRestartBrowser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("resetAllFlags",
      base::Bind(&FlagsDOMHandler::HandleResetAllFlags,
                 base::Unretained(this)));
}

void FlagsDOMHandler::HandleRequestFlagsExperiments(const ListValue* args) {
  DictionaryValue results;
  results.Set("flagsExperiments",
              about_flags::GetFlagsExperimentsData(prefs_));
  results.SetBoolean("needsRestart",
                     about_flags::IsRestartNeededToCommitChanges());
  web_ui()->CallJavascriptFunction("returnFlagsExperiments", results);
}

void FlagsDOMHandler::HandleEnableFlagsExperimentMessage(
    const ListValue* args) {
  DCHECK_EQ(2u, args->GetSize());
  if (args->GetSize() != 2)
    return;

  std::string experiment_internal_name;
  std::string enable_str;
  if (!args->GetString(0, &experiment_internal_name) ||
      !args->GetString(1, &enable_str))
    return;

  about_flags::SetExperimentEnabled(
      prefs_,
      experiment_internal_name,
      enable_str == "true");
}

void FlagsDOMHandler::HandleRestartBrowser(const ListValue* args) {
  chrome::AttemptRestart();
}

void FlagsDOMHandler::HandleResetAllFlags(const ListValue* args) {
  about_flags::ResetAllFlags(g_browser_process->local_state());
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// FlagsUI
//
///////////////////////////////////////////////////////////////////////////////

FlagsUI::FlagsUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  Profile* profile = Profile::FromWebUI(web_ui);

#if defined(OS_CHROMEOS)
  chromeos::DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::Bind(&FlagsUI::FinishInitialization,
                 weak_factory_.GetWeakPtr(), profile));
#else
  web_ui->AddMessageHandler(
      new FlagsDOMHandler(g_browser_process->local_state()));

  // Set up the about:flags source.
  content::WebUIDataSource::Add(profile, CreateFlagsUIHTMLSource());
#endif
}

FlagsUI::~FlagsUI() {
}

// static
base::RefCountedMemory* FlagsUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytesForScale(IDR_FLAGS_FAVICON, scale_factor);
}

// static
void FlagsUI::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kEnabledLabsExperiments);
}

#if defined(OS_CHROMEOS)
// static
void FlagsUI::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kEnabledLabsExperiments,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void FlagsUI::FinishInitialization(
    Profile* profile,
    chromeos::DeviceSettingsService::OwnershipStatus status,
    bool current_user_is_owner) {
  // On Chrome OS the owner can set system wide flags and other users can only
  // set flags for their own session.
  if (!current_user_is_owner) {
    web_ui()->AddMessageHandler(new FlagsDOMHandler(profile->GetPrefs()));
  } else {
    web_ui()->AddMessageHandler(
        new FlagsDOMHandler(g_browser_process->local_state()));
    // If the owner managed to set the flags pref on his own profile clear it
    // because it will never be accessible anymore.
    if (profile->GetPrefs()->HasPrefPath(prefs::kEnabledLabsExperiments))
      profile->GetPrefs()->ClearPref(prefs::kEnabledLabsExperiments);
  }

  // Set up the about:flags source.
  content::WebUIDataSource::Add(profile, CreateFlagsUIHTMLSource());
}
#endif
