// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project.

#include "chrome/installer/setup/setup_util.h"

#include <windows.h>
#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff.h"
#include "third_party/bspatch/mbspatch.h"

namespace installer {

namespace {

// Returns true if product |type| cam be meaningfully installed without the
// --multi-install flag.
bool SupportsSingleInstall(BrowserDistribution::Type type) {
  return (type == BrowserDistribution::CHROME_BROWSER ||
          type == BrowserDistribution::CHROME_FRAME);
}

}  // namespace

int CourgettePatchFiles(const base::FilePath& src,
                        const base::FilePath& patch,
                        const base::FilePath& dest) {
  VLOG(1) << "Applying Courgette patch " << patch.value()
          << " to file " << src.value()
          << " and generating file " << dest.value();

  if (src.empty() || patch.empty() || dest.empty())
    return installer::PATCH_INVALID_ARGUMENTS;

  const courgette::Status patch_status =
      courgette::ApplyEnsemblePatch(src.value().c_str(),
                                    patch.value().c_str(),
                                    dest.value().c_str());
  const int exit_code = (patch_status != courgette::C_OK) ?
      static_cast<int>(patch_status) + kCourgetteErrorOffset : 0;

  LOG_IF(ERROR, exit_code)
      << "Failed to apply Courgette patch " << patch.value()
      << " to file " << src.value() << " and generating file " << dest.value()
      << ". err=" << exit_code;

  return exit_code;
}

int BsdiffPatchFiles(const base::FilePath& src,
                     const base::FilePath& patch,
                     const base::FilePath& dest) {
  VLOG(1) << "Applying bsdiff patch " << patch.value()
          << " to file " << src.value()
          << " and generating file " << dest.value();

  if (src.empty() || patch.empty() || dest.empty())
    return installer::PATCH_INVALID_ARGUMENTS;

  const int patch_status = courgette::ApplyBinaryPatch(src, patch, dest);
  const int exit_code = patch_status != OK ?
                        patch_status + kBsdiffErrorOffset : 0;

  LOG_IF(ERROR, exit_code)
      << "Failed to apply bsdiff patch " << patch.value()
      << " to file " << src.value() << " and generating file " << dest.value()
      << ". err=" << exit_code;

  return exit_code;
}

Version* GetMaxVersionFromArchiveDir(const base::FilePath& chrome_path) {
  VLOG(1) << "Looking for Chrome version folder under " << chrome_path.value();
  base::FileEnumerator version_enum(chrome_path, false,
      base::FileEnumerator::DIRECTORIES);
  // TODO(tommi): The version directory really should match the version of
  // setup.exe.  To begin with, we should at least DCHECK that that's true.

  std::unique_ptr<Version> max_version(new Version("0.0.0.0"));
  bool version_found = false;

  while (!version_enum.Next().empty()) {
    base::FileEnumerator::FileInfo find_data = version_enum.GetInfo();
    VLOG(1) << "directory found: " << find_data.GetName().value();

    std::unique_ptr<Version> found_version(
        new Version(base::UTF16ToASCII(find_data.GetName().value())));
    if (found_version->IsValid() &&
        found_version->CompareTo(*max_version.get()) > 0) {
      max_version.reset(found_version.release());
      version_found = true;
    }
  }

  return (version_found ? max_version.release() : NULL);
}

base::FilePath FindArchiveToPatch(const InstallationState& original_state,
                                  const InstallerState& installer_state,
                                  const base::Version& desired_version) {
  if (desired_version.IsValid()) {
    base::FilePath archive(installer_state.GetInstallerDirectory(
        desired_version).Append(kChromeArchive));
    return base::PathExists(archive) ? archive : base::FilePath();
  }

  // Check based on the version number advertised to Google Update, since that
  // is the value used to select a specific differential update. If an archive
  // can't be found using that, fallback to using the newest version present.
  base::FilePath patch_source;
  const ProductState* product =
      original_state.GetProductState(installer_state.system_install(),
                                     installer_state.state_type());
  if (product) {
    patch_source = installer_state.GetInstallerDirectory(product->version())
        .Append(installer::kChromeArchive);
    if (base::PathExists(patch_source))
      return patch_source;
  }
  std::unique_ptr<Version> version(
      installer::GetMaxVersionFromArchiveDir(installer_state.target_path()));
  if (version) {
    patch_source = installer_state.GetInstallerDirectory(*version)
        .Append(installer::kChromeArchive);
    if (base::PathExists(patch_source))
      return patch_source;
  }
  return base::FilePath();
}

bool DeleteFileFromTempProcess(const base::FilePath& path,
                               uint32_t delay_before_delete_ms) {
  static const wchar_t kRunDll32Path[] =
      L"%SystemRoot%\\System32\\rundll32.exe";
  wchar_t rundll32[MAX_PATH];
  DWORD size =
      ExpandEnvironmentStrings(kRunDll32Path, rundll32, arraysize(rundll32));
  if (!size || size >= MAX_PATH)
    return false;

  STARTUPINFO startup = { sizeof(STARTUPINFO) };
  PROCESS_INFORMATION pi = {0};
  BOOL ok = ::CreateProcess(NULL, rundll32, NULL, NULL, FALSE, CREATE_SUSPENDED,
                            NULL, NULL, &startup, &pi);
  if (ok) {
    // We use the main thread of the new process to run:
    //   Sleep(delay_before_delete_ms);
    //   DeleteFile(path);
    //   ExitProcess(0);
    // This runs before the main routine of the process runs, so it doesn't
    // matter much which executable we choose except that we don't want to
    // use e.g. a console app that causes a window to be created.
    size = static_cast<DWORD>(
        (path.value().length() + 1) * sizeof(path.value()[0]));
    void* mem = ::VirtualAllocEx(pi.hProcess, NULL, size, MEM_COMMIT,
                                 PAGE_READWRITE);
    if (mem) {
      SIZE_T written = 0;
      ::WriteProcessMemory(
          pi.hProcess, mem, path.value().c_str(),
          (path.value().size() + 1) * sizeof(path.value()[0]), &written);
      HMODULE kernel32 = ::GetModuleHandle(L"kernel32.dll");
      PAPCFUNC sleep = reinterpret_cast<PAPCFUNC>(
          ::GetProcAddress(kernel32, "Sleep"));
      PAPCFUNC delete_file = reinterpret_cast<PAPCFUNC>(
          ::GetProcAddress(kernel32, "DeleteFileW"));
      PAPCFUNC exit_process = reinterpret_cast<PAPCFUNC>(
          ::GetProcAddress(kernel32, "ExitProcess"));
      if (!sleep || !delete_file || !exit_process) {
        NOTREACHED();
        ok = FALSE;
      } else {
        ::QueueUserAPC(sleep, pi.hThread, delay_before_delete_ms);
        ::QueueUserAPC(delete_file, pi.hThread,
                       reinterpret_cast<ULONG_PTR>(mem));
        ::QueueUserAPC(exit_process, pi.hThread, 0);
        ::ResumeThread(pi.hThread);
      }
    } else {
      PLOG(ERROR) << "VirtualAllocEx";
      ::TerminateProcess(pi.hProcess, ~static_cast<UINT>(0));
    }
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
  }

  return ok != FALSE;
}

// There are 4 disjoint cases => return values {false,true}:
// (1) Product is being uninstalled => false.
// (2) Product is being installed => true.
// (3) Current operation ignores product, product is absent => false.
// (4) Current operation ignores product, product is present => true.
bool WillProductBePresentAfterSetup(
    const installer::InstallerState& installer_state,
    const installer::InstallationState& machine_state,
    BrowserDistribution::Type type) {
  DCHECK(SupportsSingleInstall(type) || installer_state.is_multi_install());

  const ProductState* product_state =
      machine_state.GetProductState(installer_state.system_install(), type);

  // Determine if the product is present prior to the current operation.
  bool is_present = (product_state != NULL);
  bool is_uninstall = installer_state.operation() == InstallerState::UNINSTALL;

  // Determine if current operation affects the product.
  const Product* product = installer_state.FindProduct(type);
  bool is_affected = (product != NULL);

  // Decide among {(1),(2),(3),(4)}.
  return is_affected ? !is_uninstall : is_present;
}

bool AdjustProcessPriority() {
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    DWORD priority_class = ::GetPriorityClass(::GetCurrentProcess());
    if (priority_class == 0) {
      PLOG(WARNING) << "Failed to get the process's priority class.";
    } else if (priority_class == BELOW_NORMAL_PRIORITY_CLASS ||
               priority_class == IDLE_PRIORITY_CLASS) {
      BOOL result = ::SetPriorityClass(::GetCurrentProcess(),
                                       PROCESS_MODE_BACKGROUND_BEGIN);
      PLOG_IF(WARNING, !result) << "Failed to enter background mode.";
      return !!result;
    }
  }
  return false;
}

