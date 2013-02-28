// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/chrome_frame_test_utils.h"

#include <atlapp.h>
#include <atlmisc.h>
#include <iepmapi.h>
#include <sddl.h>
#include <shlobj.h>
#include <winsock2.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/utils.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace chrome_frame_test {

const wchar_t kCrashServicePipeName[] = L"\\\\.\\pipe\\ChromeCrashServices";

const DWORD kCrashServicePipeDesiredAccess = FILE_READ_DATA |
                                             FILE_WRITE_DATA |
                                             FILE_WRITE_ATTRIBUTES;

const DWORD kCrashServicePipeFlagsAndAttributes = SECURITY_IDENTIFICATION |
                                                  SECURITY_SQOS_PRESENT;
const int kCrashServiceDetectTimeoutMs = 500;
const int kCrashServiceStartupTimeoutMs = 1000;

const wchar_t kIEImageName[] = L"iexplore.exe";
const wchar_t kIEBrokerImageName[] = L"ieuser.exe";
const char kChromeImageName[] = "chrome.exe";
const wchar_t kIEProfileName[] = L"iexplore";
const wchar_t kChromeLauncher[] = L"chrome_launcher.exe";

#ifndef NDEBUG
const base::TimeDelta kChromeFrameLongNavigationTimeout =
    base::TimeDelta::FromSeconds(30);
const base::TimeDelta kChromeFrameVeryLongNavigationTimeout =
    base::TimeDelta::FromSeconds(90);
#else
const base::TimeDelta kChromeFrameLongNavigationTimeout =
    base::TimeDelta::FromSeconds(10);
const base::TimeDelta kChromeFrameVeryLongNavigationTimeout =
    base::TimeDelta::FromSeconds(30);
#endif

// Callback function for EnumThreadWindows.
BOOL CALLBACK CloseWindowsThreadCallback(HWND hwnd, LPARAM param) {
  int& count = *reinterpret_cast<int*>(param);
  if (IsWindowVisible(hwnd)) {
    if (IsWindowEnabled(hwnd)) {
      DWORD results = 0;
      if (!::SendMessageTimeout(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0, SMTO_BLOCK,
                                10000, &results)) {
        LOG(WARNING) << "Window hung: " << base::StringPrintf(L"%08X", hwnd);
      }
      count++;
    } else {
      LOG(WARNING) << "Skipping disabled window: "
                   << base::StringPrintf(L"%08X", hwnd);
    }
  }
  return TRUE;  // continue enumeration
}

// Attempts to close all non-child, visible windows on the given thread.
// The return value is the number of visible windows a close request was
// sent to.
int CloseVisibleTopLevelWindowsOnThread(DWORD thread_id) {
  int window_close_attempts = 0;
  EnumThreadWindows(thread_id, CloseWindowsThreadCallback,
                    reinterpret_cast<LPARAM>(&window_close_attempts));
  return window_close_attempts;
}

// Enumerates the threads of a process and attempts to close visible non-child
// windows on all threads of the process.
// The return value is the number of visible windows a close request was
// sent to.
int CloseVisibleWindowsOnAllThreads(HANDLE process) {
  DWORD process_id = ::GetProcessId(process);
  if (process_id == 0) {
    NOTREACHED();
    return 0;
  }

  base::win::ScopedHandle snapshot(
      CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0));
  if (!snapshot.IsValid()) {
    NOTREACHED();
    return 0;
  }

  int window_close_attempts = 0;
  THREADENTRY32 te = { sizeof(THREADENTRY32) };
  if (Thread32First(snapshot, &te)) {
    do {
      if (RTL_CONTAINS_FIELD(&te, te.dwSize, th32OwnerProcessID) &&
          te.th32OwnerProcessID == process_id) {
        window_close_attempts +=
            CloseVisibleTopLevelWindowsOnThread(te.th32ThreadID);
      }
      te.dwSize = sizeof(te);
    } while (Thread32Next(snapshot, &te));
  }

  return window_close_attempts;
}

