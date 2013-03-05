// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/virtual_driver/win/port_monitor/port_monitor.h"

#include <lmcons.h>
#include <shellapi.h>
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
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "cloud_print/virtual_driver/virtual_driver_switches.h"
#include "cloud_print/virtual_driver/win/port_monitor/spooler_win.h"
#include "cloud_print/virtual_driver/win/virtual_driver_consts.h"
#include "cloud_print/virtual_driver/win/virtual_driver_helpers.h"

namespace cloud_print {

namespace {

const wchar_t kIePath[] = L"Internet Explorer\\iexplore.exe";

const char kChromeInstallUrl[] =
    "http://google.com/cloudprint/learn/chrome.html";

const wchar_t kCloudPrintRegKey[] = L"Software\\Google\\CloudPrint";

const wchar_t kXpsMimeType[] = L"application/vnd.ms-xpsdocument";

const wchar_t kAppDataDir[] = L"Google\\Cloud Printer";

struct MonitorData {
  scoped_ptr<base::AtExitManager> at_exit_manager;
};

struct PortData {
  PortData() : job_id(0), printer_handle(NULL), file(0) {
  }
  ~PortData() {
    Close();
  }
  void Close() {
    if (printer_handle) {
      ClosePrinter(printer_handle);
      printer_handle = NULL;
    }
    if (file) {
      file_util::CloseFile(file);
      file = NULL;
    }
  }
  DWORD job_id;
  HANDLE printer_handle;
  FILE* file;
  base::FilePath file_path;
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

base::FilePath GetAppDataDir() {
  base::FilePath file_path;
  base::win::Version version = base::win::GetVersion();
  int path_id = (version >= base::win::VERSION_VISTA) ?
                base::DIR_LOCAL_APP_DATA_LOW : base::DIR_LOCAL_APP_DATA;
  if (!PathService::Get(path_id, &file_path)) {
    LOG(ERROR) << "Can't get DIR_LOCAL_APP_DATA";
    return base::FilePath();
  }
  return file_path.Append(kAppDataDir);
}

// Delete files which where not deleted by chrome.
void DeleteLeakedFiles(const base::FilePath& dir) {
  using file_util::FileEnumerator;
  base::Time delete_before = base::Time::Now() - base::TimeDelta::FromDays(1);
  FileEnumerator enumerator(dir, false, FileEnumerator::FILES);
  for (base::FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    FileEnumerator::FindInfo info;
    enumerator.GetFindInfo(&info);
    if (FileEnumerator::GetLastModifiedTime(info) < delete_before)
      file_util::Delete(file_path, false);
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

// Gets the primary token for the user that submitted the print job.
bool GetUserToken(HANDLE* primary_token) {
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
                        primary_token)) {
    LOG(ERROR) << "Unable to get primary thread token.";
    return false;
  }
  return true;
}

// Launches the Cloud Print dialog in Chrome.
// xps_path references a file to print.
// job_title is the title to be used for the resulting print job.
bool LaunchPrintDialog(const base::FilePath& xps_path,
                       const string16& job_title) {
  HANDLE token = NULL;
  if (!GetUserToken(&token)) {
    LOG(ERROR) << "Unable to get user token.";
    return false;
  }
  base::win::ScopedHandle primary_token_scoped(token);

  base::FilePath chrome_path = GetChromeExePath();
  if (chrome_path.empty()) {
    LOG(ERROR) << "Unable to get chrome exe path.";
    return false;
  }

  CommandLine command_line(chrome_path);

  base::FilePath chrome_profile = GetChromeProfilePath();
  if (!chrome_profile.empty()) {
    command_line.AppendSwitchPath(switches::kCloudPrintUserDataDir,
                                  chrome_profile);
  }

  command_line.AppendSwitchPath(switches::kCloudPrintFile,
                                xps_path);
  command_line.AppendSwitchNative(switches::kCloudPrintFileType,
                                  kXpsMimeType);
  command_line.AppendSwitchNative(switches::kCloudPrintJobTitle,
                                  job_title);
  command_line.AppendSwitch(switches::kCloudPrintDeleteFile);
  base::LaunchOptions options;
  options.as_user = primary_token_scoped;
  base::LaunchProcess(command_line, options, NULL);
  return true;
}

// Launches a page to allow the user to download chrome.
// TODO(abodenha@chromium.org) Point to a custom page explaining what's wrong
// rather than the generic chrome download page.  See
// http://code.google.com/p/chromium/issues/detail?id=112019
void LaunchChromeDownloadPage() {
  if (kIsUnittest)
    return;
  HANDLE token = NULL;
  if (!GetUserToken(&token)) {
    LOG(ERROR) << "Unable to get user token.";
    return;
  }
  base::win::ScopedHandle token_scoped(token);

  base::FilePath ie_path;
  PathService::Get(base::DIR_PROGRAM_FILESX86, &ie_path);
  ie_path = ie_path.Append(kIePath);
  CommandLine command_line(ie_path);
  command_line.AppendArg(kChromeInstallUrl);

  base::LaunchOptions options;
  options.as_user = token_scoped;
  base::LaunchProcess(command_line, options, NULL);
}

// Returns false if the print job is being run in a context
// that shouldn't be launching Chrome.
bool ValidateCurrentUser() {
  HANDLE token = NULL;
  if (!GetUserToken(&token)) {
    // If we can't get the token we're probably not impersonating
    // the user, so validation should fail.
    return false;
  }
  base::win::ScopedHandle token_scoped(token);

  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    DWORD session_id = 0;
    DWORD dummy;
    if (!GetTokenInformation(token_scoped,
                             TokenSessionId,
                             reinterpret_cast<void *>(&session_id),
                             sizeof(DWORD),
                             &dummy)) {
      return false;
    }
    if (session_id == 0) {
      return false;
    }
  }
  return true;
}
}  // namespace

base::FilePath ReadPathFromRegistry(HKEY root, const wchar_t* path_name) {
  base::win::RegKey gcp_key(HKEY_CURRENT_USER, kCloudPrintRegKey, KEY_READ);
  string16 data;
  if (SUCCEEDED(gcp_key.ReadValue(path_name, &data)) &&
      file_util::PathExists(base::FilePath(data))) {
    return base::FilePath(data);
  }
  return base::FilePath();
}

base::FilePath ReadPathFromAnyRegistry(const wchar_t* path_name) {
  base::FilePath result = ReadPathFromRegistry(HKEY_CURRENT_USER, path_name);
  if (!result.empty())
    return result;
  return ReadPathFromRegistry(HKEY_LOCAL_MACHINE, path_name);
}

base::FilePath GetChromeExePath() {
  base::FilePath path = ReadPathFromAnyRegistry(kChromeExePathRegValue);
  if (!path.empty())
    return path;
  return chrome_launcher_support::GetAnyChromePath();
}

base::FilePath GetChromeProfilePath() {
  base::FilePath path = ReadPathFromAnyRegistry(kChromeProfilePathRegValue);
  if (!path.empty() && file_util::DirectoryExists(path))
    return path;
  return base::FilePath();
}

BOOL WINAPI Monitor2EnumPorts(HANDLE,
                              wchar_t*,
                              DWORD level,
                              BYTE*  ports,
                              DWORD   ports_size,
                              DWORD* needed_bytes,
                              DWORD* returned) {
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
  PortData* port_data = new PortData();
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
  const wchar_t* kUsageKey = L"dr";
  // Set appropriate key to 1 to let Omaha record usage.
  base::win::RegKey key;
  if (key.Create(HKEY_CURRENT_USER, kGoogleUpdateClientStateKey,
                 KEY_SET_VALUE) != ERROR_SUCCESS ||
      key.WriteValue(kUsageKey, L"1") != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to set usage key";
  }
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
  base::FilePath& file_path = port_data->file_path;
  base::FilePath app_data_dir = GetAppDataDir();
  if (app_data_dir.empty())
    return FALSE;
  DeleteLeakedFiles(app_data_dir);
  if (!file_util::CreateDirectory(app_data_dir) ||
      !file_util::CreateTemporaryFileInDir(app_data_dir, &file_path)) {
    LOG(ERROR) << "Can't create temporary file in " << app_data_dir.value();
    return FALSE;
  }
  port_data->file = file_util::OpenFile(file_path, "wb+");
  if (port_data->file == NULL) {
    LOG(ERROR) << "Error opening file " << file_path.value() << ".";
    return FALSE;
  }
  return TRUE;
}

BOOL WINAPI Monitor2WritePort(HANDLE port_handle,
                              BYTE* buffer,
                              DWORD buffer_size,
                              DWORD* bytes_written) {
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
  LOG(ERROR) << "Read is not supported.";
  *read_bytes = 0;
  SetLastError(ERROR_NOT_SUPPORTED);
  return FALSE;
}

BOOL WINAPI Monitor2EndDocPort(HANDLE port_handle) {
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
    bool delete_file = true;
    int64 file_size = 0;
    file_util::GetFileSize(port_data->file_path, &file_size);
    if (file_size > 0) {
      string16 job_title;
      if (port_data->printer_handle != NULL) {
        GetJobTitle(port_data->printer_handle,
                    port_data->job_id,
                    &job_title);
      }
      if (!LaunchPrintDialog(port_data->file_path, job_title)) {
        LaunchChromeDownloadPage();
      } else {
        delete_file = false;
      }
    }
    if (delete_file)
      file_util::Delete(port_data->file_path, false);
  }
  if (port_data->printer_handle != NULL) {
    // Tell the spooler that the job is complete.
    SetJob(port_data->printer_handle,
           port_data->job_id,
           0,
           NULL,
           JOB_CONTROL_SENT_TO_PRINTER);
  }
  port_data->Close();
  // Return success even if we can't display the dialog.
  // TODO(abodenha@chromium.org) Come up with a better way of handling
  // this situation.
  return TRUE;
}