void MigrateGoogleUpdateStateMultiToSingle(
    bool system_level,
    BrowserDistribution::Type to_migrate,
    const installer::InstallationState& machine_state) {
  const HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  const ProductState* product = NULL;
  BrowserDistribution* dist = NULL;
  LONG result = ERROR_SUCCESS;
  base::win::RegKey state_key;

  Product product_to_migrate(
      BrowserDistribution::GetSpecificDistribution(to_migrate));

  // Copy usagestats from the binaries to the product's ClientState key.
  product = machine_state.GetProductState(system_level,
                                          BrowserDistribution::CHROME_BINARIES);
  DWORD usagestats = 0;
  if (product && product->GetUsageStats(&usagestats)) {
    dist = product_to_migrate.distribution();
    result = state_key.Open(root, dist->GetStateKey().c_str(),
                            KEY_SET_VALUE);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed opening ClientState key for "
                 << dist->GetDisplayName() << " to migrate usagestats.";
    } else {
      state_key.WriteValue(google_update::kRegUsageStatsField, usagestats);
    }
  }

  // Remove the migrating product from the "ap" value of other multi-install
  // products.
  for (int i = 0; i < BrowserDistribution::NUM_TYPES; ++i) {
    BrowserDistribution::Type type =
        static_cast<BrowserDistribution::Type>(i);
    if (type == to_migrate)
      continue;
    product = machine_state.GetProductState(system_level, type);
    if (product && product->is_multi_install()) {
      installer::ChannelInfo channel_info;
      dist = BrowserDistribution::GetSpecificDistribution(type);
      result = state_key.Open(root, dist->GetStateKey().c_str(),
                              KEY_QUERY_VALUE | KEY_SET_VALUE);
      if (result == ERROR_SUCCESS &&
          channel_info.Initialize(state_key) &&
          product_to_migrate.SetChannelFlags(false, &channel_info)) {
        VLOG(1) << "Moving " << dist->GetDisplayName()
                << " to channel: " << channel_info.value();
        channel_info.Write(&state_key);
      }
    }
  }

  // Remove -multi, all product modifiers, and everything else but the channel
  // name from the "ap" value of the product to migrate.
  dist = product_to_migrate.distribution();
  result = state_key.Open(root, dist->GetStateKey().c_str(),
                          KEY_QUERY_VALUE | KEY_SET_VALUE);
  if (result == ERROR_SUCCESS) {
    installer::ChannelInfo channel_info;
    if (!channel_info.Initialize(state_key)) {
      LOG(ERROR) << "Failed reading " << dist->GetDisplayName()
                 << " channel info.";
    } else if (channel_info.RemoveAllModifiersAndSuffixes()) {
      VLOG(1) << "Moving " << dist->GetDisplayName()
              << " to channel: " << channel_info.value();
      channel_info.Write(&state_key);
    }
  }
}

