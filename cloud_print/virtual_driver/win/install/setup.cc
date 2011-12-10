// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <setupapi.h>  // Must be included after windows.h
#include <winspool.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info_win.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "cloud_print/virtual_driver/win/virtual_driver_consts.h"
#include "cloud_print/virtual_driver/win/virtual_driver_helpers.h"
#include "grit/virtual_driver_setup_resources.h"

#include <strsafe.h>  // Must be after base headers to avoid deprecation
                      // warnings.

namespace {
const wchar_t kVersionKey[] = L"pv";
const wchar_t kNameKey[] = L"name";
const wchar_t kNameValue[] = L"GCP Virtual Driver";
const wchar_t kPpdName[] = L"GCP-DRIVER.PPD";
const wchar_t kDriverName[] = L"MXDWDRV.DLL";
const wchar_t kUiDriverName[] = L"PS5UI.DLL";
const wchar_t kHelpName[] = L"PSCRIPT.HLP";
const wchar_t* kDependencyList[] = {kDriverName, kUiDriverName, kHelpName};
const wchar_t kUninstallRegistry[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
    L"{74AA24E0-AC50-4B28-BA46-9CF05467C9B7}";
const wchar_t kInstallerName[] = L"virtual_driver_setup.exe";

void SetOmahaKeys() {
  base::win::RegKey key;
  if (key.Create(HKEY_LOCAL_MACHINE, cloud_print::kKeyLocation,
                 KEY_SET_VALUE) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to open key";
  }

  // Get the version from the resource file.
  std::wstring version_string;
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());

  if (version_info.get()) {
    FileVersionInfoWin* version_info_win =
        static_cast<FileVersionInfoWin*>(version_info.get());
    version_string = version_info_win->product_version();
  } else {
    LOG(ERROR) << "Unable to get version string";
    // Use a random version string so that Omaha has something to go by.
    version_string = L"0.0.0.99";
  }

  if (key.WriteValue(kVersionKey, version_string.c_str()) != ERROR_SUCCESS ||
      key.WriteValue(kNameKey, kNameValue) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to set registry keys";
  }
}

void DeleteOmahaKeys() {
  base::win::RegKey key;
  if (key.Open(HKEY_LOCAL_MACHINE, cloud_print::kKeyLocation,
               DELETE) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to open key to delete";
  }
  if (key.DeleteKey(L"") != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to delete key";
  }
}

HRESULT GetNativeSystemPath(FilePath* path) {
  if (cloud_print::IsSystem64Bit()) {
    if (!PathService::Get(base::DIR_WINDOWS, path)) {
      return ERROR_PATH_NOT_FOUND;
    }
    // Sysnative will bypass filesystem redirection and give us
    // the location of the 64bit system32 from a 32 bit process.
    *path = path->Append(L"sysnative");
  } else {
    if (!PathService::Get(base::DIR_SYSTEM, path)) {
      LOG(ERROR) << "Unable to get system path.";
      return ERROR_PATH_NOT_FOUND;
    }
  }
  return S_OK;
}

HRESULT GetPortMonitorTargetPath(FilePath* path) {
  HRESULT result = GetNativeSystemPath(path);
  if (SUCCEEDED(result))
    *path = path->Append(cloud_print::GetPortMonitorDllName());
  return result;
}

HRESULT GetRegsvr32Path(FilePath* path) {
  HRESULT result = GetNativeSystemPath(path);
  if (SUCCEEDED(result))
    *path = path->Append(FilePath(L"regsvr32.exe"));
  return result;
}

HRESULT RegisterPortMonitor(bool install, const FilePath& install_path) {
  FilePath target_path;
  HRESULT result = S_OK;
  result = GetPortMonitorTargetPath(&target_path);
  if (!SUCCEEDED(result)) {
    LOG(ERROR) << "Unable to get port monitor target path.";
    return result;
  }
  string16 source;
  source = cloud_print::GetPortMonitorDllName();
  if (install) {
    FilePath source_path = install_path.Append(source);
    if (!file_util::CopyFileW(source_path, target_path)) {
      LOG(ERROR) << "Unable copy port monitor dll from " <<
          source_path.value() << " to " << target_path.value();
      return ERROR_ACCESS_DENIED;
    }
  }
  FilePath regsvr32_path;
  result = GetRegsvr32Path(&regsvr32_path);
  if (!SUCCEEDED(result)) {
    LOG(ERROR) << "Can't find regsvr32.exe.";
    return result;
  }

  CommandLine command_line(regsvr32_path);
  command_line.AppendArg("/s");
  if (!install) {
    command_line.AppendArg("/u");
  }

  FilePath final_path;
  if (!PathService::Get(base::DIR_SYSTEM, &final_path)) {
    LOG(ERROR) << "Unable to get system path.";
    return ERROR_PATH_NOT_FOUND;
  }
  final_path = final_path.Append(cloud_print::GetPortMonitorDllName());
  command_line.AppendArgPath(final_path);

  base::LaunchOptions options;
  HANDLE process_handle;
  options.wait = true;
  if (!base::LaunchProcess(command_line, options, &process_handle)) {
    LOG(ERROR) << "Unable to launch regsvr32.exe.";
    return ERROR_NOT_SUPPORTED;
  }
  base::win::ScopedHandle scoped_process_handle(process_handle);

  DWORD exit_code = S_OK;
  if (!GetExitCodeProcess(scoped_process_handle, &exit_code)) {
    HRESULT result = cloud_print::GetLastHResult();
    LOG(ERROR) << "Unable to get regsvr32.exe exit code.";
    return result;
  }
  if (exit_code != 0) {
    LOG(ERROR) << "Regsvr32.exe failed with " << exit_code;
    return HRESULT_FROM_WIN32(exit_code);
  }
  if (!install) {
    if (!file_util::Delete(target_path, false)) {
      LOG(ERROR) << "Unable to delete " << target_path.value();
      return ERROR_ACCESS_DENIED;
    }
  }
  return S_OK;
}

DWORDLONG GetVersionNumber() {
  DWORDLONG retval = 0;
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (version_info.get()) {
    FileVersionInfoWin* version_info_win =
        static_cast<FileVersionInfoWin*>(version_info.get());
    VS_FIXEDFILEINFO* fixed_file_info = version_info_win->fixed_file_info();
    retval = fixed_file_info->dwFileVersionMS;
    retval <<= 32;
    retval |= fixed_file_info->dwFileVersionMS;
  }
  return retval;
}

UINT CALLBACK CabinetCallback(PVOID data,
                              UINT notification,
                              UINT_PTR param1,
                              UINT_PTR param2 ) {
  FilePath* temp_path = reinterpret_cast<FilePath*>(data);
  if (notification == SPFILENOTIFY_FILEINCABINET) {
    FILE_IN_CABINET_INFO* info =
        reinterpret_cast<FILE_IN_CABINET_INFO*>(param1);
    for (int i = 0; i < arraysize(kDependencyList); i++) {
      if (wcsstr(info->NameInCabinet, kDependencyList[i])) {
        StringCchCopy(info->FullTargetName, MAX_PATH,
                      temp_path->Append(kDependencyList[i]).value().c_str());
        return FILEOP_DOIT;
      }
    }

    return FILEOP_SKIP;
  }
  return NO_ERROR;
}

void ReadyPpdDependencies(const FilePath& install_path) {
  CORE_PRINTER_DRIVER driver;
  GetCorePrinterDrivers(NULL,
                        NULL,
                        L"{D20EA372-DD35-4950-9ED8-A6335AFE79F0}",
                        1,
                        &driver);
  DWORD size = MAX_PATH;
  wchar_t package_path[MAX_PATH];
  GetPrinterDriverPackagePath(NULL,
                              NULL,
                              NULL,
                              driver.szPackageID,
                              package_path,
                              MAX_PATH,
                              &size);

  SetupIterateCabinet(package_path,
                      0,
                      CabinetCallback,
                      const_cast<FilePath*>(&install_path));
}

HRESULT InstallPpd(const FilePath& install_path) {
  DRIVER_INFO_6 driver_info = {0};
  HRESULT result = S_OK;

  // Set up paths for the files we depend on.
  FilePath ppd_path = install_path.Append(kPpdName);
  FilePath xps_path = install_path.Append(kDriverName);
  FilePath ui_path = install_path.Append(kUiDriverName);
  FilePath ui_help_path = install_path.Append(kHelpName);
  ReadyPpdDependencies(install_path);
  // None of the print API structures likes constant strings even though they
  // don't modify the string.  const_casting is the cleanest option.
  driver_info.pDataFile = const_cast<LPWSTR>(ppd_path.value().c_str());
  driver_info.pHelpFile = const_cast<LPWSTR>(ui_help_path.value().c_str());
  driver_info.pDriverPath = const_cast<LPWSTR>(xps_path.value().c_str());
  driver_info.pConfigFile = const_cast<LPWSTR>(ui_path.value().c_str());

  // Set up user visible strings.
  string16 manufacturer = cloud_print::LoadLocalString(IDS_GOOGLE);
  driver_info.pszMfgName = const_cast<LPWSTR>(manufacturer.c_str());
  driver_info.pszProvider = const_cast<LPWSTR>(manufacturer.c_str());
  driver_info.pszOEMUrl = L"http://www.google.com/cloudprint";
  driver_info.dwlDriverVersion = GetVersionNumber();
  string16 driver_name = cloud_print::LoadLocalString(IDS_DRIVER_NAME);
  driver_info.pName = const_cast<LPWSTR>(driver_name.c_str());

  // Set up supported print system version.  Must be 3.
  driver_info.cVersion = 3;

  if (!AddPrinterDriverEx(NULL,
                          6,
                          reinterpret_cast<BYTE*>(&driver_info),
                          APD_COPY_NEW_FILES|APD_COPY_FROM_DIRECTORY)) {
    result = cloud_print::GetLastHResult();
    LOG(ERROR) << "Unable to add printer driver";
  }
  return result;
}

HRESULT UninstallPpd() {
  int tries = 10;
  string16 driver_name = cloud_print::LoadLocalString(IDS_DRIVER_NAME);
  while (!DeletePrinterDriverEx(NULL,
                                NULL,
                                const_cast<LPWSTR>(driver_name.c_str()),
                                DPD_DELETE_UNUSED_FILES,
                                0) && tries > 0) {
    if (GetLastError() == ERROR_UNKNOWN_PRINTER_DRIVER) {
      LOG(WARNING) << "Print driver is already uninstalled.";
      return S_OK;
    }
    // After deleting the printer it can take a few seconds before
    // the driver is free for deletion.  Retry a few times before giving up.
    LOG(WARNING) << "Attempt to delete printer driver failed.  Retrying.";
    tries--;
    Sleep(2000);
  }
  if (tries <= 0) {
    HRESULT result = cloud_print::GetLastHResult();
    LOG(ERROR) << "Unable to delete printer driver.";
    return result;
  }
  return S_OK;
}

HRESULT InstallPrinter(void) {
  PRINTER_INFO_2 printer_info = {0};

  // None of the print API structures likes constant strings even though they
  // don't modify the string.  const_casting is the cleanest option.
  string16 driver_name = cloud_print::LoadLocalString(IDS_DRIVER_NAME);
  printer_info.pDriverName = const_cast<LPWSTR>(driver_name.c_str());
  printer_info.pPrinterName = const_cast<LPWSTR>(driver_name.c_str());
  printer_info.pComment =  const_cast<LPWSTR>(driver_name.c_str());
  string16 port_name;
  printer_info.pPortName = const_cast<LPWSTR>(cloud_print::kPortName);
  printer_info.Attributes = PRINTER_ATTRIBUTE_DIRECT|PRINTER_ATTRIBUTE_LOCAL;
  printer_info.pPrintProcessor = L"winprint";
  HANDLE handle = AddPrinter(NULL, 2, reinterpret_cast<BYTE*>(&printer_info));
  if (handle == NULL) {
    HRESULT result = cloud_print::GetLastHResult();
    LOG(ERROR) << "Unable to add printer";
    return result;
  }
  ClosePrinter(handle);
  return S_OK;
}

HRESULT UninstallPrinter(void) {
  HANDLE handle = NULL;
  PRINTER_DEFAULTS printer_defaults = {0};
  printer_defaults.DesiredAccess = PRINTER_ALL_ACCESS;
  string16 driver_name = cloud_print::LoadLocalString(IDS_DRIVER_NAME);
  if (!OpenPrinter(const_cast<LPWSTR>(driver_name.c_str()),
                   &handle,
                   &printer_defaults)) {
    // If we can't open the printer, it was probably already removed.
    LOG(WARNING) << "Unable to open printer";
    return S_OK;
  }
  if (!DeletePrinter(handle)) {
    HRESULT result = cloud_print::GetLastHResult();
    LOG(ERROR) << "Unable to delete printer";
    ClosePrinter(handle);
    return result;
  }
  ClosePrinter(handle);
  return S_OK;
}

void SetupUninstall(const FilePath& install_path) {
  // Now write the Windows Uninstall entries
  // Minimal error checking here since the install can contiunue
  // if this fails.
  base::win::RegKey key;
  if (key.Create(HKEY_LOCAL_MACHINE, kUninstallRegistry,
                 KEY_SET_VALUE) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to open key";
    return;
  }
  CommandLine uninstall_command(install_path.Append(kInstallerName));
  uninstall_command.AppendArg("--uninstall");
  key.WriteValue(L"UninstallString",
                 uninstall_command.GetCommandLineString().c_str());
  key.WriteValue(L"InstallLocation", install_path.value().c_str());


  // Get the version resource.
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());

