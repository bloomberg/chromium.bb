// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/conflicts/conflicts_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "base/win/windows_version.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/browser/conflicts/module_info_win.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/conflicts/incompatible_applications_updater_win.h"
#include "chrome/browser/conflicts/module_blacklist_cache_updater_win.h"
#endif

namespace {

#if defined(GOOGLE_CHROME_BUILD)

// Strings used twice.
constexpr char kNotLoaded[] = "Not loaded";
constexpr char kAllowedInputMethodEditor[] = "Allowed - Input method editor";
constexpr char kAllowedMatchingCertificate[] = "Allowed - Matching certificate";
constexpr char kAllowedMicrosoftModule[] = "Allowed - Microsoft module";
constexpr char kAllowedWhitelisted[] = "Allowed - Whitelisted";
constexpr char kAllowedSameDirectory[] =
#if defined(OFFICIAL_BUILD)
    // In official builds, modules in the Chrome directory are blocked but they
    // won't cause a warning because the warning would blame Chrome itself.
    "Tolerated - In executable directory";
#else  // !defined(OFFICIAL_BUILD)
    // In developer builds, DLLs that are part of Chrome are not signed and thus
    // the easy way to identify them is to check that they are in the same
    // directory (or child folder) as the main exe.
    "Allowed - In executable directory (dev builds only)";
#endif

void AppendString(base::StringPiece input, std::string* output) {
  if (!output->empty())
    *output += ", ";
  input.AppendToString(output);
}

// Returns a string describing the current module blocking status: loaded or
// not, blocked or not, was in blacklist cache or not, bypassed blocking or not.
std::string GetBlockingStatusString(
    const ModuleBlacklistCacheUpdater::ModuleBlockingState& blocking_state) {
  std::string status;

  // Output status regarding the blacklist cache, current blocking, and
  // load status.
  if (blocking_state.was_blocked)
    status = "Blocked";
  if (!blocking_state.was_loaded)
    AppendString(kNotLoaded, &status);
  else if (blocking_state.was_in_blacklist_cache)
    AppendString("Bypassed blocking", &status);
  if (blocking_state.was_in_blacklist_cache)
    AppendString("In blacklist cache", &status);

  return status;
}

// Returns a string describing the blocking decision related to a module. This
// returns the empty string to indicate that the warning decision description
// should be used instead.
std::string GetBlockingDecisionString(
    const ModuleBlacklistCacheUpdater::ModuleBlockingState& blocking_state,
    IncompatibleApplicationsUpdater* incompatible_applications_updater) {
  using BlockingDecision = ModuleBlacklistCacheUpdater::ModuleBlockingDecision;

  // Append status regarding the logic that will be applied during the next
  // startup.
  switch (blocking_state.blocking_decision) {
    case BlockingDecision::kUnknown:
      NOTREACHED();
      break;
    case BlockingDecision::kAllowedIME:
      return kAllowedInputMethodEditor;
    case BlockingDecision::kAllowedSameCertificate:
      return kAllowedMatchingCertificate;
    case BlockingDecision::kAllowedSameDirectory:
      return kAllowedSameDirectory;
    case BlockingDecision::kAllowedMicrosoft:
      return kAllowedMicrosoftModule;
    case BlockingDecision::kAllowedWhitelisted:
      return kAllowedWhitelisted;
    case BlockingDecision::kTolerated:
      // This is a module explicitly allowed to load by the Module List
      // component. But it is still valid for a potential warning, and so the
      // warning status is used instead.
      if (incompatible_applications_updater)
        break;
      return "Tolerated - Will be blocked in the future";
    case BlockingDecision::kDisallowedExplicit:
      return "Disallowed - Explicitly blacklisted";
    case BlockingDecision::kDisallowedImplicit:
      return "Disallowed - Implicitly blacklisted";
  }

  // Returning an empty string indicates that the warning status should be used.
  return std::string();
}

// Returns a string describing the warning decision that was made regarding a
// module.
std::string GetModuleWarningDecisionString(
    const ModuleInfoKey& module_key,
    IncompatibleApplicationsUpdater* incompatible_applications_updater) {
  using WarningDecision =
      IncompatibleApplicationsUpdater::ModuleWarningDecision;

  WarningDecision warning_decision =
      incompatible_applications_updater->GetModuleWarningDecision(module_key);

  switch (warning_decision) {
    case WarningDecision::kNotLoaded:
      return kNotLoaded;
    case WarningDecision::kAllowedIME:
      return kAllowedInputMethodEditor;
    case WarningDecision::kAllowedShellExtension:
      return "Tolerated - Shell extension";
    case WarningDecision::kAllowedSameCertificate:
      return kAllowedMatchingCertificate;
    case WarningDecision::kAllowedSameDirectory:
      return kAllowedSameDirectory;
    case WarningDecision::kAllowedMicrosoft:
      return kAllowedMicrosoftModule;
    case WarningDecision::kAllowedWhitelisted:
      return kAllowedWhitelisted;
    case WarningDecision::kNoTiedApplication:
      return "Tolerated - Could not tie to an installed application";
    case WarningDecision::kIncompatible:
      return "Incompatible";
    case WarningDecision::kAddedToBlacklist:
    case WarningDecision::kUnknown:
      NOTREACHED();
      break;
  }

  return std::string();
}

std::string GetModuleStatusString(
    const ModuleInfoKey& module_key,
    IncompatibleApplicationsUpdater* incompatible_applications_updater,
    ModuleBlacklistCacheUpdater* module_blacklist_cache_updater) {
  if (!incompatible_applications_updater && !module_blacklist_cache_updater)
    return std::string();

  std::string status;

  // The blocking status is shown over the warning status.
  if (module_blacklist_cache_updater) {
    const ModuleBlacklistCacheUpdater::ModuleBlockingState& blocking_state =
        module_blacklist_cache_updater->GetModuleBlockingState(module_key);

    status = GetBlockingStatusString(blocking_state);

    std::string blocking_string = GetBlockingDecisionString(
        blocking_state, incompatible_applications_updater);
    if (!blocking_string.empty()) {
      AppendString(blocking_string, &status);
      return status;
    }

    // An empty |blocking_string| indicates that a warning decision string
    // should be used instead.
  }

  if (incompatible_applications_updater) {
    AppendString(GetModuleWarningDecisionString(
                     module_key, incompatible_applications_updater),
                 &status);
  }

  return status;
}
#endif  // defined(GOOGLE_CHROME_BUILD)

}  // namespace