bool IsUninstallSuccess(InstallStatus install_status) {
  // The following status values represent failed uninstalls:
  // 15: CHROME_NOT_INSTALLED
  // 20: UNINSTALL_FAILED
  // 21: UNINSTALL_CANCELLED
  return (install_status == UNINSTALL_SUCCESSFUL ||
          install_status == UNINSTALL_REQUIRES_REBOOT);
}

bool ContainsUnsupportedSwitch(const base::CommandLine& cmd_line) {
  static const char* const kLegacySwitches[] = {
    // Chrome Frame ready-mode.
    "ready-mode",
    "ready-mode-opt-in",
    "ready-mode-temp-opt-out",
    "ready-mode-end-temp-opt-out",
    // Chrome Frame quick-enable.
    "quick-enable-cf",
    // Installation of Chrome Frame.
    "chrome-frame",
    "migrate-chrome-frame",
    // Stand-alone App Launcher.
    "app-host",
    "app-launcher",
  };
  for (size_t i = 0; i < arraysize(kLegacySwitches); ++i) {
    if (cmd_line.HasSwitch(kLegacySwitches[i]))
      return true;
  }
  return false;
}

bool IsProcessorSupported() {
  return base::CPU().has_sse2();
}

base::string16 GetRegistrationDataCommandKey(
    const AppRegistrationData& reg_data,
    const wchar_t* name) {
  base::string16 cmd_key(reg_data.GetVersionKey());
  cmd_key.append(1, base::FilePath::kSeparators[0])
      .append(google_update::kRegCommandsKey)
      .append(1, base::FilePath::kSeparators[0])
      .append(name);
  return cmd_key;
}

