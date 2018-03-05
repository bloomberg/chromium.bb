// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/problematic_programs_updater_win.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/browser/conflicts/module_list_filter_win.h"
#include "chrome/browser/conflicts/third_party_metrics_recorder_win.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Helper function to serialize a vector of ProblematicPrograms to JSON.
base::Value ConvertToDictionary(
    const std::vector<ProblematicProgramsUpdater::ProblematicProgram>&
        programs) {
  base::Value result(base::Value::Type::DICTIONARY);

  for (const auto& program : programs) {
    base::Value element(base::Value::Type::DICTIONARY);

    // The registry location is necessary to quickly figure out if that program
    // is still installed on the computer.
    element.SetKey("registry_is_hkcu", base::Value(program.info.registry_root ==
                                                   HKEY_CURRENT_USER));
    element.SetKey("registry_key_path",
                   base::Value(program.info.registry_key_path));
    element.SetKey(
        "registry_wow64_access",
        base::Value(static_cast<int>(program.info.registry_wow64_access)));

    // And then the actual information needed to display a warning to the user.
    element.SetKey("allow_load",
                   base::Value(program.blacklist_action->allow_load()));
    element.SetKey("type",
                   base::Value(program.blacklist_action->message_type()));
    element.SetKey("message_url",
                   base::Value(program.blacklist_action->message_url()));

    result.SetKey(base::UTF16ToUTF8(program.info.name), std::move(element));
  }

  return result;
}

// Helper function to deserialize a vector of ProblematicPrograms.
std::vector<ProblematicProgramsUpdater::ProblematicProgram>
ConvertToProblematicProgramsVector(const base::Value& programs) {
  std::vector<ProblematicProgramsUpdater::ProblematicProgram> result;

  for (const auto& element : programs.DictItems()) {
    const std::string& name = element.first;
    const base::Value& value = element.second;

    if (!value.is_dict())
      continue;

    const base::Value* registry_is_hkcu_value =
        value.FindKeyOfType("registry_is_hkcu", base::Value::Type::BOOLEAN);
    const base::Value* registry_key_path_value =
        value.FindKeyOfType("registry_key_path", base::Value::Type::STRING);
    const base::Value* registry_wow64_access_value = value.FindKeyOfType(
        "registry_wow64_access", base::Value::Type::INTEGER);
    const base::Value* allow_load_value =
        value.FindKeyOfType("allow_load", base::Value::Type::BOOLEAN);
    const base::Value* type_value =
        value.FindKeyOfType("type", base::Value::Type::INTEGER);
    const base::Value* message_url_value =
        value.FindKeyOfType("message_url", base::Value::Type::STRING);

    // All of the above are required. If any is missing, the element is skipped.
    if (!registry_is_hkcu_value || !registry_key_path_value ||
        !registry_wow64_access_value || !allow_load_value || !type_value ||
        !message_url_value) {
      continue;
    }

    InstalledPrograms::ProgramInfo program_info = {
        base::UTF8ToUTF16(name),
        registry_is_hkcu_value->GetBool() ? HKEY_CURRENT_USER
                                          : HKEY_LOCAL_MACHINE,
        base::UTF8ToUTF16(registry_key_path_value->GetString()),
        static_cast<REGSAM>(registry_wow64_access_value->GetInt())};

    auto blacklist_action =
        std::make_unique<chrome::conflicts::BlacklistAction>();
    blacklist_action->set_allow_load(allow_load_value->GetBool());
    blacklist_action->set_message_type(
        static_cast<chrome::conflicts::BlacklistMessageType>(
            type_value->GetInt()));
    blacklist_action->set_message_url(message_url_value->GetString());

    result.emplace_back(std::move(program_info), std::move(blacklist_action));
  }

  return result;
}

// Removes stale programs from the cache. This can happen if a program was
// uninstalled between the time it was found and Chrome was relaunched.
// Returns true if any problematic programs are installed after trimming
// completes (i.e., HasCachedPrograms() would return true).
void RemoveStaleEntriesAndUpdateCache(
    std::vector<ProblematicProgramsUpdater::ProblematicProgram>* programs) {
  // Remove entries that can no longer be found in the registry.
  programs->erase(std::remove_if(programs->begin(), programs->end(),
                                 [](const auto& program) {
                                   base::win::RegKey registry_key(
                                       program.info.registry_root,
                                       program.info.registry_key_path.c_str(),
                                       KEY_QUERY_VALUE |
                                           program.info.registry_wow64_access);
                                   return !registry_key.Valid();
                                 }),
                  programs->end());

  // Write it back.
  if (programs->empty()) {
    g_browser_process->local_state()->ClearPref(prefs::kProblematicPrograms);
  } else {
    g_browser_process->local_state()->Set(prefs::kProblematicPrograms,
                                          ConvertToDictionary(*programs));
  }
}

}  // namespace

const base::Feature kIncompatibleApplicationsWarning{
    "IncompatibleApplicationsWarning", base::FEATURE_DISABLED_BY_DEFAULT};

// ProblematicProgram ----------------------------------------------------------

