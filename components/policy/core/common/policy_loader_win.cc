// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_win.h"

#include <ntdsapi.h>  // For Ds[Un]Bind
#include <rpc.h>      // For struct GUID
#include <shlwapi.h>  // For PathIsUNC()
#include <stddef.h>
#include <userenv.h>  // For GPO functions

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_native_library.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "components/json_schema/json_schema_constants.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_load_status.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/preg_parser.h"
#include "components/policy/core/common/registry_dict.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"

namespace schema = json_schema_constants;

namespace policy {

namespace {

const char kKeyMandatory[] = "policy";
const char kKeyRecommended[] = "recommended";
const char kKeyThirdParty[] = "3rdparty";

// The web store url that is the only trusted source for extensions.
const char kExpectedWebStoreUrl[] =
    ";https://clients2.google.com/service/update2/crx";
// String to be prepended to each blocked entry.
const char kBlockedExtensionPrefix[] = "[BLOCKED]";

// List of policies that are considered only if the user is part of a AD domain.
// Please document any new additions in policy_templates.json!
const char* kInsecurePolicies[] = {
    key::kMetricsReportingEnabled, key::kDefaultSearchProviderEnabled,
    key::kHomepageIsNewTabPage,    key::kHomepageLocation,
    key::kNewTabPageLocation,      key::kRestoreOnStartup,
    key::kRestoreOnStartupURLs,    key::kSafeBrowsingForTrustedSourcesEnabled,
    key::kCloudPolicyOverridesMachinePolicy};

#pragma warning(push)
#pragma warning(disable : 4068)  // unknown pragmas
// TODO(dcheng): Remove pragma once http://llvm.org/PR24007 is fixed.
#pragma clang diagnostic ignored "-Wmissing-braces"
// The GUID of the registry settings group policy extension.
GUID kRegistrySettingsCSEGUID = REGISTRY_EXTENSION_GUID;
#pragma warning(pop)

// The list of possible errors that can occur while collecting information about
// the current enterprise environment.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum DomainCheckErrors {
  // The check error below is no longer possible.
  DEPRECATED_DOMAIN_CHECK_ERROR_GET_JOIN_INFO = 0,
  DOMAIN_CHECK_ERROR_DS_BIND = 1,
  DOMAIN_CHECK_ERROR_SIZE,  // Not a DomainCheckError.  Must be last.
};

// Encapculates logic to determine if enterprise policies should be honored.
// This is used in various places below.
bool ShouldHonorPolicies() {
  return base::win::IsEnterpriseManaged();
}

// Verifies that untrusted policies contain only safe values. Modifies the
// |policy| in place.
void FilterUntrustedPolicy(PolicyMap* policy) {
  if (ShouldHonorPolicies())
    return;

  int invalid_policies = 0;
  const PolicyMap::Entry* map_entry =
      policy->Get(key::kExtensionInstallForcelist);
  if (map_entry && map_entry->value) {
    const base::ListValue* policy_list_value = NULL;
    if (!map_entry->value->GetAsList(&policy_list_value))
      return;

    std::unique_ptr<base::ListValue> filtered_values(new base::ListValue);
    for (const auto& list_entry : *policy_list_value) {
      std::string entry;
      if (!list_entry.GetAsString(&entry))
        continue;
      size_t pos = entry.find(';');
      if (pos == std::string::npos)
        continue;
      // Only allow custom update urls in enterprise environments.
      if (!base::LowerCaseEqualsASCII(entry.substr(pos),
                                      kExpectedWebStoreUrl)) {
        entry = kBlockedExtensionPrefix + entry;
        invalid_policies++;
      }

      filtered_values->AppendString(entry);
    }
    if (invalid_policies) {
      PolicyMap::Entry filtered_entry = map_entry->DeepCopy();
      filtered_entry.value = std::move(filtered_values);
      policy->Set(key::kExtensionInstallForcelist, std::move(filtered_entry));

      const PolicyDetails* details =
          GetChromePolicyDetails(key::kExtensionInstallForcelist);
      UMA_HISTOGRAM_SPARSE_SLOWLY("EnterpriseCheck.InvalidPolicies",
                                  details->id);
    }
  }

  for (size_t i = 0; i < arraysize(kInsecurePolicies); ++i) {
    if (policy->Get(kInsecurePolicies[i])) {
      // TODO(pastarmovj): Surface this issue in the about:policy page.
      policy->Erase(kInsecurePolicies[i]);
      invalid_policies++;
      const PolicyDetails* details =
          GetChromePolicyDetails(kInsecurePolicies[i]);
      UMA_HISTOGRAM_SPARSE_SLOWLY("EnterpriseCheck.InvalidPolicies",
                                  details->id);
    }
  }

  UMA_HISTOGRAM_COUNTS("EnterpriseCheck.InvalidPoliciesDetected",
                       invalid_policies);
}

// A helper class encapsulating run-time-linked function calls to Wow64 APIs.
class Wow64Functions {
 public:
  Wow64Functions()
      : kernel32_lib_(base::FilePath(L"kernel32")),
        is_wow_64_process_(NULL),
        wow_64_disable_wow_64_fs_redirection_(NULL),
        wow_64_revert_wow_64_fs_redirection_(NULL) {
    if (kernel32_lib_.is_valid()) {
      is_wow_64_process_ = reinterpret_cast<IsWow64Process>(
          kernel32_lib_.GetFunctionPointer("IsWow64Process"));
      wow_64_disable_wow_64_fs_redirection_ =
          reinterpret_cast<Wow64DisableWow64FSRedirection>(
              kernel32_lib_.GetFunctionPointer(
                  "Wow64DisableWow64FsRedirection"));
      wow_64_revert_wow_64_fs_redirection_ =
          reinterpret_cast<Wow64RevertWow64FSRedirection>(
              kernel32_lib_.GetFunctionPointer(
                  "Wow64RevertWow64FsRedirection"));
    }
  }