std::wstring GetExecutableAppPath(const std::wstring& file) {
  std::wstring kAppPathsKey =
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\";

  std::wstring app_path;
  base::win::RegKey key(HKEY_LOCAL_MACHINE, (kAppPathsKey + file).c_str(),
                        KEY_READ);
  if (key.Handle()) {
    key.ReadValue(NULL, &app_path);
  }

  return app_path;
}

std::wstring FormatCommandForApp(const std::wstring& exe_name,
                                 const std::wstring& argument) {
  std::wstring reg_path(
      base::StringPrintf(L"Applications\\%ls\\shell\\open\\command",
                         exe_name.c_str()));
  base::win::RegKey key(HKEY_CLASSES_ROOT, reg_path.c_str(), KEY_READ);

  std::wstring command;
  if (key.Handle()) {
    key.ReadValue(NULL, &command);
    int found = command.find(L"%1");
    if (found >= 0) {
      command.replace(found, 2, argument);
    }
  }
  return command;
}

base::ProcessHandle LaunchExecutable(const std::wstring& executable,
                                     const std::wstring& argument) {
  base::ProcessHandle process = NULL;
  std::wstring path = GetExecutableAppPath(executable);
  if (path.empty()) {
    path = FormatCommandForApp(executable, argument);
    if (path.empty()) {
      LOG(ERROR) << "Failed to find executable: " << executable;
    } else {
      CommandLine cmdline = CommandLine::FromString(path);
      if (!base::LaunchProcess(cmdline, base::LaunchOptions(), &process)) {
        LOG(ERROR) << "LaunchProcess failed: " << ::GetLastError();
      }
    }
  } else {
    CommandLine cmdline((base::FilePath(path)));
    cmdline.AppendArgNative(argument);
    if (!base::LaunchProcess(cmdline, base::LaunchOptions(), &process)) {
      LOG(ERROR) << "LaunchProcess failed: " << ::GetLastError();
    }
  }
  return process;
}

base::ProcessHandle LaunchChrome(const std::wstring& url,
                                 const base::FilePath& user_data_dir) {
  base::FilePath path;
  PathService::Get(base::DIR_MODULE, &path);
  path = path.AppendASCII(kChromeImageName);

  CommandLine cmd(path);
  cmd.AppendSwitch(switches::kNoFirstRun);
  if (!user_data_dir.empty())
    cmd.AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  cmd.AppendArgNative(url);

  base::ProcessHandle process = NULL;
  base::LaunchProcess(cmd, base::LaunchOptions(), &process);
  return process;
}

base::ProcessHandle LaunchIEOnVista(const std::wstring& url) {
  typedef HRESULT (WINAPI* IELaunchURLPtr)(const wchar_t* url,
                                           PROCESS_INFORMATION* pi,
                                           VOID* info);

  IELaunchURLPtr launch;
  PROCESS_INFORMATION pi = {0};
  IELAUNCHURLINFO  info = {sizeof info, 0};
  HMODULE h = LoadLibrary(L"ieframe.dll");
  if (!h) {
    LOG(ERROR) << "Failed to load ieframe.dll: " << ::GetLastError();
    return NULL;
  }
  launch = reinterpret_cast<IELaunchURLPtr>(GetProcAddress(h, "IELaunchURL"));
  CHECK(launch);
  HRESULT hr = launch(url.c_str(), &pi, &info);
  FreeLibrary(h);
  if (SUCCEEDED(hr)) {
    CloseHandle(pi.hThread);
  } else {
    LOG(ERROR) << base::StringPrintf("IELaunchURL failed: 0x%08X", hr);
  }
  return pi.hProcess;
}

base::ProcessHandle LaunchIE(const std::wstring& url) {
  if (GetInstalledIEVersion() >= IE_8) {
    chrome_frame_test::ClearIESessionHistory();
  }

  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    return LaunchIEOnVista(url);
  }
  return LaunchExecutable(kIEImageName, url);
}