  if (version_info.get()) {
    FileVersionInfoWin* version_info_win =
        static_cast<FileVersionInfoWin*>(version_info.get());
    key.WriteValue(L"DisplayVersion",
                   version_info_win->file_version().c_str());
    key.WriteValue(L"Publisher", version_info_win->company_name().c_str());
  } else {
    LOG(ERROR) << "Unable to get version string";
  }
  key.WriteValue(L"DisplayName",
                 cloud_print::LoadLocalString(IDS_DRIVER_NAME).c_str());
  key.WriteValue(L"NoModify", 1);
  key.WriteValue(L"NoRepair", 1);
}

void CleanupUninstall() {
  ::RegDeleteKey(HKEY_LOCAL_MACHINE, kUninstallRegistry);
}

HRESULT InstallVirtualDriver(const FilePath& install_path) {
  HRESULT result = S_OK;
  if (!file_util::CreateDirectory(install_path)) {
    LOG(ERROR) << "Can't create install directory";
    return ERROR_ACCESS_DENIED;
  }
  SetupUninstall(install_path);
  result = RegisterPortMonitor(true, install_path);
  if (!SUCCEEDED(result)) {
    LOG(ERROR) << "Unable to register port monitor.";
    return result;
  }
  result = InstallPpd(install_path);
  if (!SUCCEEDED(result)) {
    LOG(ERROR) << "Unable to install Ppd.";
    return result;
  }
  result = InstallPrinter();
  if (!SUCCEEDED(result)) {
    LOG(ERROR) << "Unable to install printer.";
    return result;
  }
  SetOmahaKeys();
  return S_OK;
}