void DeleteRegistryKeyPartial(
    HKEY root,
    const base::string16& path,
    const std::vector<base::string16>& keys_to_preserve) {
  // Downcase the list of keys to preserve (all must be ASCII strings).
  std::set<base::string16> lowered_keys_to_preserve;
  std::transform(
      keys_to_preserve.begin(), keys_to_preserve.end(),
      std::inserter(lowered_keys_to_preserve, lowered_keys_to_preserve.begin()),
      [](const base::string16& str) {
        DCHECK(!str.empty());
        DCHECK(base::IsStringASCII(str));
        return base::ToLowerASCII(str);
      });
  base::win::RegKey key;
  LONG result = key.Open(root, path.c_str(), (KEY_ENUMERATE_SUB_KEYS |
                                              KEY_QUERY_VALUE | KEY_SET_VALUE));
  if (result != ERROR_SUCCESS) {
    LOG_IF(ERROR, result != ERROR_FILE_NOT_FOUND) << "Failed to open " << path
                                                  << "; result = " << result;
    return;
  }

  // Repeatedly iterate over all subkeys deleting those that should not be
  // preserved until only those remain. Multiple passes are needed since
  // deleting one key may change the enumeration order of all remaining keys.

  // Subkeys or values to be skipped on subsequent passes.
  std::set<base::string16> to_skip;
  DWORD index = 0;
  const size_t kMaxKeyNameLength = 256;  // MSDN says 255; +1 for terminator.
  base::string16 name(kMaxKeyNameLength, base::char16());
  bool did_delete = false;  // True if at least one item was deleted.
  while (true) {
    DWORD name_length = base::saturated_cast<DWORD>(name.capacity());
    name.resize(name_length);
    result = ::RegEnumKeyEx(key.Handle(), index, &name[0], &name_length,
                            nullptr, nullptr, nullptr, nullptr);
    if (result == ERROR_MORE_DATA) {
      // Unexpected, but perhaps the max key name length was raised. MSDN
      // doesn't clearly say that name_length will contain the necessary
      // length in this case, so double the buffer and try again.
      name.reserve(name.capacity() * 2);
      continue;
    }
    if (result == ERROR_NO_MORE_ITEMS) {
      if (!did_delete)
        break;  // All subkeys were deleted. The job is done.
      // Otherwise, loop again.
      did_delete = false;
      index = 0;
      continue;
    }
    if (result != ERROR_SUCCESS)
      break;
    // Shrink the string to the actual length of the name.
    name.resize(name_length);

    // Skip over this key if it couldn't be deleted on a previous iteration.
    if (to_skip.count(name)) {
      ++index;
      continue;
    }

    // Skip over this key if it is one of the keys to preserve.
    if (base::IsStringASCII(name) &&
        lowered_keys_to_preserve.count(base::ToLowerASCII(name))) {
      // Add the true name of the key to the list of keys to skip for subsequent
      // iterations.
      to_skip.insert(name);
      ++index;
      continue;
    }

    // Delete this key.
    result = key.DeleteKey(name.c_str());
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to delete subkey " << name << " under path "
                 << path;
      // Skip over this key on subsequent iterations.
      to_skip.insert(name);
      ++index;
      continue;
    }
    did_delete = true;
  }

  // Delete the key if it no longer has any subkeys.
  if (to_skip.empty()) {
    result = key.DeleteEmptyKey(L"");
    LOG_IF(ERROR, result != ERROR_SUCCESS) << "Failed to delete empty key "
                                           << path << "; result: " << result;
    return;
  }

  // Delete all values since subkeys are being preserved.
  to_skip.clear();
  did_delete = false;
  index = 0;
  while (true) {
    DWORD name_length = base::saturated_cast<int16_t>(name.capacity());
    name.resize(name_length);
    result = ::RegEnumValue(key.Handle(), index, &name[0], &name_length,
                            nullptr, nullptr, nullptr, nullptr);
    if (result == ERROR_MORE_DATA) {
      if (name_length <
          static_cast<DWORD>(std::numeric_limits<int16_t>::max())) {
        // Double the space to hold the value name and try again.
        name.reserve(name.capacity() * 2);
        continue;
      }
      // Otherwise, the max has been exceeded. Nothing more to be done.
      break;
    }
    if (result == ERROR_NO_MORE_ITEMS) {
      if (!did_delete)
        break;  // All values were deleted. The job is done.
      // Otherwise, loop again.
      did_delete = false;
      index = 0;
      continue;
    }
    if (result != ERROR_SUCCESS)
      break;
    // Shrink the string to the actual length of the name.
    name.resize(name_length);

    // Skip over this value if it couldn't be deleted on a previous iteration.
    if (to_skip.count(name)) {
      ++index;
      continue;
    }

    // Delete this value.
    result = key.DeleteValue(name.c_str());
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to delete value " << name << " in key " << path;
      // Skip over this value on subsequent iterations.
      to_skip.insert(name);
      ++index;
      continue;
    }
    did_delete = true;
  }
}

