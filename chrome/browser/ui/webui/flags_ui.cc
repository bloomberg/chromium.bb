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
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/pref_service_flags_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_version_info.h"
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
#include "base/sys_info.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_factory.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/owner_flags_storage.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/pref_registry/pref_registry_syncable.h"
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
  source->AddLocalizedString("channelPromoBeta",
                             IDS_FLAGS_PROMOTE_BETA_CHANNEL);
  source->AddLocalizedString("channelPromoDev", IDS_FLAGS_PROMOTE_DEV_CHANNEL);
  source->AddLocalizedString("flagsUnsupportedTableTitle",
                             IDS_FLAGS_UNSUPPORTED_TABLE_TITLE);
  source->AddLocalizedString("flagsNoUnsupportedExperiments",
                             IDS_FLAGS_NO_UNSUPPORTED_EXPERIMENTS);
  source->AddLocalizedString("flagsNotSupported", IDS_FLAGS_NOT_AVAILABLE);
  source->AddLocalizedString("flagsRestartNotice", IDS_FLAGS_RELAUNCH_NOTICE);
  source->AddLocalizedString("flagsRestartButton", IDS_FLAGS_RELAUNCH_BUTTON);
  source->AddLocalizedString("resetAllButton", IDS_FLAGS_RESET_ALL_BUTTON);
  source->AddLocalizedString("disable", IDS_FLAGS_DISABLE);
  source->AddLocalizedString("enable", IDS_FLAGS_ENABLE);

