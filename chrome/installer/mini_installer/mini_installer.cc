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

// #define needed to link in RtlGenRandom(), a.k.a. SystemFunction036.  See the
// "Community Additions" comment on MSDN here:
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa387694.aspx
#define SystemFunction036 NTAPI SystemFunction036
#include <NTSecAPI.h>
#undef SystemFunction036

#include <sddl.h>
#include <shellapi.h>
#include <stdlib.h>
#include <stddef.h>

#include "chrome/installer/mini_installer/appid.h"
#include "chrome/installer/mini_installer/configuration.h"
#include "chrome/installer/mini_installer/decompress.h"
#include "chrome/installer/mini_installer/exit_code.h"
#include "chrome/installer/mini_installer/mini_installer_constants.h"
#include "chrome/installer/mini_installer/mini_string.h"
#include "chrome/installer/mini_installer/pe_resource.h"
#include "chrome/installer/mini_installer/regkey.h"

namespace mini_installer {

typedef StackString<MAX_PATH> PathString;
typedef StackString<MAX_PATH * 4> CommandString;

struct ProcessExitResult {
  DWORD exit_code;
  DWORD windows_error;

  explicit ProcessExitResult(DWORD exit) : exit_code(exit), windows_error(0) {}
  ProcessExitResult(DWORD exit, DWORD win)
      : exit_code(exit), windows_error(win) {
  }

  bool IsSuccess() {
    return exit_code == SUCCESS_EXIT_CODE;
  }
};

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


// Opens the Google Update ClientState key for the current install
// configuration.  This includes locating the correct key in the face of
// multi-install.  The flag will by default be written to HKCU, but if
// --system-level is included in the command line, it will be written to
// HKLM instead.
bool OpenInstallStateKey(const Configuration& configuration, RegKey* key) {
  const HKEY root_key =
      configuration.is_system_level() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  const wchar_t* app_guid = configuration.chrome_app_guid();
  const REGSAM key_access = KEY_QUERY_VALUE | KEY_SET_VALUE;

  return OpenClientStateKey(root_key, app_guid, key_access, key);
}

// Writes install results into registry where it is read by Google Update.
// Don't write anything if there is already a result present, likely
// written by setup.exe.
void WriteInstallResults(const Configuration& configuration,
                         ProcessExitResult result) {
#if defined(GOOGLE_CHROME_BUILD)
  // Calls to setup.exe will write a "success" result if everything was good
  // so we don't need to write anything from here.
  if (result.IsSuccess())
    return;

  RegKey key;
  DWORD value;
  if (OpenInstallStateKey(configuration, &key)) {
    if (key.ReadDWValue(kInstallerResultRegistryValue, &value)
            != ERROR_SUCCESS || value == 0) {
      key.WriteDWValue(kInstallerResultRegistryValue,
                       result.exit_code ? 1 /* FAILED_CUSTOM_ERROR */
                                        : 0 /* SUCCESS */);
      key.WriteDWValue(kInstallerErrorRegistryValue, result.exit_code);
      key.WriteDWValue(kInstallerExtraCode1RegistryValue, result.windows_error);
    }
    key.Close();
  }
#endif
}

// This function sets the flag in registry to indicate that Google Update
// should try full installer next time. If the current installer works, this
// flag is cleared by setup.exe at the end of install.
void SetInstallerFlags(const Configuration& configuration) {
  RegKey key;
  StackString<128> value;
  LONG ret = ERROR_SUCCESS;

  if (!OpenInstallStateKey(configuration, &key))
    return;

  ret = key.ReadSZValue(kApRegistryValue, value.get(), value.capacity());

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
      key.WriteSZValue(kApRegistryValue, value.get());
    }
  }
}