ProblematicProgramsUpdater::ProblematicProgram::ProblematicProgram(
    InstalledPrograms::ProgramInfo info,
    std::unique_ptr<chrome::conflicts::BlacklistAction> blacklist_action)
    : info(std::move(info)), blacklist_action(std::move(blacklist_action)) {}

ProblematicProgramsUpdater::ProblematicProgram::~ProblematicProgram() = default;

ProblematicProgramsUpdater::ProblematicProgram::ProblematicProgram(
    ProblematicProgram&& problematic_program) = default;

ProblematicProgramsUpdater::ProblematicProgram&
ProblematicProgramsUpdater::ProblematicProgram::operator=(
    ProblematicProgram&& problematic_program) = default;

// ProblematicProgramsUpdater --------------------------------------------------

ProblematicProgramsUpdater::~ProblematicProgramsUpdater() = default;

// static
void ProblematicProgramsUpdater::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kProblematicPrograms);
}

// static
std::unique_ptr<ProblematicProgramsUpdater>
ProblematicProgramsUpdater::MaybeCreate(
    const ModuleListFilter& module_list_filter,
    const InstalledPrograms& installed_programs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<ProblematicProgramsUpdater> instance;

  if (base::FeatureList::IsEnabled(kIncompatibleApplicationsWarning)) {
    instance.reset(
        new ProblematicProgramsUpdater(module_list_filter, installed_programs));
  }

  return instance;
}

// static
bool ProblematicProgramsUpdater::HasCachedPrograms() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(kIncompatibleApplicationsWarning))
    return false;

  std::vector<ProblematicProgram> programs = ConvertToProblematicProgramsVector(
      *g_browser_process->local_state()
           ->FindPreference(prefs::kProblematicPrograms)
           ->GetValue());

  RemoveStaleEntriesAndUpdateCache(&programs);

  return !programs.empty();
}

// static
std::vector<ProblematicProgramsUpdater::ProblematicProgram>
ProblematicProgramsUpdater::GetCachedPrograms() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<ProblematicProgram> programs;

  if (!base::FeatureList::IsEnabled(kIncompatibleApplicationsWarning))
    return programs;

  programs = ConvertToProblematicProgramsVector(
      *g_browser_process->local_state()
           ->FindPreference(prefs::kProblematicPrograms)
           ->GetValue());

  RemoveStaleEntriesAndUpdateCache(&programs);

  return programs;
}

void ProblematicProgramsUpdater::OnNewModuleFound(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Only consider loaded modules.
  // TODO(pmonette): Also consider blocked modules when that becomes possible.
  if ((module_data.module_types & ModuleInfoData::kTypeLoadedModule) == 0)
    return;

  // Skip explicitely whitelisted modules.
  if (module_list_filter_.IsWhitelisted(module_key, module_data))
    return;

  // Also skip a module if it cannot be associated with an installed program
  // on the user's computer.
  std::vector<InstalledPrograms::ProgramInfo> associated_programs;
  if (!installed_programs_.GetInstalledPrograms(module_key.module_path,
                                                &associated_programs)) {
    return;
  }

  std::unique_ptr<chrome::conflicts::BlacklistAction> blacklist_action =
      module_list_filter_.IsBlacklisted(module_key, module_data);
  if (blacklist_action) {
    for (auto&& associated_program : associated_programs) {
      problematic_programs_.emplace_back(std::move(associated_program),
                                         std::move(blacklist_action));
    }
    return;
  }

  // The default behavior is to suggest to uninstall.
  blacklist_action = std::make_unique<chrome::conflicts::BlacklistAction>();
  blacklist_action->set_allow_load(true);
  blacklist_action->set_message_type(
      chrome::conflicts::BlacklistMessageType::UNINSTALL);
  blacklist_action->set_message_url(std::string());

  for (auto&& associated_program : associated_programs) {
    problematic_programs_.emplace_back(std::move(associated_program),
                                       std::move(blacklist_action));
  }
}

void ProblematicProgramsUpdater::OnModuleDatabaseIdle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // On the first call to OnModuleDatabaseIdle(), the previous value must always
  // be overwritten.
  if (before_first_idle_)
    g_browser_process->local_state()->ClearPref(prefs::kProblematicPrograms);
  before_first_idle_ = false;

  // If there is no new problematic programs, there is nothing to do.
  if (problematic_programs_.empty())
    return;

  // The conversion of the accumulated programs to a json dictionary takes care
  // of eliminating duplicates.
  base::Value new_programs = ConvertToDictionary(problematic_programs_);
  problematic_programs_.clear();

  // Update the existing dictionary.
  DictionaryPrefUpdate update(g_browser_process->local_state(),
                              prefs::kProblematicPrograms);
  base::Value* existing_programs = update.Get();
  for (auto&& element : new_programs.DictItems()) {
    existing_programs->SetKey(std::move(element.first),
                              std::move(element.second));
  }
}

ProblematicProgramsUpdater::ProblematicProgramsUpdater(
    const ModuleListFilter& module_list_filter,
    const InstalledPrograms& installed_programs)
    : module_list_filter_(module_list_filter),
      installed_programs_(installed_programs) {}