  bool is_valid() {
    return is_wow_64_process_ && wow_64_disable_wow_64_fs_redirection_ &&
           wow_64_revert_wow_64_fs_redirection_;
  }

  bool IsWow64() {
    BOOL result = 0;
    if (!is_wow_64_process_(GetCurrentProcess(), &result))
      PLOG(WARNING) << "IsWow64ProcFailed";
    return !!result;
  }

  bool DisableFsRedirection(PVOID* previous_state) {
    return !!wow_64_disable_wow_64_fs_redirection_(previous_state);
  }

  bool RevertFsRedirection(PVOID previous_state) {
    return !!wow_64_revert_wow_64_fs_redirection_(previous_state);
  }

 private:
  typedef BOOL(WINAPI* IsWow64Process)(HANDLE, PBOOL);
  typedef BOOL(WINAPI* Wow64DisableWow64FSRedirection)(PVOID*);
  typedef BOOL(WINAPI* Wow64RevertWow64FSRedirection)(PVOID);

  base::ScopedNativeLibrary kernel32_lib_;

  IsWow64Process is_wow_64_process_;
  Wow64DisableWow64FSRedirection wow_64_disable_wow_64_fs_redirection_;
  Wow64RevertWow64FSRedirection wow_64_revert_wow_64_fs_redirection_;

  DISALLOW_COPY_AND_ASSIGN(Wow64Functions);
};

// Global Wow64Function instance used by ScopedDisableWow64Redirection below.
static base::LazyInstance<Wow64Functions>::DestructorAtExit g_wow_64_functions =
    LAZY_INSTANCE_INITIALIZER;

// Scoper that switches off Wow64 File System Redirection during its lifetime.
class ScopedDisableWow64Redirection {
 public:
  ScopedDisableWow64Redirection() : active_(false), previous_state_(NULL) {
    Wow64Functions* wow64 = g_wow_64_functions.Pointer();
    if (wow64->is_valid() && wow64->IsWow64()) {
      if (wow64->DisableFsRedirection(&previous_state_))
        active_ = true;
      else
        PLOG(WARNING) << "Wow64DisableWow64FSRedirection";
    }
  }

