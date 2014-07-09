// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// mini_installer.exe is the first exe that is run when chrome is being
// installed or upgraded. It is designed to be extremely small (~5KB with no
// extra resources linked) and it has two main jobs:
//   1) unpack the resources (possibly decompressing some)
//   2) run the real installer (setup.exe) with appropriate flags.
//
// In order to be really small the app doesn't link against the CRT and
// defines the following compiler/linker flags:
//   EnableIntrinsicFunctions="true" compiler: /Oi
//   BasicRuntimeChecks="0"
//   BufferSecurityCheck="false" compiler: /GS-
//   EntryPointSymbol="MainEntryPoint" linker: /ENTRY
//   IgnoreAllDefaultLibraries="true" linker: /NODEFAULTLIB
//   OptimizeForWindows98="1"  liker: /OPT:NOWIN98
//   linker: /SAFESEH:NO

// have the linker merge the sections, saving us ~500 bytes.
#pragma comment(linker, "/MERGE:.rdata=.text")

#include <windows.h>
#include <shellapi.h>

#include "chrome/installer/mini_installer/appid.h"
#include "chrome/installer/mini_installer/configuration.h"
#include "chrome/installer/mini_installer/decompress.h"
#include "chrome/installer/mini_installer/exit_code.h"
#include "chrome/installer/mini_installer/mini_installer_constants.h"
#include "chrome/installer/mini_installer/mini_string.h"
#include "chrome/installer/mini_installer/pe_resource.h"

namespace mini_installer {

typedef DWORD ProcessExitCode;
typedef StackString<MAX_PATH> PathString;
typedef StackString<MAX_PATH * 4> CommandString;

// This structure passes data back and forth for the processing
// of resource callbacks.
struct Context {
  // Input to the call back method. Specifies the dir to save resources.
  const wchar_t* base_path;
  // First output from call back method. Full path of Chrome archive.
  PathString* chrome_resource_path;
  // Second output from call back method. Full path of Setup archive/exe.
  PathString* setup_resource_path;
};

// A helper class used to manipulate the Windows registry.  Typically, members
// return Windows last-error codes a la the Win32 registry API.
class RegKey {
 public:
  RegKey() : key_(NULL) { }
  ~RegKey() { Close(); }

  // Opens the key named |sub_key| with given |access| rights.  Returns
  // ERROR_SUCCESS or some other error.
  LONG Open(HKEY key, const wchar_t* sub_key, REGSAM access);

  // Returns true if a key is open.
  bool is_valid() const { return key_ != NULL; }

  // Read a REG_SZ value from the registry into the memory indicated by |value|
  // (of |value_size| wchar_t units).  Returns ERROR_SUCCESS,
  // ERROR_FILE_NOT_FOUND, ERROR_MORE_DATA, or some other error.  |value| is
  // guaranteed to be null-terminated on success.
  LONG ReadValue(const wchar_t* value_name,
                 wchar_t* value,
                 size_t value_size) const;

  // Write a REG_SZ value to the registry.  |value| must be null-terminated.
  // Returns ERROR_SUCCESS or an error code.
  LONG WriteValue(const wchar_t* value_name, const wchar_t* value);

  // Closes the key if it was open.
  void Close();

 private:
  RegKey(const RegKey&);
  RegKey& operator=(const RegKey&);

