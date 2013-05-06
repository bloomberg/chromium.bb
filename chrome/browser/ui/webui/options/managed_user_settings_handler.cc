// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/managed_user_settings_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "googleurl/src/url_canon.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/locale_settings.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/managed_mode_policy_provider.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#endif

using content::UserMetricsAction;

namespace {

const char* kSetting = "setting";
const char* kPattern = "pattern";

// Takes the |host| string and tries to get the canonical version. Returns
// whether the operation succeeded, and if it did and the |output_string| is
// not NULL writes the canonical version in |output_string|.
bool CanonicalizeHost(std::string host, std::string* output_string) {
  url_parse::Component in_comp(0, host.length());
  url_parse::Component out_comp;
  std::string output;

  url_canon::StdStringCanonOutput canon_output(&output);
  url_canon::CanonHostInfo host_info;

  bool is_valid = url_canon::CanonicalizeHost(
      host.c_str(), in_comp, &canon_output, &out_comp);
  canon_output.Complete();
  if (is_valid && output_string)
    *output_string = output;
  return is_valid;
}

// Create a DictionaryValue that will act as a row in the pattern list.
// Ownership of the pointer is passed to the caller.
DictionaryValue* GetEntryForPattern(const std::string pattern,
                                    const std::string setting) {
  base::DictionaryValue* entry = new base::DictionaryValue();
  entry->SetString(kPattern, pattern);
  entry->SetString(kSetting, setting);
  return entry;
}

// Reads the manual url and host lists from the current profile and adds them
// to the list of |entries|.
void AddCurrentURLEntries(content::WebUI* web_ui, ListValue* entries) {
  Profile* profile = Profile::FromWebUI(web_ui);
  const DictionaryValue* dict =
      profile->GetPrefs()->GetDictionary(prefs::kManagedModeManualHosts);
  for (DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    bool allow = false;
    bool result = it.value().GetAsBoolean(&allow);
    DCHECK(result);
    entries->Append(
        GetEntryForPattern(it.key(), allow ? "allow" : "block"));
  }

  dict = profile->GetPrefs()->GetDictionary(prefs::kManagedModeManualURLs);
  for (DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    bool allow = false;
    bool result = it.value().GetAsBoolean(&allow);
    DCHECK(result);
    entries->Append(
        GetEntryForPattern(it.key(), allow ? "allow" : "block"));
  }
}

}  // namespace

namespace options {

ManagedUserSettingsHandler::ManagedUserSettingsHandler()
    : has_seen_settings_dialog_(false) {
}

ManagedUserSettingsHandler::~ManagedUserSettingsHandler() {
}

void ManagedUserSettingsHandler::InitializePage() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableManagedUsers)) {
    return;
  }

  // Populate the list.
  UpdateViewFromModel();
}

void ManagedUserSettingsHandler::HandlePageOpened(const base::ListValue* args) {
  start_time_ = base::TimeTicks::Now();
  content::RecordAction(UserMetricsAction("ManagedMode_OpenSettings"));
  Profile* profile = Profile::FromWebUI(web_ui());
  ManagedUserService* service =
      ManagedUserServiceFactory::GetForProfile(profile);
  ManagedModeNavigationObserver* observer =
      ManagedModeNavigationObserver::FromWebContents(
          web_ui()->GetWebContents());

  // Check if we need to give initial elevation for startup of a new profile.
  if (service->startup_elevation()) {
    service->set_startup_elevation(false);
#if !defined(OS_CHROMEOS)
    observer->set_elevated(true);
#endif
  } else {
    has_seen_settings_dialog_ = true;
  }

  policy::ProfilePolicyConnector* connector =
      policy::ProfilePolicyConnectorFactory::GetForProfile(profile);
  policy::ManagedModePolicyProvider* policy_provider =
      connector->managed_mode_policy_provider();
  const DictionaryValue* settings = policy_provider->GetPolicies();
  web_ui()->CallJavascriptFunction("ManagedUserSettings.loadSettings",
                                   *settings);
  if (observer->is_elevated()) {
    web_ui()->CallJavascriptFunction("ManagedUserSettings.setAuthenticated",
                                     base::FundamentalValue(true));
  }
}

void ManagedUserSettingsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    // Unlock the settings page to allow editing.
    { "unlockSettings", IDS_UNLOCK_PASSPHRASE_BUTTON },
    { "lockSettings", IDS_LOCK_MANAGED_USER_BUTTON },
    // Manual exception view.
    { "allowException", IDS_EXCEPTIONS_ALLOW_BUTTON },
    { "blockException", IDS_EXCEPTIONS_BLOCK_BUTTON },
    { "addNewExceptionInstructions", IDS_EXCEPTIONS_ADD_NEW_INSTRUCTIONS },
    { "manageExceptions", IDS_EXCEPTIONS_MANAGE },
    { "exceptionPatternHeader", IDS_EXCEPTIONS_PATTERN_HEADER },
    { "exceptionBehaviorHeader", IDS_EXCEPTIONS_ACTION_HEADER },
    // Installed content packs.
    { "manualExceptionsHeader", IDS_MANUAL_EXCEPTION_HEADER },
    { "manualExceptionsTabTitle", IDS_MANUAL_EXCEPTION_TAB_TITLE },
    { "contentPacksTabLabel", IDS_CONTENT_PACKS_TAB_LABEL },
    { "getContentPacks", IDS_GET_CONTENT_PACKS_BUTTON },
    { "getContentPacksURL", IDS_GET_CONTENT_PACKS_URL },
    // Content pack restriction options.
    { "contentPackSettings", IDS_CONTENT_PACK_SETTINGS_LABEL },
    { "outsideContentPacksAllow", IDS_OUTSIDE_CONTENT_PACKS_ALLOW_RADIO },
    { "outsideContentPacksWarn", IDS_OUTSIDE_CONTENT_PACKS_WARN_RADIO },
    { "outsideContentPacksBlock", IDS_OUTSIDE_CONTENT_PACKS_BLOCK_RADIO },
    // Other managed user settings
    { "advancedManagedUserSettings", IDS_ADVANCED_MANAGED_USER_LABEL },
    { "enableSafeSearch", IDS_SAFE_SEARCH_ENABLED },
    { "setPassphrase", IDS_SET_PASSPHRASE_BUTTON },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "managedUserSettingsPage",
                IDS_MANAGED_USER_SETTINGS_TITLE);
  RegisterTitle(localized_strings, "manualExceptions",
                IDS_MANUAL_EXCEPTION_HEADER);
  localized_strings->SetBoolean(
      "managedUsersEnabled",
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableManagedUsers));
}

void ManagedUserSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "confirmManagedUserSettings",
      base::Bind(&ManagedUserSettingsHandler::SaveMetrics,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "settingsPageOpened",
      base::Bind(&ManagedUserSettingsHandler::HandlePageOpened,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeManualException",
      base::Bind(&ManagedUserSettingsHandler::RemoveManualException,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setManualException",
      base::Bind(&ManagedUserSettingsHandler::SetManualException,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "checkManualExceptionValidity",
      base::Bind(&ManagedUserSettingsHandler::CheckManualExceptionValidity,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setManagedUserSetting",
      base::Bind(&ManagedUserSettingsHandler::SetSetting,
                 base::Unretained(this)));
}

void ManagedUserSettingsHandler::SetSetting(
    const ListValue* args) {
  std::string key;
  if (!args->GetString(0, &key)) {
    NOTREACHED();
    return;
  }
  const Value* value = NULL;
  if (!args->Get(1, &value)) {
    NOTREACHED();
    return;
  }
  Profile* profile = Profile::FromWebUI(web_ui());
  policy::ProfilePolicyConnector* connector =
      policy::ProfilePolicyConnectorFactory::GetForProfile(profile);
  policy::ManagedModePolicyProvider* policy_provider =
      connector->managed_mode_policy_provider();
  policy_provider->SetPolicy(key, make_scoped_ptr(value->DeepCopy()));
}

void ManagedUserSettingsHandler::SaveMetrics(const ListValue* args) {
  if (!has_seen_settings_dialog_) {
    has_seen_settings_dialog_ = true;
    UMA_HISTOGRAM_LONG_TIMES("ManagedMode.UserSettingsFirstRunTime",
                             base::TimeTicks::Now() - start_time_);
  } else {
    UMA_HISTOGRAM_LONG_TIMES("ManagedMode.UserSettingsModifyTime",
                             base::TimeTicks::Now() - start_time_);
  }
}

void ManagedUserSettingsHandler::RemoveManualException(
    const ListValue* args) {
  // Check if the user is authenticated.
  if (!ManagedModeNavigationObserver::FromWebContents(
      web_ui()->GetWebContents())->is_elevated())
    return;

  size_t arg_i = 0;
  std::string pattern;
  CHECK(args->GetString(arg_i++, &pattern));

  UpdateManualBehavior(pattern, ManagedUserService::MANUAL_NONE);

  UpdateViewFromModel();
}

void ManagedUserSettingsHandler::UpdateManualBehavior(
    std::string pattern,
    ManagedUserService::ManualBehavior behavior) {
  ManagedUserService* service =
      ManagedUserServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  GURL url(pattern);
  if (!url.is_valid()) {
    // This is a host, get the canonical version of the string.
    std::vector<std::string> to_update;
    std::string canonical_host;
    bool result = CanonicalizeHost(pattern, &canonical_host);
    DCHECK(result);
    to_update.push_back(canonical_host);
    service->SetManualBehaviorForHosts(to_update, behavior);
  } else {
    // A specific URL. The GURL generates the canonical form when being built.
    std::vector<GURL> to_update;
    to_update.push_back(url);
    service->SetManualBehaviorForURLs(to_update, behavior);
  }
}

void ManagedUserSettingsHandler::SetManualException(
    const ListValue* args) {
  // Check if the user is authenticated.
  if (!ManagedModeNavigationObserver::FromWebContents(
      web_ui()->GetWebContents())->is_elevated())
    return;

  size_t arg_i = 0;
  std::string pattern;
  CHECK(args->GetString(arg_i++, &pattern));
  std::string setting;
  CHECK(args->GetString(arg_i++, &setting));
  bool needs_update;
  CHECK(args->GetBoolean(arg_i++, &needs_update));

  DCHECK(setting == "allow" || setting == "block");
  ManagedUserService::ManualBehavior behavior =
      (setting == "allow") ? ManagedUserService::MANUAL_ALLOW
                           : ManagedUserService::MANUAL_BLOCK;
  UpdateManualBehavior(pattern, behavior);

  if (needs_update)
    UpdateViewFromModel();
}

void ManagedUserSettingsHandler::CheckManualExceptionValidity(
    const ListValue* args) {
  size_t arg_i = 0;
  std::string pattern_string;
  CHECK(args->GetString(arg_i++, &pattern_string));

  // First, try to see if it is a valid URL.
  GURL url(pattern_string);

  // If the pattern is a valid URL then we're done, otherwise try to get the
  // canonical host and see if that is valid.
  bool is_valid = (url.is_valid() && url.IsStandard()) ||
                  CanonicalizeHost(pattern_string, NULL);

  web_ui()->CallJavascriptFunction(
      "ManagedUserSettings.patternValidityCheckComplete",
      base::StringValue(pattern_string),
      base::FundamentalValue(is_valid));
}

void ManagedUserSettingsHandler::UpdateViewFromModel() {
  ListValue entries;
  AddCurrentURLEntries(web_ui(), &entries);

  web_ui()->CallJavascriptFunction(
      "ManagedUserSettings.setManualExceptions",
      entries);
}

}  // namespace options
