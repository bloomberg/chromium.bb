// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/environment_data_collection_win.h"

#include <windows.h>
#include <set>

#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/install_verification/win/module_info.h"
#include "chrome/browser/install_verification/win/module_verification_common.h"
#include "chrome/browser/net/service_providers_win.h"
#include "chrome/browser/safe_browsing/module_integrity_verifier_win.h"
#include "chrome/browser/safe_browsing/path_sanitizer.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome_elf/chrome_elf_constants.h"

namespace safe_browsing {

namespace {

// The modules on which we will run VerifyModule.
const wchar_t* const kModulesToVerify[] = {
    L"chrome.dll",
    L"chrome_elf.dll",
    L"ntdll.dll",
};

// Helper function for expanding all environment variables in |path|.
std::wstring ExpandEnvironmentVariables(const std::wstring& path) {
  static const DWORD kMaxBuffer = 32 * 1024;  // Max according to MSDN.
  std::wstring path_expanded;
  DWORD path_len = MAX_PATH;
  do {
    DWORD result = ExpandEnvironmentStrings(
        path.c_str(), WriteInto(&path_expanded, path_len), path_len);
    if (!result) {
      // Failed to expand variables. Return the original string.
      DPLOG(ERROR) << path;
      break;
    }
    if (result <= path_len)
      return path_expanded.substr(0, result - 1);
    path_len = result;
  } while (path_len < kMaxBuffer);

  return path;
}

}  // namespace

bool CollectDlls(ClientIncidentReport_EnvironmentData_Process* process) {
  // Retrieve the module list.
  std::set<ModuleInfo> loaded_modules;
  if (!GetLoadedModules(&loaded_modules))
    return false;

  // Sanitize path of each module and add it to the incident report.
  PathSanitizer path_sanitizer;
  for (std::set<ModuleInfo>::const_iterator it = loaded_modules.begin();
       it != loaded_modules.end();
       ++it) {
    base::FilePath dll_path(it->name);
    path_sanitizer.StripHomeDirectory(&dll_path);

    ClientIncidentReport_EnvironmentData_Process_Dll* dll = process->add_dll();
    dll->set_path(base::WideToUTF8(base::i18n::ToLower(dll_path.value())));
    dll->set_base_address(it->base_address);
    dll->set_length(it->size);
  }

  return true;
}

void RecordLspFeature(ClientIncidentReport_EnvironmentData_Process* process) {
  WinsockLayeredServiceProviderList lsp_list;
  GetWinsockLayeredServiceProviders(&lsp_list);

  // For each LSP, we extract and sanitize the path.
  PathSanitizer path_sanitizer;
  std::set<std::wstring> lsp_paths;
  for (size_t i = 0; i < lsp_list.size(); ++i) {
    base::FilePath lsp_path(ExpandEnvironmentVariables(lsp_list[i].path));
    path_sanitizer.StripHomeDirectory(&lsp_path);
    lsp_paths.insert(base::i18n::ToLower(lsp_path.value()));
  }

  // Look for a match between LSPs and loaded dlls.
  for (int i = 0; i < process->dll_size(); ++i) {
    if (lsp_paths.count(base::UTF8ToWide(process->dll(i).path()))) {
      process->mutable_dll(i)
          ->add_feature(ClientIncidentReport_EnvironmentData_Process_Dll::LSP);
    }
  }
}

void CollectDllBlacklistData(
    ClientIncidentReport_EnvironmentData_Process* process) {
  PathSanitizer path_sanitizer;
  base::win::RegistryValueIterator iter(HKEY_CURRENT_USER,
                                        blacklist::kRegistryFinchListPath);
  for (; iter.Valid(); ++iter) {
    base::FilePath dll_name(iter.Value());
    path_sanitizer.StripHomeDirectory(&dll_name);
    process->add_blacklisted_dll(dll_name.AsUTF8Unsafe());
  }
}

void CollectModuleVerificationData(
    const wchar_t* const modules_to_verify[],
    size_t num_modules_to_verify,
    ClientIncidentReport_EnvironmentData_Process* process) {
  for (size_t i = 0; i < num_modules_to_verify; ++i) {
    std::set<std::string> modified_exports;
    int modified = VerifyModule(modules_to_verify[i], &modified_exports);

    if (modified == MODULE_STATE_UNMODIFIED)
      continue;

    ClientIncidentReport_EnvironmentData_Process_ModuleState* module_state =
        process->add_module_state();

    module_state->set_name(
        base::WideToUTF8(std::wstring(modules_to_verify[i])));
    // Add 1 to the ModuleState enum to get the corresponding value in the
    // protobuf's ModuleState enum.
    module_state->set_modified_state(static_cast<
        ClientIncidentReport_EnvironmentData_Process_ModuleState_ModifiedState>(
        modified + 1));
    for (std::set<std::string>::iterator it = modified_exports.begin();
         it != modified_exports.end();
         ++it) {
      module_state->add_modified_export(*it);
    }
  }
}

void CollectPlatformProcessData(
    ClientIncidentReport_EnvironmentData_Process* process) {
  CollectDlls(process);
  RecordLspFeature(process);
  CollectDllBlacklistData(process);
  CollectModuleVerificationData(
      kModulesToVerify, arraysize(kModulesToVerify), process);
}

}  // namespace safe_browsing