int CloseAllIEWindows() {
  int ret = 0;

  base::win::ScopedComPtr<IShellWindows> windows;
  HRESULT hr = ::CoCreateInstance(__uuidof(ShellWindows), NULL, CLSCTX_ALL,
      IID_IShellWindows, reinterpret_cast<void**>(windows.Receive()));
  DCHECK(SUCCEEDED(hr));

  if (SUCCEEDED(hr)) {
    long count = 0;  // NOLINT
    windows->get_Count(&count);
    VARIANT i = { VT_I4 };
    for (i.lVal = 0; i.lVal < count; ++i.lVal) {
      base::win::ScopedComPtr<IDispatch> folder;
      windows->Item(i, folder.Receive());
      if (folder != NULL) {
        base::win::ScopedComPtr<IWebBrowser2> browser;
        if (SUCCEEDED(browser.QueryFrom(folder))) {
          bool is_ie = true;
          HWND window = NULL;
          // Check the class of the browser window to make sure we only close
          // IE windows.
          if (browser->get_HWND(reinterpret_cast<SHANDLE_PTR*>(&window))) {
            wchar_t class_name[MAX_PATH];
            if (::GetClassName(window, class_name, arraysize(class_name))) {
              is_ie = _wcsicmp(class_name, L"IEFrame") == 0;
            }
          }
          if (is_ie) {
            browser->Quit();
            ++ret;
          }
        }
      }
    }
  }

  return ret;
}


LowIntegrityToken::LowIntegrityToken() : impersonated_(false) {
}

LowIntegrityToken::~LowIntegrityToken() {
  RevertToSelf();
}

BOOL LowIntegrityToken::RevertToSelf() {
  BOOL ok = TRUE;
  if (impersonated_) {
    DCHECK(IsImpersonated());
    ok = ::RevertToSelf();
    if (ok)
      impersonated_ = false;
  }

  return ok;
}

BOOL LowIntegrityToken::Impersonate() {
  DCHECK(!impersonated_);
  DCHECK(!IsImpersonated());
  HANDLE process_token_handle = NULL;
  BOOL ok = ::OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE,
                               &process_token_handle);
  if (!ok) {
    LOG(ERROR) << "::OpenProcessToken failed: " << GetLastError();
    return ok;
  }

  base::win::ScopedHandle process_token(process_token_handle);
  // Create impersonation low integrity token.
  HANDLE impersonation_token_handle = NULL;
  ok = ::DuplicateTokenEx(process_token,
      TOKEN_QUERY | TOKEN_IMPERSONATE | TOKEN_ADJUST_DEFAULT, NULL,
      SecurityImpersonation, TokenImpersonation, &impersonation_token_handle);
  if (!ok) {
    LOG(ERROR) << "::DuplicateTokenEx failed: " << GetLastError();
    return ok;
  }

  // TODO(stoyan): sandbox/win/src/restricted_token_utils.cc has
  // SetTokenIntegrityLevel function already.
  base::win::ScopedHandle impersonation_token(impersonation_token_handle);
  PSID integrity_sid = NULL;
  TOKEN_MANDATORY_LABEL tml = {0};
  ok = ::ConvertStringSidToSid(SDDL_ML_LOW, &integrity_sid);
  if (!ok) {
    LOG(ERROR) << "::ConvertStringSidToSid failed: " << GetLastError();
    return ok;
  }

  tml.Label.Attributes = SE_GROUP_INTEGRITY | SE_GROUP_INTEGRITY_ENABLED;
  tml.Label.Sid = integrity_sid;
  ok = ::SetTokenInformation(impersonation_token, TokenIntegrityLevel,
      &tml, sizeof(tml) + ::GetLengthSid(integrity_sid));
  ::LocalFree(integrity_sid);
  if (!ok) {
    LOG(ERROR) << "::SetTokenInformation failed: " << GetLastError();
    return ok;
  }

  // Switch current thread to low integrity.
  ok = ::ImpersonateLoggedOnUser(impersonation_token);
  if (ok) {
    impersonated_ = true;
  } else {
    LOG(ERROR) << "::ImpersonateLoggedOnUser failed: " << GetLastError();
  }

  return ok;
}

bool LowIntegrityToken::IsImpersonated() {
  HANDLE token = NULL;
  if (!::OpenThreadToken(::GetCurrentThread(), 0, false, &token) &&
      ::GetLastError() != ERROR_NO_TOKEN) {
    return true;
  }

  if (token)
    ::CloseHandle(token);

  return false;
}

