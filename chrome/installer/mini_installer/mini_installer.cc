// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// mini_installer.exe is the first exe that is run when chrome is being
// installed or upgraded. It is designed to be extremely small (~5KB with no
// extra resources linked) and it has two main jobs:
//   1) unpack the resources (possibly decompressing some)
//   2) run the real installer (setup.exe) with appropiate flags.
//
// In order to be really small we don't link against the CRT and we define the
// following compiler/linker flags:
//   EnableIntrinsicFunctions="true" compiler: /Oi
//   BasicRuntimeChecks="0"
//   BufferSecurityCheck="false" compiler: /GS-
//   EntryPointSymbol="MainEntryPoint" linker: /ENTRY
//   IgnoreAllDefaultLibraries="true" linker: /NODEFAULTLIB
//   OptimizeForWindows98="1"  liker: /OPT:NOWIN98
//   linker: /SAFESEH:NO
// Also some built-in code that the compiler relies on is not defined so we
// are forced to manually link against it. It comes in the form of two
// object files that exist in $(VCInstallDir)\crt\src which are memset.obj and
// P4_memset.obj. These two object files rely on the existence of a static
// variable named __sse2_available which indicates the presence of intel sse2
// extensions. We define it to false which causes a slower but safe code for
// memcpy and memset intrinsics.

// having the linker merge the sections is saving us ~500 bytes.
#pragma comment(linker, "/MERGE:.rdata=.text")

#include <windows.h>
#include <setupapi.h>
#include <shellapi.h>

#include "chrome/installer/mini_installer/appid.h"
#include "chrome/installer/mini_installer/mini_installer.h"
#include "chrome/installer/mini_installer/mini_string.h"
#include "chrome/installer/mini_installer/pe_resource.h"

// arraysize borrowed from basictypes.h
template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

// Required linker symbol. See remarks above.
extern "C" unsigned int __sse2_available = 0;

namespace mini_installer {

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
  return client_state_key.assign(kApRegistryKeyBase) &&
         client_state_key.append(app_guid) &&
         (key->Open(root_key, client_state_key.get(), access) == ERROR_SUCCESS);
}

