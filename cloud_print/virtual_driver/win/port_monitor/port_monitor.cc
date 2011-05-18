// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/virtual_driver/win/port_monitor/port_monitor.h"

#include <lmcons.h>
#include <shlobj.h>
#include <strsafe.h>
#include <userenv.h>
#include <windows.h>
#include <winspool.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "cloud_print/virtual_driver/win/port_monitor/spooler_win.h"
#include "cloud_print/virtual_driver/win/virtual_driver_consts.h"
#include "cloud_print/virtual_driver/win/virtual_driver_helpers.h"

namespace switches {
// These constants are duplicated from chrome/common/chrome_switches.cc
// in order to avoid dependency problems.
// TODO(abodenha@chromium.org) Reunify them in some sensible manner.

// Used with kCloudPrintFile.  Tells Chrome to delete the file when
// finished displaying the print dialog.
const char kCloudPrintDeleteFile[]          = "cloud-print-delete-file";

// Tells chrome to display the cloud print dialog and upload the
// specified file for printing.
const char kCloudPrintFile[]                = "cloud-print-file";

// Used with kCloudPrintFile to specify a title for the resulting print
// job.
const char kCloudPrintJobTitle[]            = "cloud-print-job-title";

// Specifies the mime type to be used when uploading data from the
// file referenced by cloud-print-file.
// Defaults to "application/pdf" if unspecified.
const char kCloudPrintFileType[]            = "cloud-print-file-type";
}