// Gets the setup.exe path from Registry by looking at the value of Uninstall
// string.  |size| is measured in wchar_t units.
ProcessExitResult GetSetupExePathForAppGuid(bool system_level,
                                          const wchar_t* app_guid,
                                          const wchar_t* previous_version,
                                          wchar_t* path,
                                          size_t size) {
  const HKEY root_key = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey key;
  if (!OpenClientStateKey(root_key, app_guid, KEY_QUERY_VALUE, &key))
    return ProcessExitResult(UNABLE_TO_FIND_REGISTRY_KEY);
  DWORD result = key.ReadSZValue(kUninstallRegistryValue, path, size);
  if (result != ERROR_SUCCESS)
    return ProcessExitResult(UNABLE_TO_FIND_REGISTRY_KEY, result);

  // Check that the path to the existing installer includes the expected
  // version number.  It's not necessary for accuracy to verify before/after
  // delimiters.
  if (!SearchStringI(path, previous_version))
    return ProcessExitResult(PATCH_NOT_FOR_INSTALLED_VERSION);

  return ProcessExitResult(SUCCESS_EXIT_CODE);
}

// Gets the path to setup.exe of the previous version. The overall path is found
// in the Uninstall string in the registry. A previous version number specified
// in |configuration| is used if available. |size| is measured in wchar_t units.
ProcessExitResult GetPreviousSetupExePath(const Configuration& configuration,
                                        wchar_t* path,
                                        size_t size) {
  bool system_level = configuration.is_system_level();
  const wchar_t* previous_version = configuration.previous_version();
  ProcessExitResult exit_code = ProcessExitResult(GENERIC_ERROR);

  // If this is a multi install, first try looking in the binaries for the path.
  if (configuration.is_multi_install()) {
    exit_code = GetSetupExePathForAppGuid(
        system_level, google_update::kMultiInstallAppGuid, previous_version,
        path, size);
  }

  // Failing that, look in Chrome Frame's client state key if --chrome-frame was
  // specified.
  if (!exit_code.IsSuccess() && configuration.has_chrome_frame()) {
    exit_code = GetSetupExePathForAppGuid(
        system_level, google_update::kChromeFrameAppGuid, previous_version,
        path, size);
  }

  // Make a last-ditch effort to look in the Chrome client state key.
  if (!exit_code.IsSuccess()) {
    exit_code = GetSetupExePathForAppGuid(
        system_level, configuration.chrome_app_guid(), previous_version,
        path, size);
  }

  return exit_code;
}