ConflictsHandler::ConflictsHandler() : weak_ptr_factory_(this) {}

ConflictsHandler::~ConflictsHandler() {
  if (module_list_)
    ModuleDatabase::GetInstance()->RemoveObserver(this);
}

void ConflictsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestModuleList",
      base::BindRepeating(&ConflictsHandler::HandleRequestModuleList,
                          base::Unretained(this)));
}

void ConflictsHandler::OnNewModuleFound(const ModuleInfoKey& module_key,
                                        const ModuleInfoData& module_data) {
  DCHECK(module_list_);

  auto data = std::make_unique<base::DictionaryValue>();

  data->SetString("third_party_module_status", std::string());
#if defined(GOOGLE_CHROME_BUILD)
  if (ModuleDatabase::GetInstance()->third_party_conflicts_manager()) {
    auto* incompatible_applications_updater =
        ModuleDatabase::GetInstance()
            ->third_party_conflicts_manager()
            ->incompatible_applications_updater();
    auto* module_blacklist_cache_updater =
        ModuleDatabase::GetInstance()
            ->third_party_conflicts_manager()
            ->module_blacklist_cache_updater();

    data->SetString(
        "third_party_module_status",
        GetModuleStatusString(module_key, incompatible_applications_updater,
                              module_blacklist_cache_updater));
  }
#endif  // defined(GOOGLE_CHROME_BUILD)

  std::string type_string;
  if (module_data.module_properties & ModuleInfoData::kPropertyShellExtension)
    type_string = "Shell extension";
  data->SetString("type_description", type_string);

  const auto& inspection_result = *module_data.inspection_result;
  data->SetString("location", inspection_result.location);
  data->SetString("name", inspection_result.basename);
  data->SetString("product_name", inspection_result.product_name);
  data->SetString("description", inspection_result.description);
  data->SetString("version", inspection_result.version);
  data->SetString("digital_signer", inspection_result.certificate_info.subject);
  data->SetString("code_id", GenerateCodeId(module_key));

  module_list_->Append(std::move(data));
}

void ConflictsHandler::OnModuleDatabaseIdle() {
  DCHECK(module_list_);
  DCHECK(!module_list_callback_id_.empty());

  ModuleDatabase::GetInstance()->RemoveObserver(this);

  base::DictionaryValue results;
  results.SetInteger("moduleCount", module_list_->GetSize());
  results.Set("moduleList", std::move(module_list_));

  // Third-party conflicts status.
  ThirdPartyFeaturesStatus third_party_features_status =
      third_party_features_status_.value();
  results.SetBoolean("thirdPartyFeatureEnabled",
                     IsThirdPartyFeatureEnabled(third_party_features_status));
  results.SetString(
      "thirdPartyFeatureStatus",
      GetThirdPartyFeaturesStatusString(third_party_features_status));

  AllowJavascript();
  ResolveJavascriptCallback(base::Value(module_list_callback_id_), results);
}

