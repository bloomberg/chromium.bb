// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>
#include <windows.h>
#include <winspool.h>
#include <setupapi.h>  // Must be included after windows.h

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info_win.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/process/launch.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "cloud_print/common/win/cloud_print_utils.h"
#include "cloud_print/common/win/install_utils.h"
#include "cloud_print/virtual_driver/win/virtual_driver_consts.h"
#include "cloud_print/virtual_driver/win/virtual_driver_helpers.h"
#include "grit/virtual_driver_setup_resources.h"

#include <strsafe.h>  // Must be after base headers to avoid deprecation
                      // warnings.

namespace cloud_print {

namespace {

const wchar_t kNameValue[] = L"GCP Virtual Driver";
const wchar_t kUninstallId[] = L"{74AA24E0-AC50-4B28-BA46-9CF05467C9B7}";
const wchar_t kInstallerName[] = L"virtual_driver_setup.exe";
const wchar_t kGcpUrl[] = L"http://www.google.com/cloudprint";

const wchar_t kDataFileName[] = L"gcp_driver.gpd";
const wchar_t kDriverName[] = L"MXDWDRV.DLL";
const wchar_t kUiDriverName[] = L"UNIDRVUI.DLL";
const wchar_t kHelpName[] = L"UNIDRV.HLP";
const wchar_t* kDependencyList[] = {
  kDriverName,
  kHelpName,
  kUiDriverName,
  L"STDDTYPE.GDL",
  L"STDNAMES.GPD",
  L"STDSCHEM.GDL",
  L"STDSCHMX.GDL",
  L"UNIDRV.DLL",
  L"UNIRES.DLL",
  L"XPSSVCS.DLL",
};

const char kDelete[] = "delete";
const char kInstallSwitch[] = "install";
const char kRegisterSwitch[] = "register";
const char kUninstallSwitch[] = "uninstall";
const char kUnregisterSwitch[] = "unregister";

base::FilePath GetSystemPath(const base::string16& binary) {
  base::FilePath path;
  if (!PathService::Get(base::DIR_SYSTEM, &path)) {
    LOG(ERROR) << "Unable to get system path.";
    return path;
  }
  return path.Append(binary);
}

base::FilePath GetNativeSystemPath(const base::string16& binary) {
  if (!IsSystem64Bit())
    return GetSystemPath(binary);
  base::FilePath path;
  // Sysnative will bypass filesystem redirection and give us
  // the location of the 64bit system32 from a 32 bit process.
  if (!PathService::Get(base::DIR_WINDOWS, &path)) {
    LOG(ERROR) << "Unable to get windows path.";
    return path;
  }
  return path.Append(L"sysnative").Append(binary);
}

void SpoolerServiceCommand(const char* command) {
  base::FilePath net_path = GetNativeSystemPath(L"net");
  if (net_path.empty())
    return;
  CommandLine command_line(net_path);
  command_line.AppendArg(command);
  command_line.AppendArg("spooler");
  command_line.AppendArg("/y");

  base::LaunchOptions options;
  options.wait = true;
  options.start_hidden = true;
  VLOG(0) << command_line.GetCommandLineString();
  base::LaunchProcess(command_line, options, NULL);
}

HRESULT RegisterPortMonitor(bool install, const base::FilePath& install_path) {
  DCHECK(install || install_path.empty());
  base::FilePath target_path = GetNativeSystemPath(GetPortMonitorDllName());
  if (target_path.empty()) {
    LOG(ERROR) << "Unable to get port monitor target path.";
    return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
  }
  if (install) {
    base::FilePath source_path =
        install_path.Append(GetPortMonitorDllName());
    if (!base::CopyFile(source_path, target_path)) {
      LOG(ERROR) << "Unable copy port monitor dll from " <<
          source_path.value() << " to " << target_path.value();
      return GetLastHResult();
    }
  } else if (!base::PathExists(target_path)) {
    // Already removed.  Just "succeed" silently.
    return S_OK;
  }

  base::FilePath regsvr32_path = GetNativeSystemPath(L"regsvr32.exe");
  if (regsvr32_path.empty()) {
    LOG(ERROR) << "Can't find regsvr32.exe.";
    return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
  }

  CommandLine command_line(regsvr32_path);
  command_line.AppendArg("/s");
  if (!install) {
    command_line.AppendArg("/u");
  }

  // Use system32 path here because otherwise ::AddMonitor would fail.
  command_line.AppendArgPath(GetSystemPath(GetPortMonitorDllName()));

  base::LaunchOptions options;
  options.wait = true;

  base::win::ScopedHandle regsvr32_handle;
  if (!base::LaunchProcess(command_line.GetCommandLineString(), options,
                           &regsvr32_handle)) {
    LOG(ERROR) << "Unable to launch regsvr32.exe.";
    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
  }

  DWORD exit_code = S_OK;
  if (install) {
    if (!GetExitCodeProcess(regsvr32_handle, &exit_code)) {
      LOG(ERROR) << "Unable to get regsvr32.exe exit code.";
      return GetLastHResult();
    }
    if (exit_code != 0) {
      LOG(ERROR) << "Regsvr32.exe failed with " << exit_code;
      return HRESULT_FROM_WIN32(exit_code);
    }
  } else {
    if (!base::DeleteFile(target_path, false)) {
      SpoolerServiceCommand("stop");
      bool deleted = base::DeleteFile(target_path, false);
      SpoolerServiceCommand("start");

      if(!deleted) {
        LOG(ERROR) << "Unable to delete " << target_path.value();
        return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
      }
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
    retval |= fixed_file_info->dwFileVersionLS;
  }
  return retval;
}

UINT CALLBACK CabinetCallback(PVOID data,
                              UINT notification,
                              UINT_PTR param1,
                              UINT_PTR param2) {
  const base::FilePath* temp_path(
      reinterpret_cast<const base::FilePath*>(data));
  if (notification == SPFILENOTIFY_FILEINCABINET) {
    FILE_IN_CABINET_INFO* info =
        reinterpret_cast<FILE_IN_CABINET_INFO*>(param1);
    for (int i = 0; i < arraysize(kDependencyList); i++) {
      base::FilePath base_name(info->NameInCabinet);
      base_name = base_name.BaseName();
      if (base::FilePath::CompareEqualIgnoreCase(base_name.value().c_str(),
                                                 kDependencyList[i])) {
        StringCchCopy(info->FullTargetName, MAX_PATH,
                      temp_path->Append(kDependencyList[i]).value().c_str());
        return FILEOP_DOIT;
      }
    }
    return FILEOP_SKIP;
  }
  return NO_ERROR;
}

void ReadyDriverDependencies(const base::FilePath& destination) {
  base::FilePath destination_copy(destination);
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    // GetCorePrinterDrivers and GetPrinterDriverPackagePath only exist on
    // Vista and later. Winspool.drv must be delayloaded so these calls don't
    // create problems on XP.
    DWORD size = MAX_PATH;
    wchar_t package_path[MAX_PATH] = {0};
    CORE_PRINTER_DRIVER driver;
    GetCorePrinterDrivers(NULL, NULL, L"{D20EA372-DD35-4950-9ED8-A6335AFE79F5}",
                          1, &driver);
    GetPrinterDriverPackagePath(NULL, NULL, NULL, driver.szPackageID,
                                package_path, MAX_PATH, &size);
    SetupIterateCabinet(package_path, 0, &CabinetCallback, &destination_copy);
  } else {
    // Driver files are in the sp3 cab.
    base::FilePath package_path;
    PathService::Get(base::DIR_WINDOWS, &package_path);
    package_path = package_path.Append(L"Driver Cache\\i386\\sp3.cab");
    SetupIterateCabinet(package_path.value().c_str(), 0, &CabinetCallback,
                        &destination_copy);

    // Copy the rest from the driver cache or system dir.
    base::FilePath driver_cache_path;
    PathService::Get(base::DIR_WINDOWS, &driver_cache_path);
    driver_cache_path = driver_cache_path.Append(L"Driver Cache\\i386");
    for (size_t i = 0; i < arraysize(kDependencyList); ++i) {
      base::FilePath dst_path = destination.Append(kDependencyList[i]);
      if (!base::PathExists(dst_path)) {
        base::FilePath src_path = driver_cache_path.Append(kDependencyList[i]);
        if (!base::PathExists(src_path))
          src_path = GetSystemPath(kDependencyList[i]);
        base::CopyFile(src_path, dst_path);
      }
    }
  }
}

HRESULT InstallDriver(const base::FilePath& install_path) {
  base::ScopedTempDir temp_path;
  if (!temp_path.CreateUniqueTempDir())
    return HRESULT_FROM_WIN32(ERROR_CANNOT_MAKE);
  ReadyDriverDependencies(temp_path.path());

  std::vector<base::string16> dependent_array;
  // Add all files. AddPrinterDriverEx will removes unnecessary.
  for (size_t i = 0; i < arraysize(kDependencyList); ++i) {
    base::FilePath file_path = temp_path.path().Append(kDependencyList[i]);
    if (base::PathExists(file_path))
      dependent_array.push_back(file_path.value());
    else
      LOG(WARNING) << "File is missing: " << file_path.BaseName().value();
  }

  // Set up paths for the files we depend on.
  base::FilePath data_file = install_path.Append(kDataFileName);
  base::FilePath xps_path = temp_path.path().Append(kDriverName);
  base::FilePath ui_path = temp_path.path().Append(kUiDriverName);
  base::FilePath ui_help_path = temp_path.path().Append(kHelpName);

  if (!base::PathExists(xps_path)) {
    return HRESULT_FROM_WIN32(ERROR_BAD_DRIVER);
  }

  DRIVER_INFO_6 driver_info = {0};
  // Set up supported print system version.  Must be 3.
  driver_info.cVersion = 3;

  // None of the print API structures likes constant strings even though they
  // don't modify the string.  const_casting is the cleanest option.
  driver_info.pDataFile = const_cast<LPWSTR>(data_file.value().c_str());
  driver_info.pHelpFile = const_cast<LPWSTR>(ui_help_path.value().c_str());
  driver_info.pDriverPath = const_cast<LPWSTR>(xps_path.value().c_str());
  driver_info.pConfigFile = const_cast<LPWSTR>(ui_path.value().c_str());

  base::string16 dependent_files(JoinString(dependent_array, L'\n'));
  dependent_files.push_back(L'\n');
  std::replace(dependent_files.begin(), dependent_files.end(), L'\n', L'\0');
  driver_info.pDependentFiles = &dependent_files[0];

  // Set up user visible strings.
  base::string16 manufacturer = LoadLocalString(IDS_GOOGLE);
  driver_info.pszMfgName = const_cast<LPWSTR>(manufacturer.c_str());
  driver_info.pszProvider = const_cast<LPWSTR>(manufacturer.c_str());
  driver_info.pszOEMUrl = const_cast<LPWSTR>(kGcpUrl);
  driver_info.dwlDriverVersion = GetVersionNumber();
  base::string16 driver_name = LoadLocalString(IDS_DRIVER_NAME);
  driver_info.pName = const_cast<LPWSTR>(driver_name.c_str());

  if (!::AddPrinterDriverEx(NULL, 6, reinterpret_cast<BYTE*>(&driver_info),
                            APD_COPY_NEW_FILES | APD_COPY_FROM_DIRECTORY)) {
    LOG(ERROR) << "Unable to add printer driver";
    return GetLastHResult();
  }
  return S_OK;
}

HRESULT UninstallDriver() {
  int tries = 3;
  base::string16 driver_name = LoadLocalString(IDS_DRIVER_NAME);
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
    HRESULT result = GetLastHResult();
    LOG(ERROR) << "Unable to delete printer driver.";
    return result;
  }
  return S_OK;
}

HRESULT InstallPrinter(void) {
  PRINTER_INFO_2 printer_info = {0};

  // None of the print API structures likes constant strings even though they
  // don't modify the string.  const_casting is the cleanest option.
  base::string16 driver_name = LoadLocalString(IDS_DRIVER_NAME);
  printer_info.pDriverName = const_cast<LPWSTR>(driver_name.c_str());
  printer_info.pPrinterName = const_cast<LPWSTR>(driver_name.c_str());
  printer_info.pComment =  const_cast<LPWSTR>(driver_name.c_str());
  printer_info.pLocation = const_cast<LPWSTR>(kGcpUrl);
  base::string16 port_name;
  printer_info.pPortName = const_cast<LPWSTR>(kPortName);
  printer_info.Attributes = PRINTER_ATTRIBUTE_DIRECT|PRINTER_ATTRIBUTE_LOCAL;
  printer_info.pPrintProcessor = L"winprint";
  HANDLE handle = AddPrinter(NULL, 2, reinterpret_cast<BYTE*>(&printer_info));
  if (handle == NULL) {
    HRESULT result = GetLastHResult();
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
  base::string16 driver_name = LoadLocalString(IDS_DRIVER_NAME);
  if (!OpenPrinter(const_cast<LPWSTR>(driver_name.c_str()),
                   &handle,
                   &printer_defaults)) {
    // If we can't open the printer, it was probably already removed.
    LOG(WARNING) << "Unable to open printer";
    return S_OK;
  }
  if (!DeletePrinter(handle)) {
    HRESULT result = GetLastHResult();
    LOG(ERROR) << "Unable to delete printer";
    ClosePrinter(handle);
    return result;
  }
  ClosePrinter(handle);
  return S_OK;
}

bool IsOSSupported() {
  // We don't support XP service pack 2 or older.
  base::win::Version version = base::win::GetVersion();
  return (version > base::win::VERSION_XP) ||
      ((version == base::win::VERSION_XP) &&
       (base::win::OSInfo::GetInstance()->service_pack().major >= 3));
}

HRESULT RegisterVirtualDriver(const base::FilePath& install_path) {
  HRESULT result = S_OK;

  DCHECK(base::DirectoryExists(install_path));
  if (!IsOSSupported()) {
    LOG(ERROR) << "Requires XP SP3 or later.";
    return HRESULT_FROM_WIN32(ERROR_OLD_WIN_VERSION);
  }

  result = InstallDriver(install_path);
  if (FAILED(result)) {
    LOG(ERROR) << "Unable to install driver.";
    return result;
  }

  result = RegisterPortMonitor(true, install_path);
  if (FAILED(result)) {
    LOG(ERROR) << "Unable to register port monitor.";
    return result;
  }

  result = InstallPrinter();
  if (FAILED(result) &&
      result != HRESULT_FROM_WIN32(ERROR_PRINTER_ALREADY_EXISTS)) {
    LOG(ERROR) << "Unable to install printer.";
    return result;
  }
  return S_OK;
}

HRESULT TryUnregisterVirtualDriver() {
  HRESULT result = S_OK;
  result = UninstallPrinter();
  if (FAILED(result)) {
    LOG(ERROR) << "Unable to delete printer.";
    return result;
  }
  result = UninstallDriver();
  if (FAILED(result)) {
    LOG(ERROR) << "Unable to remove driver.";
    return result;
  }
  // The second argument is ignored if the first is false.
  result = RegisterPortMonitor(false, base::FilePath());
  if (FAILED(result)) {
    LOG(ERROR) << "Unable to remove port monitor.";
    return result;
  }
  return S_OK;
}

HRESULT UnregisterVirtualDriver() {
  HRESULT hr = S_FALSE;
  for (int i = 0; i < 2; ++i) {
    hr = TryUnregisterVirtualDriver();
    if (SUCCEEDED(hr)) {
      break;
    }
    // Restart spooler and try again.
    SpoolerServiceCommand("stop");
    SpoolerServiceCommand("start");
  }
  return hr;
}

HRESULT DoUninstall() {
  DeleteGoogleUpdateKeys(kGoogleUpdateProductId);
  HRESULT result = UnregisterVirtualDriver();
  if (FAILED(result))
    return result;
  DeleteUninstallKey(kUninstallId);
  DeleteProgramDir(kDelete);
  return S_OK;
}

HRESULT DoUnregister() {
  return UnregisterVirtualDriver();
}

HRESULT DoRegister(const base::FilePath& install_path) {
  HRESULT result = UnregisterVirtualDriver();
  if (FAILED(result))
    return result;
  return RegisterVirtualDriver(install_path);
}

HRESULT DoDelete(const base::FilePath& install_path) {
  if (install_path.value().empty())
    return E_INVALIDARG;
  if (!base::DirectoryExists(install_path))
    return S_FALSE;
  Sleep(5000);  // Give parent some time to exit.
  return base::DeleteFile(install_path, true) ? S_OK : E_FAIL;
}

HRESULT DoInstall(const base::FilePath& install_path) {
  HRESULT result = UnregisterVirtualDriver();
  if (FAILED(result)) {
    LOG(ERROR) << "Unable to unregister.";
    return result;
  }
  base::FilePath old_install_path = GetInstallLocation(kUninstallId);
  if (!old_install_path.value().empty() &&
      install_path != old_install_path) {
    if (base::DirectoryExists(old_install_path))
      base::DeleteFile(old_install_path, true);
  }
  CreateUninstallKey(kUninstallId, LoadLocalString(IDS_DRIVER_NAME),
                     kUninstallSwitch);
  result = RegisterVirtualDriver(install_path);
  if (FAILED(result))
    return result;
  SetGoogleUpdateKeys(kGoogleUpdateProductId, kNameValue);
  return result;
}

HRESULT ExecuteCommands() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  base::FilePath exe_path;
  if (FAILED(PathService::Get(base::DIR_EXE, &exe_path)) ||
      !base::DirectoryExists(exe_path)) {
    return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
  }

  if (command_line.HasSwitch(kDelete)) {
    return DoDelete(command_line.GetSwitchValuePath(kDelete));
  } else if (command_line.HasSwitch(kUninstallSwitch)) {
    return DoUninstall();
  } else if (command_line.HasSwitch(kInstallSwitch)) {
    return DoInstall(exe_path);
  } else if (command_line.HasSwitch(kUnregisterSwitch)) {
    return DoUnregister();
  } else if (command_line.HasSwitch(kRegisterSwitch)) {
    return DoRegister(exe_path);
  }

  return E_INVALIDARG;
}

}  // namespace

}  // namespace cloud_print

int WINAPI WinMain(__in  HINSTANCE hInstance,
                   __in  HINSTANCE hPrevInstance,
                   __in  LPSTR lpCmdLine,
                   __in  int nCmdShow) {
  using namespace cloud_print;

  base::AtExitManager at_exit_manager;
  CommandLine::Init(0, NULL);

  HRESULT retval = ExecuteCommands();

  if (retval == HRESULT_FROM_WIN32(ERROR_BAD_DRIVER)) {
    SetGoogleUpdateError(kGoogleUpdateProductId,
                         LoadLocalString(IDS_ERROR_NO_XPS));
  } else if (FAILED(retval)) {
    SetGoogleUpdateError(kGoogleUpdateProductId, retval);
  }

  VLOG(0) << GetErrorMessage(retval)
          << " HRESULT=0x" << std::setbase(16) << retval;

  // Installer is silent by default as required by Google Update.
  if (CommandLine::ForCurrentProcess()->HasSwitch("verbose")) {
    DisplayWindowsMessage(NULL, retval, LoadLocalString(IDS_DRIVER_NAME));
  }
  return retval;
}