// TODO(grt): Write a unit test for this that uses registry virtualization.
void SetInstallerFlagsHelper(int args_num, const wchar_t* const* args) {
  bool multi_install = false;
  RegKey key;
  const REGSAM key_access = KEY_QUERY_VALUE | KEY_SET_VALUE;
  const wchar_t* app_guid = google_update::kAppGuid;
  HKEY root_key = HKEY_CURRENT_USER;
  StackString<128> value;
  LONG ret;

  for (int i = 1; i < args_num; ++i) {
    if (0 == ::lstrcmpi(args[i], L"--chrome-sxs"))
      app_guid = google_update::kSxSAppGuid;
    else if (0 == ::lstrcmpi(args[i], L"--chrome-frame"))
      app_guid = google_update::kChromeFrameAppGuid;
    else if (0 == ::lstrcmpi(args[i], L"--multi-install"))
      multi_install = true;
    else if (0 == ::lstrcmpi(args[i], L"--system-level"))
      root_key = HKEY_LOCAL_MACHINE;
  }

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
  if (multi_install) {
    if (OpenClientStateKey(root_key, app_guid, key_access, &key)) {
      // The app is installed.  See if it's a single-install.
      ret = key.ReadValue(kApRegistryValueName, value.get(), value.capacity());
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
    ret = key.ReadValue(kApRegistryValueName, value.get(), value.capacity());
  }

  // The conditions below are handling two cases:
  // 1. When ap key is present, we want to add the required tags only if they
  //    are not present.
  // 2. When ap key is missing, we are going to create it with the required
  //    tags.
  if ((ret == ERROR_SUCCESS) || (ret == ERROR_FILE_NOT_FOUND)) {
    if (ret == ERROR_FILE_NOT_FOUND)
      value.clear();

    bool success = true;

    if (multi_install &&
        !FindTagInStr(value.get(), kMultifailInstallerSuffix, NULL)) {
      // We want -multifail to immediately precede -full.  Chop off the latter
      // if it's already present so that we can simply do two appends.
      if (StrEndsWith(value.get(), kFullInstallerSuffix)) {
        size_t suffix_len = (arraysize(kFullInstallerSuffix) - 1);
        size_t value_len = value.length();
        value.truncate_at(value_len - suffix_len);
      }
      success = value.append(kMultifailInstallerSuffix);
    }
    if (success && !StrEndsWith(value.get(), kFullInstallerSuffix) &&
        (value.append(kFullInstallerSuffix))) {
      key.WriteValue(kApRegistryValueName, value.get());
    }
  }
}

// This function sets the flag in registry to indicate that Google Update
// should try full installer next time. If the current installer works, this
// flag is cleared by setup.exe at the end of install. The flag will by default
// be written to HKCU, but if --system-level is included in the command line,
// it will be written to HKLM instead.
void SetInstallerFlags() {
  int args_num;
  wchar_t* cmd_line = ::GetCommandLine();
  wchar_t** args = ::CommandLineToArgvW(cmd_line, &args_num);

  SetInstallerFlagsHelper(args_num, args);

  ::LocalFree(args);
}

// Gets the setup.exe path from Registry by looking the value of Uninstall
// string, strips the arguments for uninstall and returns only the full path
// to setup.exe.  |size| is measured in wchar_t units.
bool GetSetupExePathFromRegistry(wchar_t* path, size_t size) {
  if (!ReadValueFromRegistry(HKEY_CURRENT_USER, kUninstallRegistryKey,
      kUninstallRegistryValueName, path, size)) {
    if (!ReadValueFromRegistry(HKEY_LOCAL_MACHINE, kUninstallRegistryKey,
        kUninstallRegistryValueName, path, size)) {
      return false;
    }
  }
  wchar_t* tmp = const_cast<wchar_t*>(SearchStringI(path, L" --"));
  if (tmp) {
    *tmp = L'\0';
  } else {
    return false;
  }

  return true;
}

// Calls CreateProcess with good default parameters and waits for the process
// to terminate returning the process exit code.
bool RunProcessAndWait(const wchar_t* exe_path, wchar_t* cmdline,
                       int* exit_code) {
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
    if (!::GetExitCodeProcess(pi.hProcess,
                              reinterpret_cast<DWORD*>(exit_code))) {
      ret = false;
    }
  }

  ::CloseHandle(pi.hProcess);

  return ret;
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

  if (StrStartsWith(name, kChromePrefix)) {
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
bool UnpackBinaryResources(HMODULE module, const wchar_t* base_path,
                           PathString* archive_path, PathString* setup_path) {
  // Generate the setup.exe path where we patch/uncompress setup resource.
  PathString setup_dest_path;
  if (!setup_dest_path.assign(base_path) ||
      !setup_dest_path.append(kSetupName))
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
    if (!GetSetupExePathFromRegistry(cmd_line.get(), cmd_line.capacity()) ||
        !cmd_line.append(kCmdUpdateSetupExe) ||
        !cmd_line.append(L"=\"") ||
        !cmd_line.append(setup_path->get()) ||
        !cmd_line.append(L"\"") ||
        !cmd_line.append(kCmdNewSetupExe) ||
        !cmd_line.append(L"=\"") ||
        !cmd_line.append(setup_dest_path.get()) ||
        !cmd_line.append(L"\"")) {
      success = false;
    }

    int exit_code = 0;
    if (success &&
        (!RunProcessAndWait(NULL, cmd_line.get(), &exit_code) ||
         exit_code != ERROR_SUCCESS)) {
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
    // as opposed to old DOS way of 'SZDD'. Hence use SetupInstallFile
    // instead of LZCopy.
    // Note that the API will automatically delete the original file
    // if the extraction was successful.
    // TODO(tommi): Use the cabinet API directly.
    if (!SetupInstallFile(NULL, NULL, setup_path->get(), NULL,
                          setup_dest_path.get(),
                          SP_COPY_DELETESOURCE | SP_COPY_SOURCE_ABSOLUTE,
                          NULL, NULL)) {
      DeleteFile(setup_path->get());
      return false;
    }

    return setup_path->assign(setup_dest_path.get());
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

// Append any command line params passed to mini_installer to the given buffer
// so that they can be passed on to setup.exe. We do not return any error from
// this method and simply skip making any changes in case of error.
void AppendCommandLineFlags(CommandString* buffer) {
  PathString full_exe_path;
  size_t len = ::GetModuleFileName(NULL, full_exe_path.get(),
                                   full_exe_path.capacity());
  if (!len || len >= full_exe_path.capacity())
    return;

  const wchar_t* exe_name = GetNameFromPathExt(full_exe_path.get(), len);
  if (exe_name == NULL)
    return;

  int args_num;
  wchar_t* cmd_line = ::GetCommandLine();
  wchar_t** args = ::CommandLineToArgvW(cmd_line, &args_num);
  if (args_num <= 0)
    return;

  const wchar_t* cmd_to_append = L"";
  if (!StrEndsWith(args[0], exe_name)) {
    // Current executable name not in the command line so just append
    // the whole command line.
    cmd_to_append = cmd_line;
  } else if (args_num > 1) {
    const wchar_t* tmp = SearchStringI(cmd_line, exe_name);
    tmp = SearchStringI(tmp, L" ");
    cmd_to_append = tmp;
  }

  buffer->append(cmd_to_append);

  LocalFree(args);
}

// Executes setup.exe, waits for it to finish and returns the exit code.
bool RunSetup(const wchar_t* archive_path, const wchar_t* setup_path,
              int* exit_code) {
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
  } else if (!GetSetupExePathFromRegistry(cmd_line.get(),
                                          cmd_line.capacity())) {
    return false;
  }

  // Append the command line param for chrome archive file
  if (!cmd_line.append(kCmdInstallArchive) ||
      !cmd_line.append(L"=\"") ||
      !cmd_line.append(archive_path) ||
      !cmd_line.append(L"\""))
    return false;

  // Get any command line option specified for mini_installer and pass them
  // on to setup.exe
  AppendCommandLineFlags(&cmd_line);

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
bool ProcessMiniInstallerCommandLine(int* exit_code) {
  bool ret = false;
  const wchar_t* cmd_line = ::GetCommandLineW();
  int num_args = 0;
  wchar_t** args = ::CommandLineToArgvW(cmd_line, &num_args);
  if (args) {
    for (int i = 1; i < num_args && !ret; ++i) {
      // Currently there's only one mini installer specific switch defined.
      if (lstrcmpiW(args[i], kMiniCmdCleanup) == 0) {
        *exit_code = 0;
        ret = true;
      }
    }
    ::LocalFree(args);
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
                            kCleanupRegistryValueName, value,
                            arraysize(value)) &&
      value[0] == L'0') {
    return false;
  }

  return true;
}

// Main function. First gets a working dir, unpacks the resources and finally
// executes setup.exe to do the install/upgrade.
int WMain(HMODULE module) {
  // Always start with deleting potential leftovers from previous installations.
  // This can make the difference between success and failure.  We've seen
  // many installations out in the field fail due to out of disk space problems
  // so this could buy us some space.
  DeleteOldChromeTempDirectories();

  // If the --cleanup switch was specified on the command line, then that means
  // we should only do the cleanup and then exit.
  int exit_code = 101;
  if (ProcessMiniInstallerCommandLine(&exit_code))
    return exit_code;

  // First get a path where we can extract payload
  PathString base_path;
  if (!GetWorkDir(module, &base_path))
    return 101;

#if defined(GOOGLE_CHROME_BUILD)
  // Set the magic suffix in registry to try full installer next time. We ignore
  // any errors here and we try to set the suffix for user level unless
  // --system-level is on the command line in which case we set it for system
  // level instead. This only applies to the Google Chrome distribution.
  SetInstallerFlags();
#endif

  PathString archive_path;
  PathString setup_path;
  if (!UnpackBinaryResources(module, base_path.get(), &archive_path,
                             &setup_path)) {
    exit_code = 102;
  } else {
    // While unpacking the binaries, we paged in a whole bunch of memory that
    // we don't need anymore.  Let's give it back to the pool before running
    // setup.
    ::SetProcessWorkingSetSize(::GetCurrentProcess(), -1, -1);
    if (!RunSetup(archive_path.get(), setup_path.get(), &exit_code))
      exit_code = 103;
  }

  if (ShouldDeleteExtractedFiles())
    DeleteExtractedFiles(base_path.get(), archive_path.get(), setup_path.get());

  return exit_code;
}

}  // namespace mini_installer

int MainEntryPoint() {
  int result = mini_installer::WMain(::GetModuleHandle(NULL));
  ::ExitProcess(result);
}