namespace cloud_print {

#ifndef UNIT_TEST
const wchar_t kChromeExePath[] = L"google\\chrome\\application\\chrome.exe";
const wchar_t kChromePathRegValue[] = L"PathToChromeExe";
#endif

const wchar_t kChromePathRegKey[] = L"Software\\Google\\CloudPrint";

namespace {
const wchar_t kXpsMimeType[] = L"application/vnd.ms-xpsdocument";

const size_t kMaxCommandLineLen = 0x7FFF;


struct MonitorData {
  base::AtExitManager* at_exit_manager;
};

struct PortData {
  DWORD job_id;
  HANDLE printer_handle;
  FILE* file;
  FilePath* file_path;
};

typedef struct {
  ACCESS_MASK granted_access;
} XcvUiData;


MONITORUI g_monitor_ui = {
  sizeof(MONITORUI),
  MonitorUiAddPortUi,
  MonitorUiConfigureOrDeletePortUI,
  MonitorUiConfigureOrDeletePortUI
};

MONITOR2 g_monitor_2 = {
  sizeof(MONITOR2),
  Monitor2EnumPorts,
  Monitor2OpenPort,
  NULL,           // OpenPortEx is not supported.
  Monitor2StartDocPort,
  Monitor2WritePort,
  Monitor2ReadPort,
  Monitor2EndDocPort,
  Monitor2ClosePort,
  NULL,           // AddPort is not supported.
  NULL,           // AddPortEx is not supported.
  NULL,           // ConfigurePort is not supported.
  NULL,           // DeletePort is not supported.
  NULL,
  NULL,           // SetPortTimeOuts is not supported.
  Monitor2XcvOpenPort,
  Monitor2XcvDataPort,
  Monitor2XcvClosePort,
  Monitor2Shutdown
};

// Returns true if Xps support is installed.
bool XpsIsInstalled() {
  FilePath xps_path;
  if (!SUCCEEDED(GetPrinterDriverDir(&xps_path))) {
    return false;
  }
  xps_path = xps_path.Append(L"mxdwdrv.dll");
  if (!file_util::PathExists(xps_path)) {
    return false;
  }
  return true;
}

// Returns true if registration/unregistration can be attempted.
bool CanRegister() {
  if (!XpsIsInstalled()) {
    return false;
  }
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    base::IntegrityLevel level = base::INTEGRITY_UNKNOWN;
    if (!GetProcessIntegrityLevel(base::GetCurrentProcessHandle(), &level)) {
      return false;
    }
    if (level != base::HIGH_INTEGRITY) {
      return false;
    }
  }
  return true;
}

// Frees any objects referenced by port_data and sets pointers to NULL.
void CleanupPortData(PortData* port_data) {
  delete port_data->file_path;
  port_data->file_path = NULL;
  if (port_data->printer_handle != NULL) {
    ClosePrinter(port_data->printer_handle);
    port_data->printer_handle = NULL;
  }
  if (port_data->file != NULL) {
    file_util::CloseFile(port_data->file);
    port_data->file = NULL;
  }
}

// Attempts to retrieve the title of the specified print job.
// On success returns TRUE and the first title_chars characters of the job title
// are copied into title.
// On failure returns FALSE and title is unmodified.
bool GetJobTitle(HANDLE printer_handle,
                 DWORD job_id,
                 string16 *title) {
  DCHECK(printer_handle != NULL);
  DCHECK(title != NULL);
  DWORD bytes_needed = 0;
  GetJob(printer_handle, job_id, 1, NULL, 0, &bytes_needed);
  if (bytes_needed == 0) {
    LOG(ERROR) << "Unable to get bytes needed for job info.";
    return false;
  }
  scoped_array<BYTE> buffer(new BYTE[bytes_needed]);
  if (!GetJob(printer_handle,
              job_id,
              1,
              buffer.get(),
              bytes_needed,
              &bytes_needed)) {
    LOG(ERROR) << "Unable to get job info.";
    return false;
  }
  JOB_INFO_1* job_info = reinterpret_cast<JOB_INFO_1*>(buffer.get());
  *title = job_info->pDocument;
  return true;
}

// Handler for the UI functions exported by the port monitor.
// Verifies that a valid parent Window exists and then just displays an
// error message to let the user know that there is no interactive
// configuration.
void HandlePortUi(HWND hwnd, const string16& caption) {
  if (hwnd != NULL && IsWindow(hwnd)) {
    DisplayWindowsMessage(hwnd, CO_E_NOT_SUPPORTED, cloud_print::kPortName);
  }
}

// Launches the Cloud Print dialog in Chrome.
// xps_path references a file to print.
// job_title is the title to be used for the resulting print job.
bool LaunchPrintDialog(const string16& xps_path,
                       const string16& job_title) {
  HANDLE token = NULL;
  if (!OpenThreadToken(GetCurrentThread(),
                      TOKEN_QUERY|TOKEN_DUPLICATE|TOKEN_ASSIGN_PRIMARY,
                      FALSE,
                      &token)) {
    LOG(ERROR) << "Unable to get thread token.";
    return false;
  }
  base::win::ScopedHandle token_scoped(token);
  if (!DuplicateTokenEx(token,
                       TOKEN_QUERY|TOKEN_DUPLICATE|TOKEN_ASSIGN_PRIMARY,
                       NULL,
                       SecurityImpersonation,
                       TokenPrimary,
                       &token)) {
    LOG(ERROR) << "Unable to get primary thread token.";
    return false;
  }
  base::win::ScopedHandle primary_token_scoped(token);
  FilePath chrome_path;
  if (!GetChromeExePath(&chrome_path)) {
    LOG(ERROR) << "Unable to get chrome exe path.";
    return false;
  }
  CommandLine command_line(chrome_path);
  command_line.AppendSwitchPath(switches::kCloudPrintFile,
                                FilePath(xps_path));
  command_line.AppendSwitchNative(switches::kCloudPrintFileType,
                                  kXpsMimeType);
  command_line.AppendSwitchNative(switches::kCloudPrintJobTitle,
                                  job_title);
  command_line.AppendSwitch(switches::kCloudPrintDeleteFile);
  base::LaunchAppAsUser(primary_token_scoped,
                        command_line.command_line_string(),
                        false,
                        NULL);
  return true;
}

// Returns false if the print job is being run in a context
// that shouldn't be launching Chrome.
bool ValidateCurrentUser() {
  wchar_t user_name[UNLEN + 1] = L"";
  DWORD name_size = sizeof(user_name);
  GetUserName(user_name, &name_size);
  LOG(INFO) << "Username is " << user_name << ".";
  // TODO(abodenha@chromium.org) Return false if running as session 0 or
  // as local system.
  return true;
}
}  // namespace

bool GetChromeExePath(FilePath* chrome_path) {
  base::win::RegKey app_path_key(HKEY_CURRENT_USER,
                                 kChromePathRegKey,
                                 KEY_READ);
  DCHECK(chrome_path != NULL);
  std::wstring reg_data;
  if (SUCCEEDED(app_path_key.ReadValue(kChromePathRegValue,
                                       &reg_data))) {
    if (!reg_data.empty() && file_util::PathExists(FilePath(reg_data))) {
      *chrome_path = FilePath(reg_data);
      return true;
    }
  }
  // First check %localappdata%\google\chrome\application\chrome.exe
  FilePath path;
  PathService::Get(base::DIR_LOCAL_APP_DATA, &path);
  path = path.Append(kChromeExePath);
  if (file_util::PathExists(path)) {
    *chrome_path = FilePath(path.value());
    return true;
  }

  // Chrome doesn't appear to be installed per user.
  // Now check %programfiles(x86)%\google\chrome\application
  PathService::Get(base::DIR_PROGRAM_FILESX86, &path);
  path = path.Append(kChromeExePath);
  if (file_util::PathExists(path)) {
    *chrome_path = FilePath(path.value());
    return true;
  }
  LOG(WARNING) << kChromeExePath << " not found.";
  return false;
}

