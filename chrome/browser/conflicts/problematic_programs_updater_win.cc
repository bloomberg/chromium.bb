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
#include "base/win/registry.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/browser/conflicts/third_party_metrics_recorder_win.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Converts a json dictionary of programs to a vector of ProgramInfos.
std::vector<InstalledPrograms::ProgramInfo> ConvertToProgramInfoVector(
    const base::Value& programs) {
  std::vector<InstalledPrograms::ProgramInfo> result;

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

    // If any is missing, skip this element.
    if (!registry_is_hkcu_value || !registry_key_path_value ||
        !registry_wow64_access_value) {
      continue;
    }

    result.emplace_back(base::UTF8ToUTF16(name),
                        registry_is_hkcu_value->GetBool() ? HKEY_CURRENT_USER
                                                          : HKEY_LOCAL_MACHINE,
                        base::UTF8ToUTF16(registry_key_path_value->GetString()),
                        registry_wow64_access_value->GetInt());
  }

  return result;
}

// Serializes a vector of ProgramInfos to a json dictionary.
base::Value ConvertToDictionary(
    const std::vector<InstalledPrograms::ProgramInfo>& programs) {
  base::Value result(base::Value::Type::DICTIONARY);

  for (const InstalledPrograms::ProgramInfo& program : programs) {
    base::Value element(base::Value::Type::DICTIONARY);

    element.SetKey("registry_is_hkcu",
                   base::Value(program.registry_root == HKEY_CURRENT_USER));
    element.SetKey("registry_key_path", base::Value(program.registry_key_path));
    element.SetKey(
        "registry_wow64_access",
        base::Value(static_cast<int>(program.registry_wow64_access)));

    result.SetKey(base::UTF16ToUTF8(program.name), std::move(element));
  }

  return result;
}

}  // namespace

const base::Feature kThirdPartyConflictsWarning{
    "ThirdPartyConflictsWarning", base::FEATURE_ENABLED_BY_DEFAULT};

ProblematicProgramsUpdater::~ProblematicProgramsUpdater() = default;

// static
void ProblematicProgramsUpdater::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kProblematicPrograms);
}

// static
std::unique_ptr<ProblematicProgramsUpdater>
ProblematicProgramsUpdater::MaybeCreate(
    const InstalledPrograms& installed_programs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<ProblematicProgramsUpdater> instance;

  if (base::FeatureList::IsEnabled(kThirdPartyConflictsWarning))
    instance.reset(new ProblematicProgramsUpdater(installed_programs));

  return instance;
}

// static
bool ProblematicProgramsUpdater::TrimCache() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(kThirdPartyConflictsWarning))
    return false;

  std::vector<InstalledPrograms::ProgramInfo> programs =
      ConvertToProgramInfoVector(
          *g_browser_process->local_state()
               ->FindPreference(prefs::kProblematicPrograms)
               ->GetValue());

  // Remove entries that can no longer be found in the registry.
  programs.erase(
      std::remove_if(programs.begin(), programs.end(),
                     [](const InstalledPrograms::ProgramInfo& element) {
                       base::win::RegKey registry_key(
                           element.registry_root,
                           element.registry_key_path.c_str(),
                           KEY_QUERY_VALUE | element.registry_wow64_access);
                       return !registry_key.Valid();
                     }),
      programs.end());

  // Write it back.
  if (programs.empty()) {
    g_browser_process->local_state()->ClearPref(prefs::kProblematicPrograms);
  } else {
    g_browser_process->local_state()->Set(prefs::kProblematicPrograms,
                                          ConvertToDictionary(programs));
  }
  return !programs.empty();
}

// static
bool ProblematicProgramsUpdater::HasCachedPrograms() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(kThirdPartyConflictsWarning))
    return false;

  return !g_browser_process->local_state()
              ->GetDictionary(prefs::kProblematicPrograms)
              ->empty();
}

// static
base::Value ProblematicProgramsUpdater::GetCachedProgramNames() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Value program_names(base::Value::Type::LIST);

  if (base::FeatureList::IsEnabled(kThirdPartyConflictsWarning)) {
    std::vector<InstalledPrograms::ProgramInfo> programs =
        ConvertToProgramInfoVector(
            *g_browser_process->local_state()
                 ->FindPreference(prefs::kProblematicPrograms)
                 ->GetValue());

    for (const auto& program : programs)
      // Only the name is returned.
      program_names.GetList().push_back(base::Value(program.name));
  }

  return program_names;
}

void ProblematicProgramsUpdater::OnNewModuleFound(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Only consider loaded modules.
  if (module_data.module_types & ModuleInfoData::kTypeLoadedModule) {
    installed_programs_.GetInstalledPrograms(module_key.module_path,
                                             &programs_);
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
  if (programs_.empty())
    return;

  // Converting the accumulated programs to a json dictionary takes care of
  // eliminating duplicates.
  base::Value new_programs = ConvertToDictionary(programs_);
  programs_.clear();

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
    const InstalledPrograms& installed_programs)
    : installed_programs_(installed_programs) {}