void ConflictsHandler::HandleRequestModuleList(const base::ListValue* args) {
  // Make sure the JS doesn't call 'requestModuleList' more than once.
  // TODO(739291): It would be better to kill the renderer instead of the
  // browser for malformed messages.
  CHECK(!module_list_);

  CHECK_EQ(1U, args->GetSize());
  CHECK(args->GetString(0, &module_list_callback_id_));

#if defined(GOOGLE_CHROME_BUILD)
  // If the ThirdPartyConflictsManager instance exists, wait until it is fully
  // initialized before retrieving the list of modules.
  auto* third_party_conflicts_manager =
      ModuleDatabase::GetInstance()->third_party_conflicts_manager();
  if (third_party_conflicts_manager) {
    third_party_conflicts_manager->ForceInitialization(
        base::BindRepeating(&ConflictsHandler::OnManagerInitializationComplete,
                            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Figure out why the manager instance doesn't exist.
  if (!ModuleDatabase::IsThirdPartyBlockingPolicyEnabled())
    third_party_features_status_ = kPolicyDisabled;

  if (!base::FeatureList::IsEnabled(features::kThirdPartyModulesBlocking) &&
      !IncompatibleApplicationsUpdater::IsWarningEnabled()) {
    third_party_features_status_ = kFeatureDisabled;
  }

  // The above 2 cases are the only possible reasons why the manager wouldn't
  // exist.
  DCHECK(third_party_features_status_.has_value());
#else  // defined(GOOGLE_CHROME_BUILD)
  third_party_features_status_ = kNonGoogleChromeBuild;
#endif

  GetListOfModules();
}

#if defined(GOOGLE_CHROME_BUILD)
void ConflictsHandler::OnManagerInitializationComplete(
    ThirdPartyConflictsManager::State state) {
  switch (state) {
    case ThirdPartyConflictsManager::State::kModuleListInvalidFailure:
      third_party_features_status_ = kModuleListInvalid;
      break;
    case ThirdPartyConflictsManager::State::kNoModuleListAvailableFailure:
      third_party_features_status_ = kNoModuleListAvailable;
      break;
    case ThirdPartyConflictsManager::State::kWarningInitialized:
      third_party_features_status_ = kWarningInitialized;
      break;
    case ThirdPartyConflictsManager::State::kBlockingInitialized:
      third_party_features_status_ = kBlockingInitialized;
      break;
    case ThirdPartyConflictsManager::State::kWarningAndBlockingInitialized:
      third_party_features_status_ = kWarningAndBlockingInitialized;
      break;
    case ThirdPartyConflictsManager::State::kDestroyed:
      // Turning off the feature via group policy is the only way to have the
      // manager destroyed.
      third_party_features_status_ = kPolicyDisabled;
      break;
  }

  GetListOfModules();
}
#endif  // defined(GOOGLE_CHROME_BUILD)

void ConflictsHandler::GetListOfModules() {
  // The request is handled asynchronously, filling up the |module_list_|,
  // and will callback via OnModuleDatabaseIdle() on completion.
  module_list_ = std::make_unique<base::ListValue>();

  auto* module_database = ModuleDatabase::GetInstance();
  module_database->IncreaseInspectionPriority();
  module_database->AddObserver(this);
}

// static
bool ConflictsHandler::IsThirdPartyFeatureEnabled(
    ThirdPartyFeaturesStatus status) {
  return status == kWarningInitialized || status == kBlockingInitialized ||
         status == kWarningAndBlockingInitialized;
}

// static
std::string ConflictsHandler::GetThirdPartyFeaturesStatusString(
    ThirdPartyFeaturesStatus status) {
  switch (status) {
    case ThirdPartyFeaturesStatus::kNonGoogleChromeBuild:
      return "The third-party features are not available in non-Google Chrome "
             "builds.";
    case ThirdPartyFeaturesStatus::kPolicyDisabled:
      return "The ThirdPartyBlockingEnabled group policy is disabled.";
    case ThirdPartyFeaturesStatus::kFeatureDisabled:
      if (base::win::GetVersion() < base::win::VERSION_WIN10)
        return "The ThirdPartyModulesBlocking feature is disabled.";

      return "Both the IncompatibleApplicationsWarning and "
             "ThirdPartyModulesBlocking features are disabled.";
    case ThirdPartyFeaturesStatus::kModuleListInvalid:
      return "Disabled - The Module List component version is invalid.";
    case ThirdPartyFeaturesStatus::kNoModuleListAvailable:
      return "Disabled - There is no Module List version available.";
    case ThirdPartyFeaturesStatus::kWarningInitialized:
      DCHECK_GE(base::win::GetVersion(), base::win::VERSION_WIN10);
      return "The IncompatibleApplicationsWarning feature is enabled, while "
             "the ThirdPartyModulesBlocking feature is disabled.";
    case ThirdPartyFeaturesStatus::kBlockingInitialized:
      if (base::win::GetVersion() < base::win::VERSION_WIN10)
        return "The ThirdPartyModulesBlocking feature is enabled.";

      return "The ThirdPartyModulesBlocking feature is enabled, while the "
             "IncompatibleApplicationsWarning feature is disabled.";
    case ThirdPartyFeaturesStatus::kWarningAndBlockingInitialized:
      DCHECK_GE(base::win::GetVersion(), base::win::VERSION_WIN10);
      return "Both the IncompatibleApplicationsWarning and "
             "ThirdPartyModulesBlocking features are enabled";
  }
}