base::string16 GuidToSquid(const base::string16& guid) {
  base::string16 squid;
  squid.reserve(32);
  auto input = guid.begin();
  auto output = std::back_inserter(squid);

  // Reverse-copy relevant characters, skipping separators.
  std::reverse_copy(input + 0, input + 8, output);
  std::reverse_copy(input + 9, input + 13, output);
  std::reverse_copy(input + 14, input + 18, output);
  std::reverse_copy(input + 19, input + 21, output);
  std::reverse_copy(input + 21, input + 23, output);
  std::reverse_copy(input + 24, input + 26, output);
  std::reverse_copy(input + 26, input + 28, output);
  std::reverse_copy(input + 28, input + 30, output);
  std::reverse_copy(input + 30, input + 32, output);
  std::reverse_copy(input + 32, input + 34, output);
  std::reverse_copy(input + 34, input + 36, output);
  return squid;
}

bool IsDowngradeAllowed(const MasterPreferences& prefs) {
  bool allow_downgrade = false;
  return prefs.GetBool(master_preferences::kAllowDowngrade, &allow_downgrade) &&
         allow_downgrade;
}

ScopedTokenPrivilege::ScopedTokenPrivilege(const wchar_t* privilege_name)
    : is_enabled_(false) {
  HANDLE temp_handle;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          &temp_handle)) {
    return;
  }
  token_.Set(temp_handle);

  LUID privilege_luid;
  if (!::LookupPrivilegeValue(NULL, privilege_name, &privilege_luid)) {
    token_.Close();
    return;
  }

  // Adjust the token's privileges to enable |privilege_name|. If this privilege
  // was already enabled, |previous_privileges_|.PrivilegeCount will be set to 0
  // and we then know not to disable this privilege upon destruction.
  TOKEN_PRIVILEGES tp;
  tp.PrivilegeCount = 1;
  tp.Privileges[0].Luid = privilege_luid;
  tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  DWORD return_length;
  if (!::AdjustTokenPrivileges(token_.Get(), FALSE, &tp,
                               sizeof(TOKEN_PRIVILEGES),
                               &previous_privileges_, &return_length)) {
    token_.Close();
    return;
  }

  is_enabled_ = true;
}

ScopedTokenPrivilege::~ScopedTokenPrivilege() {
  if (is_enabled_ && previous_privileges_.PrivilegeCount != 0) {
    ::AdjustTokenPrivileges(token_.Get(), FALSE, &previous_privileges_,
                            sizeof(TOKEN_PRIVILEGES), NULL, NULL);
  }
}

}  // namespace installer