  HKEY key_;
};  // class RegKey

LONG RegKey::Open(HKEY key, const wchar_t* sub_key, REGSAM access) {
  Close();
  return ::RegOpenKeyEx(key, sub_key, NULL, access, &key_);
}

LONG RegKey::ReadValue(const wchar_t* value_name,
                       wchar_t* value,
                       size_t value_size) const {
  DWORD type;
  DWORD byte_length = static_cast<DWORD>(value_size * sizeof(wchar_t));
  LONG result = ::RegQueryValueEx(key_, value_name, NULL, &type,
                                  reinterpret_cast<BYTE*>(value),
                                  &byte_length);
  if (result == ERROR_SUCCESS) {
    if (type != REG_SZ) {
      result = ERROR_NOT_SUPPORTED;
    } else if (byte_length == 0) {
      *value = L'\0';
    } else if (value[byte_length/sizeof(wchar_t) - 1] != L'\0') {
      if ((byte_length / sizeof(wchar_t)) < value_size)
        value[byte_length / sizeof(wchar_t)] = L'\0';
      else
        result = ERROR_MORE_DATA;
    }
  }
  return result;
}

LONG RegKey::WriteValue(const wchar_t* value_name, const wchar_t* value) {
  return ::RegSetValueEx(key_, value_name, 0, REG_SZ,
                         reinterpret_cast<const BYTE*>(value),
                         (lstrlen(value) + 1) * sizeof(wchar_t));
}

void RegKey::Close() {
  if (key_ != NULL) {
    ::RegCloseKey(key_);
    key_ = NULL;
  }
}

// Helper function to read a value from registry. Returns true if value
// is read successfully and stored in parameter value. Returns false otherwise.
// |size| is measured in wchar_t units.
bool ReadValueFromRegistry(HKEY root_key, const wchar_t *sub_key,
                           const wchar_t *value_name, wchar_t *value,
                           size_t size) {
  RegKey key;

  if (key.Open(root_key, sub_key, KEY_QUERY_VALUE) == ERROR_SUCCESS &&
      key.ReadValue(value_name, value, size) == ERROR_SUCCESS) {
    return true;
  }
  return false;
}

// Opens the Google Update ClientState key for a product.
bool OpenClientStateKey(HKEY root_key, const wchar_t* app_guid, REGSAM access,
                        RegKey* key) {
  PathString client_state_key;
  return client_state_key.assign(kClientStateKeyBase) &&
         client_state_key.append(app_guid) &&
         (key->Open(root_key,
                    client_state_key.get(),
                    access | KEY_WOW64_32KEY) == ERROR_SUCCESS);
}

// This function sets the flag in registry to indicate that Google Update
// should try full installer next time. If the current installer works, this
// flag is cleared by setup.exe at the end of install. The flag will by default
// be written to HKCU, but if --system-level is included in the command line,
// it will be written to HKLM instead.
// TODO(grt): Write a unit test for this that uses registry virtualization.
void SetInstallerFlags(const Configuration& configuration) {
  RegKey key;
  const REGSAM key_access = KEY_QUERY_VALUE | KEY_SET_VALUE;
  const HKEY root_key =
      configuration.is_system_level() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  // This is ignored if multi-install is true.
  const wchar_t* app_guid =
      configuration.has_chrome_frame() ?
          google_update::kChromeFrameAppGuid :
          configuration.chrome_app_guid();
  StackString<128> value;
  LONG ret;

  // When multi_install is true, we are potentially:
  // 1. Performing a multi-install of some product(s) on a clean machine.
  //    Neither the product(s) nor the multi-installer will have a ClientState
  //    key in the registry, so there is nothing to be done.
  // 2. Upgrading an existing multi-install.  The multi-installer will have a
  //    ClientState key in the registry.  Only it need be modified.
  // 3. Migrating a single-install into a multi-install.  The product will have
  //    a ClientState key in the registry.  Only it need be modified.
  // To handle all cases, we inspect the product's ClientState to see if it
  // exists and its "ap" value does not contain "-multi".  This is case 3, so we
  // modify the product's ClientState.  Otherwise, we check the
  // multi-installer's ClientState and modify it if it exists.
  if (configuration.is_multi_install()) {
    if (OpenClientStateKey(root_key, app_guid, key_access, &key)) {
      // The product has a client state key.  See if it's a single-install.
      ret = key.ReadValue(kApRegistryValue, value.get(), value.capacity());
      if (ret != ERROR_FILE_NOT_FOUND &&
          (ret != ERROR_SUCCESS ||
           FindTagInStr(value.get(), kMultiInstallTag, NULL))) {
        // Error or case 2: modify the multi-installer's value.
        key.Close();
        app_guid = google_update::kMultiInstallAppGuid;
      }  // else case 3: modify this value.
    } else {
      // case 1 or 2: modify the multi-installer's value.
      key.Close();
      app_guid = google_update::kMultiInstallAppGuid;
    }
  }

  if (!key.is_valid()) {
    if (!OpenClientStateKey(root_key, app_guid, key_access, &key))
      return;

    value.clear();
    ret = key.ReadValue(kApRegistryValue, value.get(), value.capacity());
  }

  // The conditions below are handling two cases:
  // 1. When ap value is present, we want to add the required tag only if it is
  //    not present.
  // 2. When ap value is missing, we are going to create it with the required
  //    tag.
  if ((ret == ERROR_SUCCESS) || (ret == ERROR_FILE_NOT_FOUND)) {
    if (ret == ERROR_FILE_NOT_FOUND)
      value.clear();

    if (!StrEndsWith(value.get(), kFullInstallerSuffix) &&
        value.append(kFullInstallerSuffix)) {
      key.WriteValue(kApRegistryValue, value.get());
    }
  }
}

// Gets the setup.exe path from Registry by looking the value of Uninstall
// string.  |size| is measured in wchar_t units.
bool GetSetupExePathForGuidFromRegistry(bool system_level,
                                        const wchar_t* app_guid,
                                        wchar_t* path,
                                        size_t size) {
  const HKEY root_key = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey key;
  return OpenClientStateKey(root_key, app_guid, KEY_QUERY_VALUE, &key) &&
      (key.ReadValue(kUninstallRegistryValue, path, size) == ERROR_SUCCESS);
}

// Gets the setup.exe path from Registry by looking the value of Uninstall
// string.  |size| is measured in wchar_t units.
bool GetSetupExePathFromRegistry(const Configuration& configuration,
                                 wchar_t* path,
                                 size_t size) {
  bool system_level = configuration.is_system_level();

  // If this is a multi install, first try looking in the binaries for the path.
  if (configuration.is_multi_install() && GetSetupExePathForGuidFromRegistry(
          system_level, google_update::kMultiInstallAppGuid, path, size)) {
    return true;
  }

  // Failing that, look in Chrome Frame's client state key if --chrome-frame was
  // specified.
  if (configuration.has_chrome_frame() && GetSetupExePathForGuidFromRegistry(
          system_level, google_update::kChromeFrameAppGuid, path, size)) {
    return true;
  }

  // Make a last-ditch effort to look in the Chrome and App Host client state
  // keys.
  if (GetSetupExePathForGuidFromRegistry(
          system_level, configuration.chrome_app_guid(), path, size)) {
    return true;
  }
  if (configuration.has_app_host() && GetSetupExePathForGuidFromRegistry(
          system_level, google_update::kChromeAppHostAppGuid, path, size)) {
    return true;
  }

  return false;
}

// Calls CreateProcess with good default parameters and waits for the process to
// terminate returning the process exit code. |exit_code|, if non-NULL, is
// populated with the process exit code.
bool RunProcessAndWait(const wchar_t* exe_path, wchar_t* cmdline,
                       ProcessExitCode* exit_code) {
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  if (!::CreateProcess(exe_path, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
    return false;
  }

  ::CloseHandle(pi.hThread);

  bool ret = true;
  DWORD wr = ::WaitForSingleObject(pi.hProcess, INFINITE);
  if (WAIT_OBJECT_0 != wr) {
    ret = false;
  } else if (exit_code) {
    if (!::GetExitCodeProcess(pi.hProcess, exit_code))
      ret = false;
  }

  ::CloseHandle(pi.hProcess);

  return ret;
}

// Append any command line params passed to mini_installer to the given buffer
// so that they can be passed on to setup.exe. We do not return any error from
// this method and simply skip making any changes in case of error.
void AppendCommandLineFlags(const Configuration& configuration,
                            CommandString* buffer) {
  PathString full_exe_path;
  size_t len = ::GetModuleFileName(NULL, full_exe_path.get(),
                                   full_exe_path.capacity());
  if (!len || len >= full_exe_path.capacity())
    return;

  const wchar_t* exe_name = GetNameFromPathExt(full_exe_path.get(), len);
  if (exe_name == NULL)
    return;

  const wchar_t* cmd_to_append = L"";
  if (!StrEndsWith(configuration.program(), exe_name)) {
    // Current executable name not in the command line so just append
    // the whole command line.
    cmd_to_append = configuration.command_line();
  } else if (configuration.argument_count() > 1) {
    const wchar_t* tmp = SearchStringI(configuration.command_line(), exe_name);
    tmp = SearchStringI(tmp, L" ");
    cmd_to_append = tmp;
  }

  buffer->append(cmd_to_append);
}


// Windows defined callback used in the EnumResourceNames call. For each
// matching resource found, the callback is invoked and at this point we write
// it to disk. We expect resource names to start with 'chrome' or 'setup'. Any
// other name is treated as an error.
BOOL CALLBACK OnResourceFound(HMODULE module, const wchar_t* type,
                              wchar_t* name, LONG_PTR context) {
  if (NULL == context)
    return FALSE;

  Context* ctx = reinterpret_cast<Context*>(context);

  PEResource resource(name, type, module);
  if ((!resource.IsValid()) ||
      (resource.Size() < 1) ||
      (resource.Size() > kMaxResourceSize)) {
    return FALSE;
  }

  PathString full_path;
  if (!full_path.assign(ctx->base_path) ||
      !full_path.append(name) ||
      !resource.WriteToDisk(full_path.get()))
    return FALSE;

  if (StrStartsWith(name, kChromeArchivePrefix)) {
    if (!ctx->chrome_resource_path->assign(full_path.get()))
      return FALSE;
  } else if (StrStartsWith(name, kSetupPrefix)) {
    if (!ctx->setup_resource_path->assign(full_path.get()))
      return FALSE;
  } else {
    // Resources should either start with 'chrome' or 'setup'. We don't handle
    // anything else.
    return FALSE;
  }

  return TRUE;
}

// Finds and writes to disk resources of various types. Returns false
// if there is a problem in writing any resource to disk. setup.exe resource
// can come in one of three possible forms:
// - Resource type 'B7', compressed using LZMA (*.7z)
// - Resource type 'BL', compressed using LZ (*.ex_)
// - Resource type 'BN', uncompressed (*.exe)
// If setup.exe is present in more than one form, the precedence order is
// BN < BL < B7
// For more details see chrome/tools/build/win/create_installer_archive.py.
bool UnpackBinaryResources(const Configuration& configuration, HMODULE module,
                           const wchar_t* base_path, PathString* archive_path,
                           PathString* setup_path) {
  // Generate the setup.exe path where we patch/uncompress setup resource.
  PathString setup_dest_path;
  if (!setup_dest_path.assign(base_path) ||
      !setup_dest_path.append(kSetupExe))
    return false;

  // Prepare the input to OnResourceFound method that needs a location where
  // it will write all the resources.
  Context context = {
    base_path,
    archive_path,
    setup_path,
  };

  // Get the resources of type 'B7' (7zip archive).
  // We need a chrome archive to do the installation. So if there
  // is a problem in fetching B7 resource, just return an error.
  if (!::EnumResourceNames(module, kLZMAResourceType, OnResourceFound,
                           reinterpret_cast<LONG_PTR>(&context)) ||
      archive_path->length() == 0)
    return false;

  // If we found setup 'B7' resource, handle it.
  if (setup_path->length() > 0) {
    CommandString cmd_line;
    // Get the path to setup.exe first.
    bool success = true;
    if (!GetSetupExePathFromRegistry(configuration, cmd_line.get(),
                                     cmd_line.capacity()) ||
        !cmd_line.append(L" --") ||
        !cmd_line.append(kCmdUpdateSetupExe) ||
        !cmd_line.append(L"=\"") ||
        !cmd_line.append(setup_path->get()) ||
        !cmd_line.append(L"\" --") ||
        !cmd_line.append(kCmdNewSetupExe) ||
        !cmd_line.append(L"=\"") ||
        !cmd_line.append(setup_dest_path.get()) ||
        !cmd_line.append(L"\"")) {
      success = false;
    }

    // Get any command line option specified for mini_installer and pass them
    // on to setup.exe.  This is important since switches such as
    // --multi-install and --chrome-frame affect where setup.exe will write
    // installer results for consumption by Google Update.
    AppendCommandLineFlags(configuration, &cmd_line);

    ProcessExitCode exit_code = SUCCESS_EXIT_CODE;
    if (success &&
        (!RunProcessAndWait(NULL, cmd_line.get(), &exit_code) ||
         exit_code != SUCCESS_EXIT_CODE)) {
      success = false;
    }

    if (!success)
      DeleteFile(setup_path->get());

    return success && setup_path->assign(setup_dest_path.get());
  }

  // setup.exe wasn't sent as 'B7', lets see if it was sent as 'BL'
  // (compressed setup).
  if (!::EnumResourceNames(module, kLZCResourceType, OnResourceFound,
                           reinterpret_cast<LONG_PTR>(&context)) &&
      ::GetLastError() != ERROR_RESOURCE_TYPE_NOT_FOUND)
    return false;

  if (setup_path->length() > 0) {
    // Uncompress LZ compressed resource. Setup is packed with 'MSCF'
    // as opposed to old DOS way of 'SZDD'. Hence we don't use LZCopy.
    bool success = mini_installer::Expand(setup_path->get(),
                                          setup_dest_path.get());
    ::DeleteFile(setup_path->get());
    if (success) {
      if (!setup_path->assign(setup_dest_path.get())) {
        ::DeleteFile(setup_dest_path.get());
        success = false;
      }
    }

    return success;
  }

  // setup.exe still not found. So finally check if it was sent as 'BN'
  // (uncompressed setup).
  // TODO(tommi): We don't need BN anymore so let's remove it (and remove
  // it from create_installer_archive.py).
  if (!::EnumResourceNames(module, kBinResourceType, OnResourceFound,
                           reinterpret_cast<LONG_PTR>(&context)) &&
      ::GetLastError() != ERROR_RESOURCE_TYPE_NOT_FOUND)
    return false;

  if (setup_path->length() > 0) {
    if (setup_path->comparei(setup_dest_path.get()) != 0) {
      if (!::MoveFileEx(setup_path->get(), setup_dest_path.get(),
                        MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)) {
        ::DeleteFile(setup_path->get());
        setup_path->clear();
      } else if (!setup_path->assign(setup_dest_path.get())) {
        ::DeleteFile(setup_dest_path.get());
      }
    }
  }

  return setup_path->length() > 0;
}

// Executes setup.exe, waits for it to finish and returns the exit code.
bool RunSetup(const Configuration& configuration, const wchar_t* archive_path,
              const wchar_t* setup_path, ProcessExitCode* exit_code) {
  // There could be three full paths in the command line for setup.exe (path
  // to exe itself, path to archive and path to log file), so we declare
  // total size as three + one additional to hold command line options.
  CommandString cmd_line;

  // Get the path to setup.exe first.
  if (::lstrlen(setup_path) > 0) {
    if (!cmd_line.assign(L"\"") ||
        !cmd_line.append(setup_path) ||
        !cmd_line.append(L"\""))
      return false;
  } else if (!GetSetupExePathFromRegistry(configuration, cmd_line.get(),
                                          cmd_line.capacity())) {
    return false;
  }

  // Append the command line param for chrome archive file
  if (!cmd_line.append(L" --") ||
      !cmd_line.append(kCmdInstallArchive) ||
      !cmd_line.append(L"=\"") ||
      !cmd_line.append(archive_path) ||
      !cmd_line.append(L"\""))
    return false;

  // Get any command line option specified for mini_installer and pass them
  // on to setup.exe
  AppendCommandLineFlags(configuration, &cmd_line);

  return RunProcessAndWait(NULL, cmd_line.get(), exit_code);
}

// Deletes given files and working dir.
void DeleteExtractedFiles(const wchar_t* base_path,
                          const wchar_t* archive_path,
                          const wchar_t* setup_path) {
  ::DeleteFile(archive_path);
  ::DeleteFile(setup_path);
  // Delete the temp dir (if it is empty, otherwise fail).
  ::RemoveDirectory(base_path);
}

// Creates a temporary directory under |base_path| and returns the full path
// of created directory in |work_dir|. If successful return true, otherwise
// false.  When successful, the returned |work_dir| will always have a trailing
// backslash and this function requires that |base_path| always includes a
// trailing backslash as well.
// We do not use GetTempFileName here to avoid running into AV software that
// might hold on to the temp file as soon as we create it and then we can't
// delete it and create a directory in its place.  So, we use our own mechanism
// for creating a directory with a hopefully-unique name.  In the case of a
// collision, we retry a few times with a new name before failing.
bool CreateWorkDir(const wchar_t* base_path, PathString* work_dir) {
  if (!work_dir->assign(base_path) || !work_dir->append(kTempPrefix))
    return false;

  // Store the location where we'll append the id.
  size_t end = work_dir->length();

  // Check if we'll have enough buffer space to continue.
  // The name of the directory will use up 11 chars and then we need to append
  // the trailing backslash and a terminator.  We've already added the prefix
  // to the buffer, so let's just make sure we've got enough space for the rest.
  if ((work_dir->capacity() - end) < (arraysize("fffff.tmp") + 1))
    return false;

  // Generate a unique id.  We only use the lowest 20 bits, so take the top
  // 12 bits and xor them with the lower bits.
  DWORD id = ::GetTickCount();
  id ^= (id >> 12);

  int max_attempts = 10;
  while (max_attempts--) {
    // This converts 'id' to a string in the format "78563412" on windows
    // because of little endianness, but we don't care since it's just
    // a name.
    if (!HexEncode(&id, sizeof(id), work_dir->get() + end,
                   work_dir->capacity() - end)) {
      return false;
    }

    // We only want the first 5 digits to remain within the 8.3 file name
    // format (compliant with previous implementation).
    work_dir->truncate_at(end + 5);

    // for consistency with the previous implementation which relied on
    // GetTempFileName, we append the .tmp extension.
    work_dir->append(L".tmp");
    if (::CreateDirectory(work_dir->get(), NULL)) {
      // Yay!  Now let's just append the backslash and we're done.
      return work_dir->append(L"\\");
    }
    ++id;  // Try a different name.
  }

  return false;
}

// Creates and returns a temporary directory that can be used to extract
// mini_installer payload.
bool GetWorkDir(HMODULE module, PathString* work_dir) {
  PathString base_path;
  DWORD len = ::GetTempPath(base_path.capacity(), base_path.get());
  if (!len || len >= base_path.capacity() ||
      !CreateWorkDir(base_path.get(), work_dir)) {
    // Problem creating the work dir under TEMP path, so try using the
    // current directory as the base path.
    len = ::GetModuleFileName(module, base_path.get(), base_path.capacity());
    if (len >= base_path.capacity() || !len)
      return false;  // Can't even get current directory? Return an error.

    wchar_t* name = GetNameFromPathExt(base_path.get(), len);
    if (!name)
      return false;

    *name = L'\0';

    return CreateWorkDir(base_path.get(), work_dir);
  }
  return true;
}

// Returns true for ".." and "." directories.
bool IsCurrentOrParentDirectory(const wchar_t* dir) {
  return dir &&
         dir[0] == L'.' &&
         (dir[1] == L'\0' || (dir[1] == L'.' && dir[2] == L'\0'));
}

// Best effort directory tree deletion including the directory specified
// by |path|, which must not end in a separator.
// The |path| argument is writable so that each recursion can use the same
// buffer as was originally allocated for the path.  The path will be unchanged
// upon return.
void RecursivelyDeleteDirectory(PathString* path) {
  // |path| will never have a trailing backslash.
  size_t end = path->length();
  if (!path->append(L"\\*.*"))
    return;

  WIN32_FIND_DATA find_data = {0};
  HANDLE find = ::FindFirstFile(path->get(), &find_data);
  if (find != INVALID_HANDLE_VALUE) {
    do {
      // Use the short name if available to make the most of our buffer.
      const wchar_t* name = find_data.cAlternateFileName[0] ?
          find_data.cAlternateFileName : find_data.cFileName;
      if (IsCurrentOrParentDirectory(name))
        continue;

      path->truncate_at(end + 1);  // Keep the trailing backslash.
      if (!path->append(name))
        continue;  // Continue in spite of too long names.

      if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        RecursivelyDeleteDirectory(path);
      } else {
        ::DeleteFile(path->get());
      }
    } while (::FindNextFile(find, &find_data));
    ::FindClose(find);
  }

  // Restore the path and delete the directory before we return.
  path->truncate_at(end);
  ::RemoveDirectory(path->get());
}

// Enumerates subdirectories of |parent_dir| and deletes all subdirectories
// that match with a given |prefix|.  |parent_dir| must have a trailing
// backslash.
// The process is done on a best effort basis, so conceivably there might
// still be matches left when the function returns.
void DeleteDirectoriesWithPrefix(const wchar_t* parent_dir,
                                 const wchar_t* prefix) {
  // |parent_dir| is guaranteed to always have a trailing backslash.
  PathString spec;
  if (!spec.assign(parent_dir) || !spec.append(prefix) || !spec.append(L"*.*"))
    return;

  WIN32_FIND_DATA find_data = {0};
  HANDLE find = ::FindFirstFileEx(spec.get(), FindExInfoStandard, &find_data,
                                  FindExSearchLimitToDirectories, NULL, 0);
  if (find == INVALID_HANDLE_VALUE)
    return;

  PathString path;
  do {
    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      // Use the short name if available to make the most of our buffer.
      const wchar_t* name = find_data.cAlternateFileName[0] ?
          find_data.cAlternateFileName : find_data.cFileName;
      if (IsCurrentOrParentDirectory(name))
        continue;
      if (path.assign(parent_dir) && path.append(name))
        RecursivelyDeleteDirectory(&path);
    }
  } while (::FindNextFile(find, &find_data));
  ::FindClose(find);
}