BOOL WINAPI Monitor2EnumPorts(HANDLE,
                              wchar_t*,
                              DWORD level,
                              BYTE*  ports,
                              DWORD   ports_size,
                              DWORD* needed_bytes,
                              DWORD* returned) {
  LOG(INFO) << "Monitor2EnumPorts";

  if (needed_bytes == NULL) {
    LOG(ERROR) << "needed_bytes should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  if (level == 1) {
    *needed_bytes = sizeof(PORT_INFO_1);
  } else if (level == 2) {
    *needed_bytes = sizeof(PORT_INFO_2);
  } else {
    LOG(ERROR) << "Level "  << level << "is not supported.";
    SetLastError(ERROR_INVALID_LEVEL);
    return FALSE;
  }
  *needed_bytes += static_cast<DWORD>(cloud_print::kPortNameSize);
  if (ports_size < *needed_bytes) {
    LOG(WARNING) << *needed_bytes << " bytes are required.  Only "
                 << ports_size << " were allocated.";
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return FALSE;
  }
  if (ports == NULL) {
    LOG(ERROR) << "ports should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  if (returned == NULL) {
    LOG(ERROR) << "returned should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  // Windows expects any strings refernced by PORT_INFO_X structures to
  // appear at the END of the buffer referenced by ports.  Placing
  // strings immediately after the PORT_INFO_X structure will cause
  // EnumPorts to fail until the spooler is restarted.
  // This is NOT mentioned in the documentation.
  wchar_t* string_target =
      reinterpret_cast<wchar_t*>(ports + ports_size -
      cloud_print::kPortNameSize);
  if (level == 1) {
    PORT_INFO_1* port_info = reinterpret_cast<PORT_INFO_1*>(ports);
    port_info->pName = string_target;
    StringCbCopy(port_info->pName,
                 cloud_print::kPortNameSize,
                 cloud_print::kPortName);
  } else {
    PORT_INFO_2* port_info = reinterpret_cast<PORT_INFO_2*>(ports);
    port_info->pPortName = string_target;
    StringCbCopy(port_info->pPortName,
                 cloud_print::kPortNameSize,
                 cloud_print::kPortName);
    port_info->pMonitorName = NULL;
    port_info->pDescription = NULL;
    port_info->fPortType = PORT_TYPE_WRITE;
    port_info->Reserved = 0;
  }
  *returned = 1;
  return TRUE;
}

BOOL WINAPI Monitor2OpenPort(HANDLE, wchar_t*, HANDLE* handle) {
  LOG(INFO) << "Monitor2OpenPort";

  PortData* port_data =
      reinterpret_cast<PortData*>(GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT,
                                              sizeof(PortData)));
  if (port_data == NULL) {
    LOG(ERROR) << "Unable to allocate memory for internal structures.";
    SetLastError(E_OUTOFMEMORY);
    return FALSE;
  }
  if (handle == NULL) {
    LOG(ERROR) << "handle should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  *handle = (HANDLE)port_data;
  return TRUE;
}

BOOL WINAPI Monitor2StartDocPort(HANDLE port_handle,
                                 wchar_t* printer_name,
                                 DWORD job_id,
                                 DWORD,
                                 BYTE*) {
  LOG(INFO) << "Monitor2StartDocPort";
  if (port_handle == NULL) {
    LOG(ERROR) << "port_handle should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  if (printer_name == NULL) {
    LOG(ERROR) << "printer_name should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  if (!ValidateCurrentUser()) {
    // TODO(abodenha@chromium.org) Abort the print job.
    return FALSE;
  }
  PortData* port_data = reinterpret_cast<PortData*>(port_handle);
  port_data->job_id = job_id;
  if (!OpenPrinter(printer_name, &(port_data->printer_handle), NULL)) {
    LOG(WARNING) << "Unable to open printer " << printer_name << ".";
    // We can continue without a handle to the printer.
    // It just means we can't get the job title or tell the spooler that
    // the print job is complete.
    // This is the normal flow during a unit test.
    port_data->printer_handle = NULL;
  }
  FilePath app_data;
  port_data->file_path = new FilePath();
  if (port_data->file_path == NULL) {
    LOG(ERROR) << "Unable to allocate memory for internal structures.";
    SetLastError(E_OUTOFMEMORY);
    return FALSE;
  }
  bool result = PathService::Get(base::DIR_LOCAL_APP_DATA_LOW, &app_data);
  file_util::CreateTemporaryFileInDir(app_data, port_data->file_path);
  port_data->file = file_util::OpenFile(*(port_data->file_path), "wb+");
  if (port_data->file == NULL) {
    LOG(ERROR) << "Error opening file " << port_data->file_path << ".";
    return FALSE;
  }

  return TRUE;
}

BOOL WINAPI Monitor2WritePort(HANDLE port_handle,
                              BYTE* buffer,
                              DWORD buffer_size,
                              DWORD* bytes_written) {
  LOG(INFO) << "Monitor2WritePort";
  PortData* port_data = reinterpret_cast<PortData*>(port_handle);
  if (!ValidateCurrentUser()) {
    // TODO(abodenha@chromium.org) Abort the print job.
    return FALSE;
  }
  *bytes_written =
      static_cast<DWORD>(fwrite(buffer, 1, buffer_size, port_data->file));
  if (*bytes_written > 0) {
    return TRUE;
  } else {
    return FALSE;
  }
}

BOOL WINAPI Monitor2ReadPort(HANDLE, BYTE*, DWORD, DWORD* read_bytes) {
  LOG(INFO) << "Monitor2ReadPort";
  LOG(ERROR) << "Read is not supported.";
  *read_bytes = 0;
  SetLastError(ERROR_NOT_SUPPORTED);
  return FALSE;
}

BOOL WINAPI Monitor2EndDocPort(HANDLE port_handle) {
  LOG(INFO) << "Monitor2EndDocPort";
  if (!ValidateCurrentUser()) {
    // TODO(abodenha@chromium.org) Abort the print job.
    return FALSE;
  }
  PortData* port_data = reinterpret_cast<PortData*>(port_handle);
  if (port_data == NULL) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  if (port_data->file != NULL) {
    file_util::CloseFile(port_data->file);
    port_data->file = NULL;
    string16 job_title;
    if (port_data->printer_handle != NULL) {
      GetJobTitle(port_data->printer_handle,
                  port_data->job_id,
                  &job_title);
    }
    LaunchPrintDialog(port_data->file_path->value().c_str(),
                      job_title);
  }
  if (port_data->printer_handle != NULL) {
    // Tell the spooler that the job is complete.
    SetJob(port_data->printer_handle,
           port_data->job_id,
           0,
           NULL,
           JOB_CONTROL_SENT_TO_PRINTER);
  }
  CleanupPortData(port_data);
  // Return success even if we can't display the dialog.
  // TODO(abodenha@chromium.org) Come up with a better way of handling
  // this situation.
  return TRUE;
}

BOOL WINAPI Monitor2ClosePort(HANDLE port_handle) {
  LOG(INFO) << "Monitor2ClosePort";
  if (port_handle == NULL) {
    LOG(ERROR) << "port_handle should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  PortData* port_data = reinterpret_cast<PortData*>(port_handle);
  CleanupPortData(port_data);
  GlobalFree(port_handle);
  return TRUE;
}

VOID WINAPI Monitor2Shutdown(HANDLE monitor_handle) {
  LOG(INFO) << "Monitor2Shutdown";
  if (monitor_handle != NULL) {
    MonitorData* monitor_data =
      reinterpret_cast<MonitorData*>(monitor_handle);
    delete monitor_data->at_exit_manager;
    GlobalFree(monitor_handle);
  }
}

BOOL WINAPI Monitor2XcvOpenPort(HANDLE,
                                const wchar_t*,
                                ACCESS_MASK granted_access,
                                HANDLE* handle) {
  LOG(INFO) << "Monitor2XcvOpenPort";
  if (handle == NULL) {
    LOG(ERROR) << "handle should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  XcvUiData* xcv_data;
  xcv_data = reinterpret_cast<XcvUiData*>(GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT,
                                                      sizeof(XcvUiData)));
  if (xcv_data == NULL) {
    LOG(ERROR) << "Unable to allocate memory for internal structures.";
    SetLastError(E_OUTOFMEMORY);
    return FALSE;
  }
  xcv_data->granted_access = granted_access;
  *handle = (HANDLE)xcv_data;
  return TRUE;
}

DWORD WINAPI Monitor2XcvDataPort(HANDLE xcv_handle,
                                 const wchar_t* data_name,
                                 BYTE*,
                                 DWORD,
                                 BYTE* output_data,
                                 DWORD output_data_bytes,
                                 DWORD* output_data_bytes_needed) {
  LOG(INFO) << "Monitor2XcvDataPort";
  XcvUiData* xcv_data = reinterpret_cast<XcvUiData*>(xcv_handle);
  DWORD ret_val = ERROR_SUCCESS;
  if ((xcv_data->granted_access & SERVER_ACCESS_ADMINISTER) == 0) {
    return ERROR_ACCESS_DENIED;
  }
  if (output_data == NULL || output_data_bytes == 0) {
    return ERROR_INVALID_PARAMETER;
  }
  // We don't handle AddPort or DeletePort since we don't support
  // dynamic creation of ports.
  if (lstrcmp(L"MonitorUI", data_name) == 0) {
    DWORD dll_path_len = 0;
    FilePath dll_path(GetPortMonitorDllName());
    dll_path_len = static_cast<DWORD>(dll_path.value().length());
    if (output_data_bytes_needed != NULL) {
      *output_data_bytes_needed = dll_path_len;
    }
    if (output_data_bytes < dll_path_len) {
        return ERROR_INSUFFICIENT_BUFFER;
    } else {
        ret_val = StringCbCopy(reinterpret_cast<wchar_t*>(output_data),
                               output_data_bytes,
                               dll_path.value().c_str());
    }
  } else {
    return ERROR_INVALID_PARAMETER;
  }
  return ret_val;
}

BOOL WINAPI Monitor2XcvClosePort(HANDLE handle) {
  GlobalFree(handle);
  return TRUE;
}

BOOL WINAPI MonitorUiAddPortUi(const wchar_t*,
                               HWND hwnd,
                               const wchar_t* monitor_name,
                               wchar_t**) {
  HandlePortUi(hwnd, monitor_name);
  return TRUE;
}

BOOL WINAPI MonitorUiConfigureOrDeletePortUI(const wchar_t*,
                                             HWND hwnd,
                                             const wchar_t* port_name) {
  HandlePortUi(hwnd, port_name);
  return TRUE;
}

}   // namespace cloud_print

MONITOR2* WINAPI InitializePrintMonitor2(MONITORINIT*,
                                         HANDLE* handle) {
  LOG(INFO) << "InitializePrintMonitor2";
  cloud_print::MonitorData* monitor_data =
      reinterpret_cast<cloud_print::MonitorData*>
      (GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT, sizeof(cloud_print::MonitorData)));
  if (monitor_data == NULL) {
    return NULL;
  }
  if (handle != NULL) {
    *handle = (HANDLE)monitor_data;
    #ifndef UNIT_TEST
    // Unit tests set up their own AtExitManager
    monitor_data->at_exit_manager = new base::AtExitManager();
    #endif
  } else {
    SetLastError(ERROR_INVALID_PARAMETER);
    return NULL;
  }
  return &cloud_print::g_monitor_2;
}