HRESULT UninstallVirtualDriver(const FilePath& install_path) {
  HRESULT result = S_OK;
  result = UninstallPrinter();
  if (!SUCCEEDED(result)) {
    LOG(ERROR) << "Unable to uninstall Ppd.";
    return result;
  }
  result = UninstallPpd();
  if (!SUCCEEDED(result)) {
    LOG(ERROR) << "Unable to remove Ppd.";
    return result;
  }
  result = RegisterPortMonitor(false, install_path);
  if (!SUCCEEDED(result)) {
    LOG(ERROR) << "Unable to remove port monitor.";
    return result;
  }
  DeleteOmahaKeys();
  file_util::Delete(install_path, true);
  CleanupUninstall();
  return S_OK;
}

HRESULT LaunchChildForUninstall() {
  FilePath installer_source;
  if (PathService::Get(base::FILE_EXE, &installer_source)) {
    FilePath temp_path;
    if (file_util::CreateTemporaryFile(&temp_path)) {
      file_util::Move(installer_source, temp_path);
      file_util::DeleteAfterReboot(temp_path);
      CommandLine command_line(temp_path);
      command_line.AppendArg("--douninstall");
      base::LaunchOptions options;
      if (!base::LaunchProcess(command_line, options, NULL)) {
        LOG(ERROR) << "Unable to launch child uninstall.";
        return ERROR_NOT_SUPPORTED;
      }
    }
  }
  return S_OK;
}
}  // namespace

int WINAPI WinMain(__in  HINSTANCE hInstance,
            __in  HINSTANCE hPrevInstance,
            __in  LPSTR lpCmdLine,
            __in  int nCmdShow) {
  base::AtExitManager at_exit_manager;
  CommandLine::Init(0, NULL);

  FilePath install_path;
  HRESULT retval = PathService::Get(base::DIR_EXE, &install_path);
  if (SUCCEEDED(retval)) {
    if (CommandLine::ForCurrentProcess()->HasSwitch("douninstall")) {
      retval = UninstallVirtualDriver(install_path);
    } else if (CommandLine::ForCurrentProcess()->HasSwitch("uninstall")) {
      retval = LaunchChildForUninstall();
    } else {
      retval = InstallVirtualDriver(install_path);
    }
  }
  // Installer is silent by default as required by Omaha.
  if (CommandLine::ForCurrentProcess()->HasSwitch("verbose")) {
    cloud_print::DisplayWindowsMessage(NULL, retval,
        cloud_print::LoadLocalString(IDS_DRIVER_NAME));
  }
  return retval;
}