  ~ScopedDisableWow64Redirection() {
    if (active_)
      CHECK(g_wow_64_functions.Get().RevertFsRedirection(previous_state_));
  }

  bool is_active() { return active_; }

 private:
  bool active_;
  PVOID previous_state_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDisableWow64Redirection);
};

// AppliedGPOListProvider implementation that calls actual Windows APIs.
class WinGPOListProvider : public AppliedGPOListProvider {
 public:
  ~WinGPOListProvider() override {}

  // AppliedGPOListProvider:
  DWORD GetAppliedGPOList(DWORD flags,
                          LPCTSTR machine_name,
                          PSID sid_user,
                          GUID* extension_guid,
                          PGROUP_POLICY_OBJECT* gpo_list) override {
    return ::GetAppliedGPOList(flags, machine_name, sid_user, extension_guid,
                               gpo_list);
  }

  BOOL FreeGPOList(PGROUP_POLICY_OBJECT gpo_list) override {
    return ::FreeGPOList(gpo_list);
  }
};

// The default windows GPO list provider used for PolicyLoaderWin.
static base::LazyInstance<WinGPOListProvider>::DestructorAtExit
    g_win_gpo_list_provider = LAZY_INSTANCE_INITIALIZER;

// Parses |gpo_dict| according to |schema| and writes the resulting policy
// settings to |policy| for the given |scope| and |level|.
void ParsePolicy(const RegistryDict* gpo_dict,
                 PolicyLevel level,
                 PolicyScope scope,
                 const Schema& schema,
                 PolicyMap* policy) {
  if (!gpo_dict)
    return;

  std::unique_ptr<base::Value> policy_value(gpo_dict->ConvertToJSON(schema));
  const base::DictionaryValue* policy_dict = NULL;
  if (!policy_value->GetAsDictionary(&policy_dict) || !policy_dict) {
    LOG(WARNING) << "Root policy object is not a dictionary!";
    return;
  }

  policy->LoadFrom(policy_dict, level, scope, POLICY_SOURCE_PLATFORM);
}

// Collects stats about the enterprise environment that can be used to decide
// how to parse the existing policy information.
void CollectEnterpriseUMAs() {
  // Collect statistics about the windows suite.
  UMA_HISTOGRAM_ENUMERATION("EnterpriseCheck.OSType",
                            base::win::OSInfo::GetInstance()->version_type(),
                            base::win::SUITE_LAST);

  UMA_HISTOGRAM_BOOLEAN("EnterpriseCheck.InDomain",
                        base::win::IsEnrolledToDomain());
  UMA_HISTOGRAM_BOOLEAN("EnterpriseCheck.IsManaged",
                        base::win::IsDeviceRegisteredWithManagement());
  UMA_HISTOGRAM_BOOLEAN("EnterpriseCheck.IsEnterpriseUser",
                        base::win::IsEnterpriseManaged());
}

}  // namespace

const base::FilePath::CharType PolicyLoaderWin::kPRegFileName[] =
    FILE_PATH_LITERAL("Registry.pol");