BOOL WINAPI Monitor2ClosePort(HANDLE port_handle) {
  if (port_handle == NULL) {
    LOG(ERROR) << "port_handle should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  PortData* port_data = reinterpret_cast<PortData*>(port_handle);
  delete port_data;
  return TRUE;
}

VOID WINAPI Monitor2Shutdown(HANDLE monitor_handle) {
  if (monitor_handle != NULL) {
    MonitorData* monitor_data =
      reinterpret_cast<MonitorData*>(monitor_handle);
    delete monitor_handle;
  }
}

BOOL WINAPI Monitor2XcvOpenPort(HANDLE,
                                const wchar_t*,
                                ACCESS_MASK granted_access,
                                HANDLE* handle) {
  if (handle == NULL) {
    LOG(ERROR) << "handle should not be NULL.";
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  XcvUiData* xcv_data = new XcvUiData();
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
    base::FilePath dll_path(GetPortMonitorDllName());
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
  XcvUiData* xcv_data = reinterpret_cast<XcvUiData*>(handle);
  delete xcv_data;
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
  cloud_print::MonitorData* monitor_data = new cloud_print::MonitorData;
  if (monitor_data == NULL) {
    return NULL;
  }
  if (handle != NULL) {
    *handle = (HANDLE)monitor_data;
    if (!cloud_print::kIsUnittest) {
      // Unit tests set up their own AtExitManager
      monitor_data->at_exit_manager.reset(new base::AtExitManager());
      // Single spooler.exe handles verbose users.
      PathService::DisableCache();
    }
  } else {
    SetLastError(ERROR_INVALID_PARAMETER);
    return NULL;
  }
  return &cloud_print::g_monitor_2;
}

MONITORUI* WINAPI InitializePrintMonitorUI(void) {
  return &cloud_print::g_monitor_ui;
}