#if defined(OS_CHROMEOS)
  if (!chromeos::UserManager::Get()->IsCurrentUserOwner() &&
      base::SysInfo::IsRunningOnChromeOS()) {
    // Set the strings to show which user can actually change the flags.
    std::string owner;
    chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);
    source->AddString("ownerWarning",
                      l10n_util::GetStringFUTF16(IDS_SYSTEM_FLAGS_OWNER_ONLY,
                                                 base::UTF8ToUTF16(owner)));
  } else {
    // The warning will be only shown on ChromeOS, when the current user is not
    // the owner.
    source->AddString("ownerWarning", base::string16());
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
  FlagsDOMHandler() : access_(about_flags::kGeneralAccessFlagsOnly),
                      flags_experiments_requested_(false) {
  }
  virtual ~FlagsDOMHandler() {}

  // Initializes the DOM handler with the provided flags storage and flags
  // access. If there were flags experiments requested from javascript before
  // this was called, it calls |HandleRequestFlagsExperiments| again.
  void Init(about_flags::FlagsStorage* flags_storage,
            about_flags::FlagAccess access);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "requestFlagsExperiments" message.
  void HandleRequestFlagsExperiments(const base::ListValue* args);

  // Callback for the "enableFlagsExperiment" message.
  void HandleEnableFlagsExperimentMessage(const base::ListValue* args);

  // Callback for the "restartBrowser" message. Restores all tabs on restart.
  void HandleRestartBrowser(const base::ListValue* args);

  // Callback for the "resetAllFlags" message.
  void HandleResetAllFlags(const base::ListValue* args);

 private:
  scoped_ptr<about_flags::FlagsStorage> flags_storage_;
  about_flags::FlagAccess access_;
  bool flags_experiments_requested_;

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

void FlagsDOMHandler::Init(about_flags::FlagsStorage* flags_storage,
                           about_flags::FlagAccess access) {
  flags_storage_.reset(flags_storage);
  access_ = access;

  if (flags_experiments_requested_)
    HandleRequestFlagsExperiments(NULL);
}

void FlagsDOMHandler::HandleRequestFlagsExperiments(
    const base::ListValue* args) {
  flags_experiments_requested_ = true;
  // Bail out if the handler hasn't been initialized yet. The request will be
  // handled after the initialization.
  if (!flags_storage_)
    return;

  base::DictionaryValue results;

  scoped_ptr<base::ListValue> supported_experiments(new base::ListValue);
  scoped_ptr<base::ListValue> unsupported_experiments(new base::ListValue);
  about_flags::GetFlagsExperimentsData(flags_storage_.get(),
                                       access_,
                                       supported_experiments.get(),
                                       unsupported_experiments.get());
  results.Set("supportedExperiments", supported_experiments.release());
  results.Set("unsupportedExperiments", unsupported_experiments.release());
  results.SetBoolean("needsRestart",
                     about_flags::IsRestartNeededToCommitChanges());
  results.SetBoolean("showOwnerWarning",
                     access_ == about_flags::kGeneralAccessFlagsOnly);

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  results.SetBoolean("showBetaChannelPromotion",
                     channel == chrome::VersionInfo::CHANNEL_STABLE);
  results.SetBoolean("showDevChannelPromotion",
                     channel == chrome::VersionInfo::CHANNEL_BETA);
#else
  results.SetBoolean("showBetaChannelPromotion", false);
  results.SetBoolean("showDevChannelPromotion", false);
#endif
  web_ui()->CallJavascriptFunction("returnFlagsExperiments", results);
}

void FlagsDOMHandler::HandleEnableFlagsExperimentMessage(
    const base::ListValue* args) {
  DCHECK(flags_storage_);
  DCHECK_EQ(2u, args->GetSize());
  if (args->GetSize() != 2)
    return;

  std::string experiment_internal_name;
  std::string enable_str;
  if (!args->GetString(0, &experiment_internal_name) ||
      !args->GetString(1, &enable_str))
    return;

  about_flags::SetExperimentEnabled(
      flags_storage_.get(),
      experiment_internal_name,
      enable_str == "true");
}

void FlagsDOMHandler::HandleRestartBrowser(const base::ListValue* args) {
  DCHECK(flags_storage_);
#if defined(OS_CHROMEOS)
  // On ChromeOS be less intrusive and restart inside the user session after
  // we apply the newly selected flags.
  CommandLine user_flags(CommandLine::NO_PROGRAM);
  about_flags::ConvertFlagsToSwitches(flags_storage_.get(),
                                      &user_flags,
                                      about_flags::kAddSentinels);
  CommandLine::StringVector flags;
  // argv[0] is the program name |CommandLine::NO_PROGRAM|.
  flags.assign(user_flags.argv().begin() + 1, user_flags.argv().end());
  VLOG(1) << "Restarting to apply per-session flags...";
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
      SetFlagsForUser(chromeos::UserManager::Get()->GetActiveUser()->email(),
                      flags);
#endif
  chrome::AttemptRestart();
}

void FlagsDOMHandler::HandleResetAllFlags(const base::ListValue* args) {
  DCHECK(flags_storage_);
  about_flags::ResetAllFlags(flags_storage_.get());
}


#if defined(OS_CHROMEOS)
// On ChromeOS verifying if the owner is signed in is async operation and only
// after finishing it the UI can be properly populated. This function is the
// callback for whether the owner is signed in. It will respectively pick the
// proper PrefService for the flags interface.
void FinishInitialization(base::WeakPtr<FlagsUI> flags_ui,
                          Profile* profile,
                          FlagsDOMHandler* dom_handler,
                          bool current_user_is_owner) {
  // If the flags_ui has gone away, there's nothing to do.
  if (!flags_ui)
    return;

  // On Chrome OS the owner can set system wide flags and other users can only
  // set flags for their own session.
  // Note that |dom_handler| is owned by the web ui that owns |flags_ui|, so
  // it is still alive if |flags_ui| is.
  if (current_user_is_owner) {
    dom_handler->Init(new chromeos::about_flags::OwnerFlagsStorage(
                          profile->GetPrefs(),
                          chromeos::CrosSettings::Get()),
                      about_flags::kOwnerAccessToFlags);
  } else {
    dom_handler->Init(
        new about_flags::PrefServiceFlagsStorage(profile->GetPrefs()),
        about_flags::kGeneralAccessFlagsOnly);
  }
}
#endif

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// FlagsUI
//
///////////////////////////////////////////////////////////////////////////////

FlagsUI::FlagsUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      weak_factory_(this) {
  Profile* profile = Profile::FromWebUI(web_ui);

  FlagsDOMHandler* handler = new FlagsDOMHandler();
  web_ui->AddMessageHandler(handler);

#if defined(OS_CHROMEOS)
  chromeos::OwnerSettingsService* service =
      chromeos::OwnerSettingsServiceFactory::GetForProfile(profile);
  if (service) {
    service->IsOwnerAsync(base::Bind(
        &FinishInitialization, weak_factory_.GetWeakPtr(), profile, handler));
  } else {
    FinishInitialization(weak_factory_.GetWeakPtr(),
                         profile,
                         handler,
                         false /* current_user_is_owner */);
  }
#else
  handler->Init(new about_flags::PrefServiceFlagsStorage(
                    g_browser_process->local_state()),
                about_flags::kOwnerAccessToFlags);
#endif

  // Set up the about:flags source.
  content::WebUIDataSource::Add(profile, CreateFlagsUIHTMLSource());
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
void FlagsUI::RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kEnabledLabsExperiments,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

#endif