PolicyLoaderWin::PolicyLoaderWin(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::string16& chrome_policy_key,
    AppliedGPOListProvider* gpo_provider)
    : AsyncPolicyLoader(task_runner),
      is_initialized_(false),
      chrome_policy_key_(chrome_policy_key),
      gpo_provider_(gpo_provider),
      user_policy_changed_event_(
          base::WaitableEvent::ResetPolicy::AUTOMATIC,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      machine_policy_changed_event_(
          base::WaitableEvent::ResetPolicy::AUTOMATIC,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      user_policy_watcher_failed_(false),
      machine_policy_watcher_failed_(false) {
  if (!::RegisterGPNotification(user_policy_changed_event_.handle(), false)) {
    DPLOG(WARNING) << "Failed to register user group policy notification";
    user_policy_watcher_failed_ = true;
  }
  if (!::RegisterGPNotification(machine_policy_changed_event_.handle(), true)) {
    DPLOG(WARNING) << "Failed to register machine group policy notification.";
    machine_policy_watcher_failed_ = true;
  }
}

PolicyLoaderWin::~PolicyLoaderWin() {
  if (!user_policy_watcher_failed_) {
    ::UnregisterGPNotification(user_policy_changed_event_.handle());
    user_policy_watcher_.StopWatching();
  }
  if (!machine_policy_watcher_failed_) {
    ::UnregisterGPNotification(machine_policy_changed_event_.handle());
    machine_policy_watcher_.StopWatching();
  }
}

// static
std::unique_ptr<PolicyLoaderWin> PolicyLoaderWin::Create(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::string16& chrome_policy_key) {
  return base::WrapUnique(new PolicyLoaderWin(
      task_runner, chrome_policy_key, g_win_gpo_list_provider.Pointer()));
}

void PolicyLoaderWin::InitOnBackgroundThread() {
  is_initialized_ = true;
  SetupWatches();
  CollectEnterpriseUMAs();
}

std::unique_ptr<PolicyBundle> PolicyLoaderWin::Load() {
  // Reset the watches BEFORE reading the individual policies to avoid
  // missing a change notification.
  if (is_initialized_)
    SetupWatches();

  // Policy scope and corresponding hive.
  static const struct {
    PolicyScope scope;
    HKEY hive;
  } kScopes[] = {
      {POLICY_SCOPE_MACHINE, HKEY_LOCAL_MACHINE},
      {POLICY_SCOPE_USER, HKEY_CURRENT_USER},
  };

  bool honor_policies = ShouldHonorPolicies();
  VLOG(1) << "Reading policy from the registry is "
          << (honor_policies ? "enabled." : "disabled.");

  // Load policy data for the different scopes/levels and merge them.
  std::unique_ptr<PolicyBundle> bundle(new PolicyBundle());
  PolicyMap* chrome_policy =
      &bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  for (size_t i = 0; i < arraysize(kScopes); ++i) {
    PolicyScope scope = kScopes[i].scope;
    PolicyLoadStatusUmaReporter status;
    RegistryDict gpo_dict;

    // Note: GPO rules mandate a call to EnterCriticalPolicySection() here, and
    // a matching LeaveCriticalPolicySection() call below after the
    // ReadPolicyFromGPO() block. Unfortunately, the policy mutex may be
    // unavailable for extended periods of time, and there are reports of this
    // happening in the wild: http://crbug.com/265862.
    //
    // Blocking for minutes is neither acceptable for Chrome startup, nor on
    // the FILE thread on which this code runs in steady state. Given that
    // there have never been any reports of issues due to partially-applied /
    // corrupt group policy, this code intentionally omits the
    // EnterCriticalPolicySection() call.
    //
    // If there's ever reason to revisit this decision, one option could be to
    // make the EnterCriticalPolicySection() call on a dedicated thread and
    // timeout on it more aggressively. For now, there's no justification for
    // the additional effort this would introduce.

    bool is_registry_forced = honor_policies || gpo_provider_ == nullptr;
    if (is_registry_forced || !ReadPolicyFromGPO(scope, &gpo_dict, &status)) {
      VLOG_IF(1, !is_registry_forced) << "Failed to read GPO files for "
                                      << scope << " falling back to registry.";
      gpo_dict.ReadRegistry(kScopes[i].hive, chrome_policy_key_);
    }

    // Remove special-cased entries from the GPO dictionary.
    std::unique_ptr<RegistryDict> recommended_dict(
        gpo_dict.RemoveKey(kKeyRecommended));
    std::unique_ptr<RegistryDict> third_party_dict(
        gpo_dict.RemoveKey(kKeyThirdParty));

    // Load Chrome policy.
    LoadChromePolicy(&gpo_dict, POLICY_LEVEL_MANDATORY, scope, chrome_policy);
    LoadChromePolicy(recommended_dict.get(), POLICY_LEVEL_RECOMMENDED, scope,
                     chrome_policy);

    // Load 3rd-party policy.
    if (third_party_dict)
      Load3rdPartyPolicy(third_party_dict.get(), scope, bundle.get());
  }

  return bundle;
}

bool PolicyLoaderWin::ReadPRegFile(const base::FilePath& preg_file,
                                   RegistryDict* policy,
                                   PolicyLoadStatusSampler* status) {
  // The following deals with the minor annoyance that Wow64 FS redirection
  // might need to be turned off: This is the case if running as a 32-bit
  // process on a 64-bit system, in which case Wow64 FS redirection redirects
  // access to the %WINDIR%/System32/GroupPolicy directory to
  // %WINDIR%/SysWOW64/GroupPolicy, but the file is actually in the
  // system-native directory.
  if (base::PathExists(preg_file)) {
    return preg_parser::ReadFile(preg_file, chrome_policy_key_, policy, status);
  } else {
    // Try with redirection switched off.
    ScopedDisableWow64Redirection redirection_disable;
    if (redirection_disable.is_active() && base::PathExists(preg_file)) {
      status->Add(POLICY_LOAD_STATUS_WOW64_REDIRECTION_DISABLED);
      return preg_parser::ReadFile(preg_file, chrome_policy_key_, policy,
                                   status);
    }
  }

  // Report the error.
  LOG(ERROR) << "PReg file doesn't exist: " << preg_file.value();
  status->Add(POLICY_LOAD_STATUS_MISSING);
  return false;
}

bool PolicyLoaderWin::LoadGPOPolicy(PolicyScope scope,
                                    PGROUP_POLICY_OBJECT policy_object_list,
                                    RegistryDict* policy,
                                    PolicyLoadStatusSampler* status) {
  RegistryDict parsed_policy;
  RegistryDict forced_policy;
  for (GROUP_POLICY_OBJECT* policy_object = policy_object_list; policy_object;
       policy_object = policy_object->pNext) {
    if (policy_object->dwOptions & GPO_FLAG_DISABLE)
      continue;

    if (PathIsUNC(policy_object->lpFileSysPath)) {
      // UNC path: Assume this is an AD-managed machine, which updates the
      // registry via GPO's standard registry CSE periodically. Fall back to
      // reading from the registry in this case.
      status->Add(POLICY_LOAD_STATUS_INACCCESSIBLE);
      return false;
    }

    base::FilePath preg_file_path(
        base::FilePath(policy_object->lpFileSysPath).Append(kPRegFileName));
    if (policy_object->dwOptions & GPO_FLAG_FORCE) {
      RegistryDict new_forced_policy;
      if (!ReadPRegFile(preg_file_path, &new_forced_policy, status))
        return false;

      // Merge with existing forced policy, giving precedence to the existing
      // forced policy.
      // TODO(ljusten): Same problem as below.
      new_forced_policy.Merge(forced_policy);
      forced_policy.Swap(&new_forced_policy);
    } else {
      if (!ReadPRegFile(preg_file_path, &parsed_policy, status))
        return false;
    }
  }

  // Merge, give precedence to forced policy.
  // TODO(ljusten): This doesn't work properly if policies are explicitly
  // disabled or string list policies have less entries in the forced_policy.
  // The merged dictionary still has excessive policies and strings. To fix
  // this, use only one dict and call ReadPRegFile in the proper order (forced
  // last). See crbug.com/659979.
  parsed_policy.Merge(forced_policy);
  policy->Swap(&parsed_policy);

  return true;
}

bool PolicyLoaderWin::ReadPolicyFromGPO(PolicyScope scope,
                                        RegistryDict* policy,
                                        PolicyLoadStatusSampler* status) {
  PGROUP_POLICY_OBJECT policy_object_list = NULL;
  DWORD flags = scope == POLICY_SCOPE_MACHINE ? GPO_LIST_FLAG_MACHINE : 0;
  if (gpo_provider_->GetAppliedGPOList(flags, NULL, NULL,
                                       &kRegistrySettingsCSEGUID,
                                       &policy_object_list) != ERROR_SUCCESS) {
    PLOG(ERROR) << "GetAppliedGPOList scope " << scope;
    status->Add(POLICY_LOAD_STATUS_QUERY_FAILED);
    return false;
  }

  bool result = true;
  if (policy_object_list) {
    result = LoadGPOPolicy(scope, policy_object_list, policy, status);
    if (!gpo_provider_->FreeGPOList(policy_object_list))
      LOG(WARNING) << "FreeGPOList";
  } else {
    status->Add(POLICY_LOAD_STATUS_NO_POLICY);
  }

  return result;
}

void PolicyLoaderWin::LoadChromePolicy(const RegistryDict* gpo_dict,
                                       PolicyLevel level,
                                       PolicyScope scope,
                                       PolicyMap* chrome_policy_map) {
  PolicyMap policy;
  const Schema* chrome_schema =
      schema_map()->GetSchema(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));
  ParsePolicy(gpo_dict, level, scope, *chrome_schema, &policy);
  FilterUntrustedPolicy(&policy);
  chrome_policy_map->MergeFrom(policy);
}

void PolicyLoaderWin::Load3rdPartyPolicy(const RegistryDict* gpo_dict,
                                         PolicyScope scope,
                                         PolicyBundle* bundle) {
  // Map of known 3rd party policy domain name to their enum values.
  static const struct {
    const char* name;
    PolicyDomain domain;
  } k3rdPartyDomains[] = {
      {"extensions", POLICY_DOMAIN_EXTENSIONS},
  };

  // Policy level and corresponding path.
  static const struct {
    PolicyLevel level;
    const char* path;
  } kLevels[] = {
      {POLICY_LEVEL_MANDATORY, kKeyMandatory},
      {POLICY_LEVEL_RECOMMENDED, kKeyRecommended},
  };

  for (size_t i = 0; i < arraysize(k3rdPartyDomains); i++) {
    const char* name = k3rdPartyDomains[i].name;
    const PolicyDomain domain = k3rdPartyDomains[i].domain;
    const RegistryDict* domain_dict = gpo_dict->GetKey(name);
    if (!domain_dict)
      continue;

    for (RegistryDict::KeyMap::const_iterator component(
             domain_dict->keys().begin());
         component != domain_dict->keys().end(); ++component) {
      const PolicyNamespace policy_namespace(domain, component->first);

      const Schema* schema_from_map = schema_map()->GetSchema(policy_namespace);
      if (!schema_from_map) {
        // This extension isn't installed or doesn't support policies.
        continue;
      }
      Schema schema = *schema_from_map;

      // Parse policy.
      for (size_t j = 0; j < arraysize(kLevels); j++) {
        const RegistryDict* policy_dict =
            component->second->GetKey(kLevels[j].path);
        if (!policy_dict)
          continue;

        PolicyMap policy;
        ParsePolicy(policy_dict, kLevels[j].level, scope, schema, &policy);
        bundle->Get(policy_namespace).MergeFrom(policy);
      }
    }
  }
}

void PolicyLoaderWin::SetupWatches() {
  DCHECK(is_initialized_);
  if (!user_policy_watcher_failed_ &&
      !user_policy_watcher_.GetWatchedObject() &&
      !user_policy_watcher_.StartWatchingOnce(
          user_policy_changed_event_.handle(), this)) {
    DLOG(WARNING) << "Failed to start watch for user policy change event";
    user_policy_watcher_failed_ = true;
  }
  if (!machine_policy_watcher_failed_ &&
      !machine_policy_watcher_.GetWatchedObject() &&
      !machine_policy_watcher_.StartWatchingOnce(
          machine_policy_changed_event_.handle(), this)) {
    DLOG(WARNING) << "Failed to start watch for machine policy change event";
    machine_policy_watcher_failed_ = true;
  }
}

void PolicyLoaderWin::OnObjectSignaled(HANDLE object) {
  DCHECK(object == user_policy_changed_event_.handle() ||
         object == machine_policy_changed_event_.handle())
      << "unexpected object signaled policy reload, obj = " << std::showbase
      << std::hex << object;
  Reload(false);
}

}  // namespace policy