HRESULT LaunchIEAsComServer(IWebBrowser2** web_browser) {
  if (!web_browser)
    return E_INVALIDARG;

  if (GetInstalledIEVersion() >= IE_8) {
    chrome_frame_test::ClearIESessionHistory();
  }

  AllowSetForegroundWindow(ASFW_ANY);

  HRESULT hr = S_OK;
  DWORD cocreate_flags = CLSCTX_LOCAL_SERVER;
  chrome_frame_test::LowIntegrityToken token;
  base::IntegrityLevel integrity_level = base::INTEGRITY_UNKNOWN;
  // Vista has a bug which manifests itself when a medium integrity process
  // launches a COM server like IE which runs in protected mode due to UAC.
  // This causes the IWebBrowser2 interface which is returned to be useless,
  // i.e it does not receive any events, etc. Our workaround for this is
  // to impersonate a low integrity token and then launch IE.  Skip this if the
  // tests are running at high integrity, since the workaround results in the
  // medium-integrity broker exiting, and the low-integrity IE is therefore
  // unable to get chrome_launcher running at medium integrity.
  if (base::win::GetVersion() == base::win::VERSION_VISTA &&
      GetInstalledIEVersion() == IE_7 &&
      base::GetProcessIntegrityLevel(base::Process::Current().handle(),
                                     &integrity_level) &&
      integrity_level != base::HIGH_INTEGRITY) {
    // Create medium integrity browser that will launch IE broker.
    base::win::ScopedComPtr<IWebBrowser2> medium_integrity_browser;
    hr = medium_integrity_browser.CreateInstance(CLSID_InternetExplorer, NULL,
                                                 CLSCTX_LOCAL_SERVER);
    if (FAILED(hr))
      return hr;
    medium_integrity_browser->Quit();
    // Broker remains alive.
    if (!token.Impersonate()) {
      hr = HRESULT_FROM_WIN32(GetLastError());
      return hr;
    }

    cocreate_flags |= CLSCTX_ENABLE_CLOAKING;
  }

  hr = ::CoCreateInstance(CLSID_InternetExplorer, NULL,
                          cocreate_flags, IID_IWebBrowser2,
                          reinterpret_cast<void**>(web_browser));
  // ~LowIntegrityToken() will switch integrity back to medium.
  return hr;
}

std::wstring GetExeVersion(const std::wstring& exe_path) {
  scoped_ptr<FileVersionInfo> ie_version_info(
      FileVersionInfo::CreateFileVersionInfo(base::FilePath(exe_path)));
  return ie_version_info->product_version();
}

IEVersion GetInstalledIEVersion() {
  std::wstring path(chrome_frame_test::GetExecutableAppPath(kIEImageName));
  std::wstring version(GetExeVersion(path));
  size_t first_dot = version.find(L'.');
  int major_version = 0;
  if (!base::StringToInt(base::StringPiece16(
          version.data(),
          first_dot == std::wstring::npos ? version.size() : first_dot),
                         &major_version)) {
    return IE_UNSUPPORTED;
  }

  switch (major_version) {
    case 6:
      return IE_6;
    case 7:
      return IE_7;
    case 8:
      return IE_8;
    case 9:
      return IE_9;
    case 10:
      return IE_10;
    default:
      break;
  }

  return IE_UNSUPPORTED;
}

base::FilePath GetProfilePathForIE() {
  base::FilePath profile_path;
  // Browsers without IDeleteBrowsingHistory in non-priv mode
  // have their profiles moved into "Temporary Internet Files".
  // The code below basically retrieves the version of IE and computes
  // the profile directory accordingly.
  if (GetInstalledIEVersion() <= IE_7) {
    profile_path = GetIETemporaryFilesFolder();
    profile_path = profile_path.Append(L"Google Chrome Frame");
  } else {
    GetChromeFrameProfilePath(kIEProfileName, &profile_path);
  }
  return profile_path;
}

base::FilePath GetTestDataFolder() {
  base::FilePath test_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &test_dir);
  test_dir = test_dir.Append(FILE_PATH_LITERAL("chrome_frame"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"));
  return test_dir;
}

base::FilePath GetSeleniumTestFolder() {
  base::FilePath test_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &test_dir);
  test_dir = test_dir.Append(FILE_PATH_LITERAL("data"))
      .Append(FILE_PATH_LITERAL("selenium_core"));
  return test_dir;
}