// Attempts to free up space by deleting temp directories that previous
// installer runs have failed to clean up.
void DeleteOldChromeTempDirectories() {
  static const wchar_t* const kDirectoryPrefixes[] = {
    kTempPrefix,
    L"chrome_"  // Previous installers created directories with this prefix
                // and there are still some lying around.
  };

  PathString temp;
  // GetTempPath always returns a path with a trailing backslash.
  DWORD len = ::GetTempPath(temp.capacity(), temp.get());
  // GetTempPath returns 0 or number of chars copied, not including the
  // terminating '\0'.
  if (!len || len >= temp.capacity())
    return;

  for (int i = 0; i < arraysize(kDirectoryPrefixes); ++i) {
    DeleteDirectoriesWithPrefix(temp.get(), kDirectoryPrefixes[i]);
  }
}

// Checks the command line for specific mini installer flags.
// If the function returns true, the command line has been processed and all
// required actions taken.  The installer must exit and return the returned
// |exit_code|.
bool ProcessNonInstallOperations(const Configuration& configuration,
                                 ProcessExitCode* exit_code) {
  bool ret = false;

  switch (configuration.operation()) {
    case Configuration::CLEANUP:
      // Cleanup has already taken place in DeleteOldChromeTempDirectories at
      // this point, so just tell our caller to exit early.
      *exit_code = SUCCESS_EXIT_CODE;
      ret = true;
      break;

    default: break;
  }

  return ret;
}

