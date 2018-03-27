// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/problematic_programs_updater_win.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/browser/conflicts/module_info_util_win.h"
#include "chrome/browser/conflicts/module_list_filter_win.h"
#include "chrome/browser/conflicts/third_party_metrics_recorder_win.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Serializes a vector of ProblematicPrograms to JSON.
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

// Deserializes a ProblematicProgram named |name| from |value|. Returns null if
// |value| is not a dict containing all required fields.
std::unique_ptr<ProblematicProgramsUpdater::ProblematicProgram>
ConvertToProblematicProgram(const std::string& name, const base::Value& value) {
  if (!value.is_dict())
    return nullptr;

  const base::Value* registry_is_hkcu_value =
      value.FindKeyOfType("registry_is_hkcu", base::Value::Type::BOOLEAN);
  const base::Value* registry_key_path_value =
      value.FindKeyOfType("registry_key_path", base::Value::Type::STRING);
  const base::Value* registry_wow64_access_value =
      value.FindKeyOfType("registry_wow64_access", base::Value::Type::INTEGER);
  const base::Value* allow_load_value =
      value.FindKeyOfType("allow_load", base::Value::Type::BOOLEAN);
  const base::Value* type_value =
      value.FindKeyOfType("type", base::Value::Type::INTEGER);
  const base::Value* message_url_value =
      value.FindKeyOfType("message_url", base::Value::Type::STRING);

  // All of the above are required for a valid program.
  if (!registry_is_hkcu_value || !registry_key_path_value ||
      !registry_wow64_access_value || !allow_load_value || !type_value ||
      !message_url_value) {
    return nullptr;
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

  return std::make_unique<ProblematicProgramsUpdater::ProblematicProgram>(
      std::move(program_info), std::move(blacklist_action));
}

// Returns true if |program| references an existing program in the registry.
//
// Used to filter out stale programs from the cache. This can happen if a
// program was uninstalled between the time it was found and Chrome was
// relaunched.
bool IsValidProgram(
    const ProblematicProgramsUpdater::ProblematicProgram& program) {
  return base::win::RegKey(program.info.registry_root,
                           program.info.registry_key_path.c_str(),
                           KEY_QUERY_VALUE | program.info.registry_wow64_access)
      .Valid();
}

// Clears the cache of all the programs whose name is in |state_program_names|.
void RemoveStalePrograms(const std::vector<std::string>& stale_program_names) {
  // Early exit because DictionaryPrefUpdate will write to the pref even if it
  // doesn't contain a value.
  if (stale_program_names.empty())
    return;

  DictionaryPrefUpdate update(g_browser_process->local_state(),
                              prefs::kProblematicPrograms);
  base::Value* existing_programs = update.Get();

  for (const auto& program_name : stale_program_names) {
    bool removed = existing_programs->RemoveKey(program_name);
    DCHECK(removed);
  }
}

// Applies the given |function| object to each valid ProblematicProgram found
// in the kProblematicPrograms preference.
//
// The signature of the function must be equivalent to the following:
//   bool Function(std::unique_ptr<ProblematicProgram> program));
//
// The return value of |function| indicates if the enumeration should continue
// (true) or be stopped (false).
//
// This function takes care of removing invalid entries that are found during
// the enumeration.
template <class UnaryFunction>
void EnumerateAndTrimProblematicPrograms(UnaryFunction function) {
  std::vector<std::string> stale_program_names;
  for (const auto& item : g_browser_process->local_state()
                              ->FindPreference(prefs::kProblematicPrograms)
                              ->GetValue()
                              ->DictItems()) {
    auto program = ConvertToProblematicProgram(item.first, item.second);

    if (!program || !IsValidProgram(*program)) {
      // Mark every invalid program as stale so they are removed from the cache.
      stale_program_names.push_back(item.first);
      continue;
    }

    // Notify the caller and stop the enumeration if requested by the function.
    if (!function(std::move(program)))
      break;
  }

  RemoveStalePrograms(stale_program_names);
}

}  // namespace

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

ProblematicProgramsUpdater::ProblematicProgramsUpdater(
    const CertificateInfo& exe_certificate_info,
    const ModuleListFilter& module_list_filter,
    const InstalledPrograms& installed_programs)
    : exe_certificate_info_(exe_certificate_info),
      module_list_filter_(module_list_filter),
      installed_programs_(installed_programs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ProblematicProgramsUpdater::~ProblematicProgramsUpdater() = default;

// static
void ProblematicProgramsUpdater::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kProblematicPrograms);
}

// static
bool ProblematicProgramsUpdater::IsIncompatibleApplicationsWarningEnabled() {
  return ModuleDatabase::GetInstance() &&
         ModuleDatabase::GetInstance()->third_party_conflicts_manager();
}

// static
bool ProblematicProgramsUpdater::HasCachedPrograms() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool found_valid_program = false;

  EnumerateAndTrimProblematicPrograms(
      [&found_valid_program](std::unique_ptr<ProblematicProgram> program) {
        found_valid_program = true;

        // Break the enumeration.
        return false;
      });

  return found_valid_program;
}

// static
std::vector<ProblematicProgramsUpdater::ProblematicProgram>
ProblematicProgramsUpdater::GetCachedPrograms() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<ProblematicProgram> valid_programs;

  EnumerateAndTrimProblematicPrograms(
      [&valid_programs](std::unique_ptr<ProblematicProgram> program) {
        valid_programs.push_back(std::move(*program));

        // Continue the enumeration.
        return true;
      });

  return valid_programs;
}

void ProblematicProgramsUpdater::OnNewModuleFound(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Only consider loaded modules.
  // TODO(pmonette): Also consider blocked modules when that becomes possible.
  if ((module_data.module_types & ModuleInfoData::kTypeLoadedModule) == 0)
    return;

  // Explicitly whitelist modules whose signing cert's Subject field matches the
  // one in the current executable. No attempt is made to check the validity of
  // module signatures or of signing certs.
  if (exe_certificate_info_.type != CertificateType::NO_CERTIFICATE &&
      exe_certificate_info_.subject ==
          module_data.inspection_result->certificate_info.subject) {
    return;
  }

  // Skip modules whitelisted by the Module List component.
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
  if (!blacklist_action) {
    // The default behavior is to suggest to uninstall.
    blacklist_action = std::make_unique<chrome::conflicts::BlacklistAction>();
    blacklist_action->set_allow_load(true);
    blacklist_action->set_message_type(
        chrome::conflicts::BlacklistMessageType::UNINSTALL);
    blacklist_action->set_message_url(std::string());
  }

  for (auto&& associated_program : associated_programs) {
    problematic_programs_.emplace_back(
        std::move(associated_program),
        std::make_unique<chrome::conflicts::BlacklistAction>(
            *blacklist_action));
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