std::wstring GetPathFromUrl(const std::wstring& url) {
  string16 url16 = WideToUTF16(url);
  GURL gurl = GURL(url16);
  if (gurl.has_query()) {
    GURL::Replacements replacements;
    replacements.ClearQuery();
    gurl = gurl.ReplaceComponents(replacements);
  }
  return UTF8ToWide(gurl.PathForRequest());
}

std::wstring GetPathAndQueryFromUrl(const std::wstring& url) {
  string16 url16 = WideToUTF16(url);
  GURL gurl = GURL(url16);
  return UTF8ToWide(gurl.PathForRequest());
}

std::wstring GetClipboardText() {
  string16 text16;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::Clipboard::BUFFER_STANDARD, &text16);
  return UTF16ToWide(text16);
}

void SetClipboardText(const std::wstring& text) {
  ui::ScopedClipboardWriter clipboard_writer(
      ui::Clipboard::GetForCurrentThread(),
      ui::Clipboard::BUFFER_STANDARD);
  clipboard_writer.WriteText(WideToUTF16(text));
}

bool AddCFMetaTag(std::string* html_data) {
  if (!html_data) {
    NOTREACHED();
    return false;
  }
  std::string lower = StringToLowerASCII(*html_data);
  size_t head = lower.find("<head>");
  if (head == std::string::npos) {
    // Add missing head section.
    size_t html = lower.find("<html>");
    if (html != std::string::npos) {
      head = html + strlen("<html>");
      html_data->insert(head, "<head></head>");
    } else {
      LOG(ERROR) << "Meta tag will not be injected "
                 << "because the html tag could not be found";
    }
  }
  if (head != std::string::npos) {
    html_data->insert(
        head + strlen("<head>"),
        "<meta http-equiv=\"x-ua-compatible\" content=\"chrome=1\" />");
  }
  return head != std::string::npos;
}

CloseIeAtEndOfScope::~CloseIeAtEndOfScope() {
  int closed = CloseAllIEWindows();
  LOG_IF(ERROR, closed != 0) << "Closed " << closed << " windows forcefully";
}

// Attempt to connect to a running crash_service instance. Success occurs if we
// can actually connect to the service's pipe or we receive ERROR_PIPE_BUSY.
// Waits up to |timeout_ms| for success. |timeout_ms| may be 0, meaning only try
// once, or negative, meaning wait forever.
bool DetectRunningCrashService(int timeout_ms) {
  // Wait for the crash_service.exe to be ready for clients.
  base::Time start = base::Time::Now();
  base::win::ScopedHandle new_pipe;

  while (true) {
    new_pipe.Set(::CreateFile(kCrashServicePipeName,
                              kCrashServicePipeDesiredAccess,
                              0,  // dwShareMode
                              NULL,  // lpSecurityAttributes
                              OPEN_EXISTING,
                              kCrashServicePipeFlagsAndAttributes,
                              NULL));  // hTemplateFile

    if (new_pipe.IsValid()) {
      return true;
    }

    switch (::GetLastError()) {
      case ERROR_PIPE_BUSY:
        // OK, it exists, let's assume that clients will eventually be able to
        // connect to it.
        return true;
      case ERROR_FILE_NOT_FOUND:
        // Wait a bit longer
        break;
      default:
        DPLOG(WARNING) << "Unexpected error while checking crash_service.exe's "
                       << "pipe.";
        // Go ahead and wait in case it clears up.
        break;
    }

    if (timeout_ms == 0) {
      return false;
    } else if (timeout_ms > 0) {
      base::TimeDelta duration = base::Time::Now() - start;
      if (duration.InMilliseconds() > timeout_ms) {
        return false;
      }
    }

    Sleep(10);
  }
}