// Returns true if we should delete the temp files we create (default).
// Returns false iff the user has manually created a ChromeInstallerCleanup
// string value in the registry under HKCU\\Software\\[Google|Chromium]
// and set its value to "0".  That explicitly forbids the mini installer from
// deleting these files.
// Support for this has been publicly mentioned in troubleshooting tips so
// we continue to support it.
bool ShouldDeleteExtractedFiles() {
  wchar_t value[2] = {0};
  if (ReadValueFromRegistry(HKEY_CURRENT_USER, kCleanupRegistryKey,
                            kCleanupRegistryValue, value, arraysize(value)) &&
      value[0] == L'0') {
    return false;
  }

  return true;
}

// Main function. First gets a working dir, unpacks the resources and finally
// executes setup.exe to do the install/upgrade.
ProcessExitCode WMain(HMODULE module) {
#if defined(COMPONENT_BUILD)
  if (::GetEnvironmentVariable(L"MINI_INSTALLER_TEST", NULL, 0) == 0) {
    static const wchar_t kComponentBuildIncompatibleMessage[] =
        L"mini_installer.exe is incompatible with the component build, please"
        L" run setup.exe with the same command line instead. See"
        L" http://crbug.com/127233#c17 for details.";
    ::MessageBox(NULL, kComponentBuildIncompatibleMessage, NULL, MB_ICONERROR);
    return GENERIC_ERROR;
  }
#endif

  // Always start with deleting potential leftovers from previous installations.
  // This can make the difference between success and failure.  We've seen
  // many installations out in the field fail due to out of disk space problems
  // so this could buy us some space.
  DeleteOldChromeTempDirectories();

  // TODO(grt): Make the exit codes more granular so we know where the popular
  // errors truly are.
  ProcessExitCode exit_code = GENERIC_INITIALIZATION_FAILURE;

  // Parse the command line.
  Configuration configuration;
  if (!configuration.Initialize())
    return exit_code;

  if (configuration.query_component_build()) {
    // Exit immediately with a generic success exit code (0) to indicate
    // component build and a generic failure exit code (1) to indicate static
    // build. This is used by the tests in /src/chrome/test/mini_installer/.
#if defined(COMPONENT_BUILD)
    return SUCCESS_EXIT_CODE;
#else
    return GENERIC_ERROR;
#endif
  }

  // If the --cleanup switch was specified on the command line, then that means
  // we should only do the cleanup and then exit.
  if (ProcessNonInstallOperations(configuration, &exit_code))
    return exit_code;

  // First get a path where we can extract payload
  PathString base_path;
  if (!GetWorkDir(module, &base_path))
    return GENERIC_INITIALIZATION_FAILURE;

#if defined(GOOGLE_CHROME_BUILD)
  // Set the magic suffix in registry to try full installer next time. We ignore
  // any errors here and we try to set the suffix for user level unless
  // --system-level is on the command line in which case we set it for system
  // level instead. This only applies to the Google Chrome distribution.
  SetInstallerFlags(configuration);
#endif

  PathString archive_path;
  PathString setup_path;
  if (!UnpackBinaryResources(configuration, module, base_path.get(),
                             &archive_path, &setup_path)) {
    exit_code = GENERIC_UNPACKING_FAILURE;
  } else {
    // While unpacking the binaries, we paged in a whole bunch of memory that
    // we don't need anymore.  Let's give it back to the pool before running
    // setup.
    ::SetProcessWorkingSetSize(::GetCurrentProcess(), -1, -1);
    if (!RunSetup(configuration, archive_path.get(), setup_path.get(),
                  &exit_code)) {
      exit_code = GENERIC_SETUP_FAILURE;
    }
  }

  if (ShouldDeleteExtractedFiles())
    DeleteExtractedFiles(base_path.get(), archive_path.get(), setup_path.get());

  return exit_code;
}

}  // namespace mini_installer

int MainEntryPoint() {
  mini_installer::ProcessExitCode result =
      mini_installer::WMain(::GetModuleHandle(NULL));
  ::ExitProcess(result);
}

// VC Express editions don't come with the memset CRT obj file and linking to
// the obj files between versions becomes a bit problematic. Therefore,
// simply implement memset.
//
// This also avoids having to explicitly set the __sse2_available hack when
// linking with both the x64 and x86 obj files which is required when not
// linking with the std C lib in certain instances (including Chromium) with
// MSVC.  __sse2_available determines whether to use SSE2 intructions with
// std C lib routines, and is set by MSVC's std C lib implementation normally.
extern "C" {
#pragma function(memset)
void* memset(void* dest, int c, size_t count) {
  void* start = dest;
  while (count--) {
    *reinterpret_cast<char*>(dest) = static_cast<char>(c);
    dest = reinterpret_cast<char*>(dest) + 1;
  }
  return start;
}
}  // extern "C"
