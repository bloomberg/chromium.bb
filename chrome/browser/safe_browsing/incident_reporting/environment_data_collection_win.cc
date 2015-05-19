// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/environment_data_collection_win.h"

#include <windows.h>
#include <set>
#include <string>

#include "base/i18n/case_conversion.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/install_verification/win/module_info.h"
#include "chrome/browser/install_verification/win/module_verification_common.h"
#include "chrome/browser/net/service_providers_win.h"
#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_verifier_win.h"
#include "chrome/browser/safe_browsing/path_sanitizer.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
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

  // Sanitize path of each module and add it to the incident report along with
  // its headers.
  PathSanitizer path_sanitizer;
  scoped_refptr<BinaryFeatureExtractor> feature_extractor(
      new BinaryFeatureExtractor());
  for (const auto& module : loaded_modules) {
    base::FilePath dll_path(module.name);
    base::FilePath sanitized_path(dll_path);
    path_sanitizer.StripHomeDirectory(&sanitized_path);

    ClientIncidentReport_EnvironmentData_Process_Dll* dll = process->add_dll();
    dll->set_path(
        base::WideToUTF8(base::i18n::ToLower(sanitized_path.value())));
    dll->set_base_address(module.base_address);
    dll->set_length(module.size);
    // TODO(grt): Consider skipping this for valid system modules.
    if (!feature_extractor->ExtractImageFeatures(
            dll_path,
            BinaryFeatureExtractor::kOmitExports,
            dll->mutable_image_headers(),
            nullptr /* signed_data */)) {
      dll->clear_image_headers();
    }
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
#if !defined(_WIN64)
  using ModuleState = ClientIncidentReport_EnvironmentData_Process_ModuleState;

  for (size_t i = 0; i < num_modules_to_verify; ++i) {
    scoped_ptr<ModuleState> module_state(new ModuleState());

    int num_bytes_different = 0;
    bool scan_complete = VerifyModule(modules_to_verify[i],
                                      module_state.get(),
                                      &num_bytes_different);

    if (module_state->modified_state() == ModuleState::MODULE_STATE_UNMODIFIED)
      continue;

    if (module_state->modified_state() == ModuleState::MODULE_STATE_MODIFIED) {
      UMA_HISTOGRAM_COUNTS_10000(
          "ModuleIntegrityVerification.BytesModified.WithoutByteSet",
          num_bytes_different);
    }

    if (!scan_complete) {
      UMA_HISTOGRAM_ENUMERATION(
          "ModuleIntegrityVerification.RelocationsUnordered", i,
          num_modules_to_verify);
    }

    process->mutable_module_state()->AddAllocated(module_state.release());
  }
#endif  // _WIN64
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