base::ProcessHandle StartCrashService() {
  if (DetectRunningCrashService(kCrashServiceDetectTimeoutMs)) {
    VLOG(1) << "crash_service.exe is already running. We will use the "
               "existing process and leave it running after tests complete.";
    return NULL;
  }

  base::FilePath exe_dir;
  if (!PathService::Get(base::DIR_EXE, &exe_dir)) {
    DCHECK(false);
    return NULL;
  }

  base::ProcessHandle crash_service = NULL;

  VLOG(1) << "Starting crash_service.exe so you know if a test crashes!";

  base::FilePath crash_service_path = exe_dir.AppendASCII("crash_service.exe");
  if (!base::LaunchProcess(crash_service_path.value(), base::LaunchOptions(),
                           &crash_service)) {
    LOG(ERROR) << "Couldn't start crash_service.exe";
    return NULL;
  }

  base::Time start = base::Time::Now();

  if (DetectRunningCrashService(kCrashServiceStartupTimeoutMs)) {
    VLOG(1) << "crash_service.exe is ready for clients in "
            << (base::Time::Now() - start).InMilliseconds() << " ms.";
    return crash_service;
  } else {
    LOG(ERROR) << "crash_service.exe failed to accept client connections "
                  "within " << kCrashServiceStartupTimeoutMs << " ms. "
                  "Terminating it now.";

    // First check to see if it's even still running just to minimize the
    // likelihood of spurious error messages from KillProcess.
    if (WAIT_OBJECT_0 != ::WaitForSingleObject(crash_service, 0)) {
      base::KillProcess(crash_service, 0, false);
    }
    return NULL;
  }
}

ScopedVirtualizeHklmAndHkcu::ScopedVirtualizeHklmAndHkcu() {
  override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE, L"hklm_fake");
  override_manager_.OverrideRegistry(HKEY_CURRENT_USER, L"hkcu_fake");
}

ScopedVirtualizeHklmAndHkcu::~ScopedVirtualizeHklmAndHkcu() {
}

void ScopedVirtualizeHklmAndHkcu::RemoveAllOverrides() {
  override_manager_.RemoveAllOverrides();
}

bool KillProcesses(const std::wstring& executable_name, int exit_code,
                   bool wait) {
  bool result = true;
  base::NamedProcessIterator iter(executable_name, NULL);
  while (const base::ProcessEntry* entry = iter.NextProcessEntry()) {
    result &= base::KillProcessById(entry->pid(), exit_code, wait);
  }
  return result;
}

ScopedChromeFrameRegistrar::RegistrationType GetTestBedType() {
  if (GetConfigBool(false, L"PerUserTestBed")) {
    return ScopedChromeFrameRegistrar::PER_USER;
  } else {
    return ScopedChromeFrameRegistrar::SYSTEM_LEVEL;
  }
}

void ClearIESessionHistory() {
  base::FilePath session_history_path;
  if (!PathService::Get(base::DIR_LOCAL_APP_DATA, &session_history_path))
    return;

  session_history_path = session_history_path.AppendASCII("Microsoft");
  session_history_path = session_history_path.AppendASCII("Internet Explorer");
  session_history_path = session_history_path.AppendASCII("Recovery");
  file_util::Delete(session_history_path, true);
}

std::string GetLocalIPv4Address() {
  std::string address;
  net::NetworkInterfaceList nic_list;

  if (!net::GetNetworkList(&nic_list)) {
    LOG(ERROR) << "GetNetworkList failed to look up non-loopback adapters. "
               << "Tests will be run over the loopback adapter, which may "
               << "result in hangs.";
  } else {
    // GetNetworkList only returns 'Up' non-loopback adapters. Select the first
    // IPv4 address found - we should be able to bind/connect over it.
    for (size_t i = 0; i < nic_list.size(); ++i) {
      if (nic_list[i].address.size() != net::kIPv4AddressSize)
        continue;
      char* address_string =
          inet_ntoa(*reinterpret_cast<in_addr*>(&nic_list[i].address[0]));
      DCHECK(address_string != NULL);
      if (address_string != NULL) {
        LOG(INFO) << "HTTP tests will run over " << address_string << ".";
        address.assign(address_string);
        break;
      }
    }
  }

  if (address.empty()) {
    LOG(ERROR) << "Failed to find a non-loopback IP_V4 address. Tests will be "
               << "run over the loopback adapter, which may result in hangs.";
    address.assign("127.0.0.1");
  }

  return address;
}

}  // namespace chrome_frame_test