// Calls CreateProcess with good default parameters and waits for the process to
// terminate returning the process exit code. |exit_code|, if non-NULL, is
// populated with the process exit code.
ProcessExitResult RunProcessAndWait(const wchar_t* exe_path, wchar_t* cmdline) {
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  if (!::CreateProcess(exe_path, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
    return ProcessExitResult(COULD_NOT_CREATE_PROCESS, ::GetLastError());
  }

  ::CloseHandle(pi.hThread);

  DWORD exit_code = SUCCESS_EXIT_CODE;
  DWORD wr = ::WaitForSingleObject(pi.hProcess, INFINITE);
  if (WAIT_OBJECT_0 != wr || !::GetExitCodeProcess(pi.hProcess, &exit_code)) {
    // Note:  We've assumed that WAIT_OBJCT_0 != wr means a failure.  The call
    // could return a different object but since we never spawn more than one
    // sub-process at a time that case should never happen.
    return ProcessExitResult(WAIT_FOR_PROCESS_FAILED, ::GetLastError());
  }

  ::CloseHandle(pi.hProcess);

  return ProcessExitResult(exit_code);
}

// Appends any command line params passed to mini_installer to the given buffer
// so that they can be passed on to setup.exe.
// |buffer| is unchanged in case of error.
void AppendCommandLineFlags(const Configuration& configuration,
                            CommandString* buffer) {
  PathString full_exe_path;
  size_t len = ::GetModuleFileName(
      NULL, full_exe_path.get(), static_cast<DWORD>(full_exe_path.capacity()));
  if (!len || len >= full_exe_path.capacity())
    return;

  const wchar_t* exe_name =
      GetNameFromPathExt(full_exe_path.get(), static_cast<DWORD>(len));

  // - configuration.program() returns the first command line argument
  //   passed into the program (that the user probably typed in this case).
  //       "mini_installer.exe"
  //       "mini_installer"
  //       "out\Release\mini_installer"
  // - |exe_name| is the executable file of the current process.
  //       "mini_installer.exe"
  //
  // Note that there are three possibilities to handle here.
  // Receive a cmdline containing:
  // 1) executable name WITH extension
  // 2) executable name with NO extension
  // 3) NO executable name as part of cmdline
  const wchar_t* cmd_to_append = L"";
  const wchar_t* arg0 = configuration.program();
  if (!arg0)
    return;
  const wchar_t* arg0_base_name = GetNameFromPathExt(arg0, ::lstrlen(arg0));
  if (!StrStartsWith(exe_name, arg0_base_name)) {
    // State 3: NO executable name as part of cmdline.
    buffer->append(L" ");
    cmd_to_append = configuration.command_line();
  } else if (configuration.argument_count() > 1) {
    // State 1 or 2: Executable name is in cmdline.
    // - Append everything AFTER the executable name.
    //   (Using arg0_base_name here to make sure to match with or without
    //   extension.  Then move to the space following the token.)
    const wchar_t* tmp = SearchStringI(configuration.command_line(),
                                       arg0_base_name);
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
  if (!resource.IsValid() || resource.Size() < 1)
    return FALSE;

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

#if defined(COMPONENT_BUILD)
// An EnumResNameProc callback that writes the resource |name| to disk in the
// directory |base_path_ptr| (which must end with a path separator).
BOOL CALLBACK WriteResourceToDirectory(HMODULE module,
                                       const wchar_t* type,
                                       wchar_t* name,
                                       LONG_PTR base_path_ptr) {
  const wchar_t* base_path = reinterpret_cast<const wchar_t*>(base_path_ptr);
  PathString full_path;

  PEResource resource(name, type, module);
  return (resource.IsValid() &&
          full_path.assign(base_path) &&
          full_path.append(name) &&
          resource.WriteToDisk(full_path.get()));
}
#endif

// Finds and writes to disk resources of various types. Returns false
// if there is a problem in writing any resource to disk. setup.exe resource
// can come in one of three possible forms:
// - Resource type 'B7', compressed using LZMA (*.7z)
// - Resource type 'BL', compressed using LZ (*.ex_)
// - Resource type 'BN', uncompressed (*.exe)
// If setup.exe is present in more than one form, the precedence order is
// BN < BL < B7
// For more details see chrome/tools/build/win/create_installer_archive.py.
// For component builds (where setup.ex_ is always used), all files stored as
// uncompressed 'BN' resources are also extracted. This is generally the set of
// DLLs/resources needed by setup.exe to run.
ProcessExitResult UnpackBinaryResources(const Configuration& configuration,
                                      HMODULE module, const wchar_t* base_path,
                                      PathString* archive_path,
                                      PathString* setup_path) {
  // Generate the setup.exe path where we patch/uncompress setup resource.
  PathString setup_dest_path;
  if (!setup_dest_path.assign(base_path) ||
      !setup_dest_path.append(kSetupExe))
    return ProcessExitResult(PATH_STRING_OVERFLOW);

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
                           reinterpret_cast<LONG_PTR>(&context))) {
    return ProcessExitResult(UNABLE_TO_EXTRACT_CHROME_ARCHIVE,
                             ::GetLastError());
  }
  if (archive_path->length() == 0) {
    return ProcessExitResult(UNABLE_TO_EXTRACT_CHROME_ARCHIVE);
  }

  ProcessExitResult exit_code = ProcessExitResult(SUCCESS_EXIT_CODE);

  // If we found setup 'B7' resource (used for differential updates), handle
  // it.  Note that this is only for Chrome; Chromium installs are always
  // "full" installs.
  if (setup_path->length() > 0) {
    CommandString cmd_line;
    PathString exe_path;
    // Get the path to setup.exe first.
    exit_code = GetPreviousSetupExePath(configuration, exe_path.get(),
                                        exe_path.capacity());
    if (exit_code.IsSuccess()) {
      if (!cmd_line.append(exe_path.get()) ||
          !cmd_line.append(L" --") ||
          !cmd_line.append(kCmdUpdateSetupExe) ||
          !cmd_line.append(L"=\"") ||
          !cmd_line.append(setup_path->get()) ||
          !cmd_line.append(L"\" --") ||
          !cmd_line.append(kCmdNewSetupExe) ||
          !cmd_line.append(L"=\"") ||
          !cmd_line.append(setup_dest_path.get()) ||
          !cmd_line.append(L"\"")) {
        exit_code = ProcessExitResult(COMMAND_STRING_OVERFLOW);
      }
    }

    // Get any command line option specified for mini_installer and pass them
    // on to setup.exe.  This is important since switches such as
    // --multi-install and --chrome-frame affect where setup.exe will write
    // installer results for consumption by Google Update.
    AppendCommandLineFlags(configuration, &cmd_line);

    if (exit_code.IsSuccess())
      exit_code = RunProcessAndWait(exe_path.get(), cmd_line.get());

    if (!exit_code.IsSuccess())
      DeleteFile(setup_path->get());
    else if (!setup_path->assign(setup_dest_path.get()))
      exit_code = ProcessExitResult(PATH_STRING_OVERFLOW);

    return exit_code;
  }

  // setup.exe wasn't sent as 'B7', lets see if it was sent as 'BL'
  // (compressed setup).
  if (!::EnumResourceNames(module, kLZCResourceType, OnResourceFound,
                           reinterpret_cast<LONG_PTR>(&context))) {
    return ProcessExitResult(UNABLE_TO_EXTRACT_SETUP_BL, ::GetLastError());
  }
  if (setup_path->length() == 0) {
    // Neither setup_patch.packed.7z nor setup.ex_ was found.
    return ProcessExitResult(UNABLE_TO_EXTRACT_SETUP);
  }

  // Uncompress LZ compressed resource. Setup is packed with 'MSCF'
  // as opposed to old DOS way of 'SZDD'. Hence we don't use LZCopy.
  bool success =
      mini_installer::Expand(setup_path->get(), setup_dest_path.get());
  ::DeleteFile(setup_path->get());
  if (success) {
    if (!setup_path->assign(setup_dest_path.get())) {
      ::DeleteFile(setup_dest_path.get());
      exit_code = ProcessExitResult(PATH_STRING_OVERFLOW);
    }
  } else {
    exit_code = ProcessExitResult(UNABLE_TO_EXTRACT_SETUP_EXE);
  }

#if defined(COMPONENT_BUILD)
  if (exit_code.IsSuccess()) {
    // Extract the (uncompressed) modules required by setup.exe.
    if (!::EnumResourceNames(module, kBinResourceType, WriteResourceToDirectory,
                             reinterpret_cast<LONG_PTR>(base_path))) {
      return ProcessExitResult(UNABLE_TO_EXTRACT_SETUP, ::GetLastError());
    }
  }
#endif

  return exit_code;
}

// Executes setup.exe, waits for it to finish and returns the exit code.
ProcessExitResult RunSetup(const Configuration& configuration,
                         const wchar_t* archive_path,
                         const wchar_t* setup_path) {
  // There could be three full paths in the command line for setup.exe (path
  // to exe itself, path to archive and path to log file), so we declare
  // total size as three + one additional to hold command line options.
  CommandString cmd_line;

  // Get the path to setup.exe first.
  if (::lstrlen(setup_path) > 0) {
    if (!cmd_line.assign(L"\"") ||
        !cmd_line.append(setup_path) ||
        !cmd_line.append(L"\""))
      return ProcessExitResult(COMMAND_STRING_OVERFLOW);
  } else {
    ProcessExitResult exit_code = GetPreviousSetupExePath(
        configuration, cmd_line.get(), cmd_line.capacity());
    if (!exit_code.IsSuccess())
      return exit_code;
  }

  // Append the command line param for chrome archive file.
  if (!cmd_line.append(L" --") ||
#if defined(COMPONENT_BUILD)
      // For faster developer turnaround, the component build generates
      // uncompressed archives.
      !cmd_line.append(kCmdUncompressedArchive) ||
#else
      !cmd_line.append(kCmdInstallArchive) ||
#endif
      !cmd_line.append(L"=\"") ||
      !cmd_line.append(archive_path) ||
      !cmd_line.append(L"\""))
    return ProcessExitResult(COMMAND_STRING_OVERFLOW);

  // Append the command line param for chrome previous version.
  if (configuration.previous_version() &&
      (!cmd_line.append(L" --") ||
       !cmd_line.append(kCmdPreviousVersion) ||
       !cmd_line.append(L"=\"") ||
       !cmd_line.append(configuration.previous_version()) ||
       !cmd_line.append(L"\""))) {
    return ProcessExitResult(COMMAND_STRING_OVERFLOW);
  }

  // Get any command line option specified for mini_installer and pass them
  // on to setup.exe
  AppendCommandLineFlags(configuration, &cmd_line);

  return RunProcessAndWait(NULL, cmd_line.get());
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

// Returns true if the supplied path supports ACLs.
bool IsAclSupportedForPath(const wchar_t* path) {
  PathString volume;
  DWORD flags = 0;
  return ::GetVolumePathName(path, volume.get(),
                             static_cast<DWORD>(volume.capacity())) &&
         ::GetVolumeInformation(volume.get(), NULL, 0, NULL, NULL, &flags, NULL,
                                0) &&
         (flags & FILE_PERSISTENT_ACLS);
}

// Retrieves the SID of the default owner for objects created by this user
// token (accounting for different behavior under UAC elevation, etc.).
// NOTE: On success the |sid| parameter must be freed with LocalFree().
bool GetCurrentOwnerSid(wchar_t** sid) {
  HANDLE token;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return false;

  DWORD size = 0;
  bool result = false;
  // We get the TokenOwner rather than the TokenUser because e.g. under UAC
  // elevation we want the admin to own the directory rather than the user.
  ::GetTokenInformation(token, TokenOwner, NULL, 0, &size);
  if (size && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    if (TOKEN_OWNER* owner =
            reinterpret_cast<TOKEN_OWNER*>(::LocalAlloc(LPTR, size))) {
      if (::GetTokenInformation(token, TokenOwner, owner, size, &size))
        result = !!::ConvertSidToStringSid(owner->Owner, sid);
      ::LocalFree(owner);
    }
  }
  ::CloseHandle(token);
  return result;
}

// Populates |sd| suitable for use when creating directories within |path| with
// ACLs allowing access to only the current owner, admin, and system.
// NOTE: On success the |sd| parameter must be freed with LocalFree().
bool SetSecurityDescriptor(const wchar_t* path, PSECURITY_DESCRIPTOR* sd) {
  *sd = NULL;
  // We succeed without doing anything if ACLs aren't supported.
  if (!IsAclSupportedForPath(path))
    return true;

  wchar_t* sid = NULL;
  if (!GetCurrentOwnerSid(&sid))
    return false;

  // The largest SID is under 200 characters, so 300 should give enough slack.
  StackString<300> sddl;
  bool result = sddl.append(L"D:PAI"  // Protected, auto-inherited DACL.
                    L"(A;;FA;;;BA)"  // Admin: Full control.
                    L"(A;OIIOCI;GA;;;BA)"
                    L"(A;;FA;;;SY)"  // System: Full control.
                    L"(A;OIIOCI;GA;;;SY)"
                    L"(A;OIIOCI;GA;;;CO)"  // Owner: Full control.
                    L"(A;;FA;;;") && sddl.append(sid) && sddl.append(L")");
  if (result) {
    result = !!::ConvertStringSecurityDescriptorToSecurityDescriptor(
        sddl.get(), SDDL_REVISION_1, sd, NULL);
  }

  ::LocalFree(sid);
  return result;
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
bool CreateWorkDir(const wchar_t* base_path, PathString* work_dir,
                   ProcessExitResult* exit_code) {
  *exit_code = ProcessExitResult(PATH_STRING_OVERFLOW);
  if (!work_dir->assign(base_path) || !work_dir->append(kTempPrefix))
    return false;

  // Store the location where we'll append the id.
  size_t end = work_dir->length();

  // Check if we'll have enough buffer space to continue.
  // The name of the directory will use up 11 chars and then we need to append
  // the trailing backslash and a terminator.  We've already added the prefix
  // to the buffer, so let's just make sure we've got enough space for the rest.
  if ((work_dir->capacity() - end) < (_countof("fffff.tmp") + 1))
    return false;

  // Add an ACL if supported by the filesystem. Otherwise system-level installs
  // are potentially vulnerable to file squatting attacks.
  SECURITY_ATTRIBUTES sa = {};
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  if (!SetSecurityDescriptor(base_path, &sa.lpSecurityDescriptor)) {
    *exit_code = ProcessExitResult(UNABLE_TO_SET_DIRECTORY_ACL,
                                   ::GetLastError());
    return false;
  }

  unsigned int id;
  *exit_code = ProcessExitResult(UNABLE_TO_GET_WORK_DIRECTORY);
  for (int max_attempts = 10; max_attempts; --max_attempts) {
    ::RtlGenRandom(&id, sizeof(id));  // Try a different name.

    // This converts 'id' to a string in the format "78563412" on windows
    // because of little endianness, but we don't care since it's just
    // a name. Since we checked capaity at the front end, we don't need to
    // duplicate it here.
    HexEncode(&id, sizeof(id), work_dir->get() + end,
              work_dir->capacity() - end);

    // We only want the first 5 digits to remain within the 8.3 file name
    // format (compliant with previous implementation).
    work_dir->truncate_at(end + 5);

    // for consistency with the previous implementation which relied on
    // GetTempFileName, we append the .tmp extension.
    work_dir->append(L".tmp");

    if (::CreateDirectory(work_dir->get(),
                          sa.lpSecurityDescriptor ? &sa : NULL)) {
      // Yay!  Now let's just append the backslash and we're done.
      work_dir->append(L"\\");
      *exit_code = ProcessExitResult(SUCCESS_EXIT_CODE);
      break;
    }
  }

  if (sa.lpSecurityDescriptor)
    LocalFree(sa.lpSecurityDescriptor);
  return exit_code->IsSuccess();
}

// Creates and returns a temporary directory in |work_dir| that can be used to
// extract mini_installer payload. |work_dir| ends with a path separator.
bool GetWorkDir(HMODULE module, PathString* work_dir,
                ProcessExitResult* exit_code) {
  PathString base_path;
  DWORD len = ::GetTempPath(static_cast<DWORD>(base_path.capacity()),
                            base_path.get());
  if (!len || len >= base_path.capacity() ||
      !CreateWorkDir(base_path.get(), work_dir, exit_code)) {
    // Problem creating the work dir under TEMP path, so try using the
    // current directory as the base path.
    len = ::GetModuleFileName(module, base_path.get(),
                              static_cast<DWORD>(base_path.capacity()));
    if (len >= base_path.capacity() || !len)
      return false;  // Can't even get current directory? Return an error.

    wchar_t* name = GetNameFromPathExt(base_path.get(), len);
    if (name == base_path.get())
      return false;  // There was no directory in the string!  Bail out.

    *name = L'\0';

    *exit_code = ProcessExitResult(SUCCESS_EXIT_CODE);
    return CreateWorkDir(base_path.get(), work_dir, exit_code);
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
  DWORD len = ::GetTempPath(static_cast<DWORD>(temp.capacity()), temp.get());
  // GetTempPath returns 0 or number of chars copied, not including the
  // terminating '\0'.
  if (!len || len >= temp.capacity())
    return;

  for (size_t i = 0; i < _countof(kDirectoryPrefixes); ++i) {
    DeleteDirectoriesWithPrefix(temp.get(), kDirectoryPrefixes[i]);
  }
}

// Checks the command line for specific mini installer flags.
// If the function returns true, the command line has been processed and all
// required actions taken.  The installer must exit and return the returned
// |exit_code|.
bool ProcessNonInstallOperations(const Configuration& configuration,
                                 ProcessExitResult* exit_code) {
  switch (configuration.operation()) {
    case Configuration::CLEANUP:
      // Cleanup has already taken place in DeleteOldChromeTempDirectories at
      // this point, so just tell our caller to exit early.
      *exit_code = ProcessExitResult(SUCCESS_EXIT_CODE);
      return true;

    default:
      return false;
  }
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
  if (RegKey::ReadSZValue(HKEY_CURRENT_USER, kCleanupRegistryKey,
                          kCleanupRegistryValue, value, _countof(value)) &&
      value[0] == L'0') {
    return false;
  }

  return true;
}

// Main function. First gets a working dir, unpacks the resources and finally
// executes setup.exe to do the install/upgrade.
ProcessExitResult WMain(HMODULE module) {
  // Always start with deleting potential leftovers from previous installations.
  // This can make the difference between success and failure.  We've seen
  // many installations out in the field fail due to out of disk space problems
  // so this could buy us some space.
  DeleteOldChromeTempDirectories();

  ProcessExitResult exit_code = ProcessExitResult(SUCCESS_EXIT_CODE);

  // Parse configuration from the command line and resources.
  Configuration configuration;
  if (!configuration.Initialize(module))
    return ProcessExitResult(GENERIC_INITIALIZATION_FAILURE);

  // If the --cleanup switch was specified on the command line, then that means
  // we should only do the cleanup and then exit.
  if (ProcessNonInstallOperations(configuration, &exit_code))
    return exit_code;

  // First get a path where we can extract payload
  PathString base_path;
  if (!GetWorkDir(module, &base_path, &exit_code))
    return exit_code;

#if defined(GOOGLE_CHROME_BUILD)
  // Set the magic suffix in registry to try full installer next time. We ignore
  // any errors here and we try to set the suffix for user level unless
  // --system-level is on the command line in which case we set it for system
  // level instead. This only applies to the Google Chrome distribution.
  SetInstallerFlags(configuration);
#endif

  PathString archive_path;
  PathString setup_path;
  exit_code = UnpackBinaryResources(configuration, module, base_path.get(),
                                    &archive_path, &setup_path);

  // While unpacking the binaries, we paged in a whole bunch of memory that
  // we don't need anymore.  Let's give it back to the pool before running
  // setup.
  ::SetProcessWorkingSetSize(::GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

  if (exit_code.IsSuccess())
    exit_code = RunSetup(configuration, archive_path.get(), setup_path.get());

  if (ShouldDeleteExtractedFiles())
    DeleteExtractedFiles(base_path.get(), archive_path.get(), setup_path.get());

  WriteInstallResults(configuration, exit_code);
  return exit_code;
}

}  // namespace mini_installer

int MainEntryPoint() {
  mini_installer::ProcessExitResult result =
      mini_installer::WMain(::GetModuleHandle(NULL));

  ::ExitProcess(result.exit_code);
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