MONITORUI* WINAPI InitializePrintMonitorUI(void) {
  LOG(INFO) << "InitializePrintMonitorUI";
  return &cloud_print::g_monitor_ui;
}

HRESULT WINAPI DllRegisterServer(void) {
  base::AtExitManager at_exit_manager;
  if (!cloud_print::CanRegister()) {
    return E_ACCESSDENIED;
  }
  MONITOR_INFO_2 monitor_info = {0};
  // YUCK!!!  I can either copy the constant, const_cast, or define my own
  // MONITOR_INFO_2 that will take const strings.
  FilePath dll_path(cloud_print::GetPortMonitorDllName());
  monitor_info.pDLLName = const_cast<LPWSTR>(dll_path.value().c_str());
  monitor_info.pName = const_cast<LPWSTR>(dll_path.value().c_str());
  if (AddMonitor(NULL, 2, reinterpret_cast<BYTE*>(&monitor_info))) {
    return S_OK;
  }
  return cloud_print::GetLastHResult();
}

HRESULT WINAPI DllUnregisterServer(void) {
  base::AtExitManager at_exit_manager;
  if (!cloud_print::CanRegister()) {
    return E_ACCESSDENIED;
  }
  FilePath dll_path(cloud_print::GetPortMonitorDllName());
  if (DeleteMonitor(NULL,
                    NULL,
                    const_cast<LPWSTR>(dll_path.value().c_str()))) {
    return S_OK;
  }
  return cloud_print::GetLastHResult();
}
