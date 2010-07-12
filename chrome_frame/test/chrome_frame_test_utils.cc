// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/chrome_frame_test_utils.h"

#include <atlbase.h>
#include <atlwin.h>
#include <iepmapi.h>
#include <sddl.h>

#include "base/file_version_info.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/registry.h"   // to find IE and firefox
#include "base/scoped_bstr_win.h"
#include "base/scoped_handle.h"
#include "base/scoped_comptr_win.h"
#include "base/utf_string_conversions.h"
#include "base/win_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome_frame/utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_frame_test {

const int kDefaultWaitForIEToTerminateMs = 10 * 1000;

const wchar_t kIEImageName[] = L"iexplore.exe";
const wchar_t kIEBrokerImageName[] = L"ieuser.exe";
const wchar_t kFirefoxImageName[] = L"firefox.exe";
const wchar_t kOperaImageName[] = L"opera.exe";
const wchar_t kSafariImageName[] = L"safari.exe";
const char kChromeImageName[] = "chrome.exe";
const wchar_t kIEProfileName[] = L"iexplore";
const wchar_t kChromeLauncher[] = L"chrome_launcher.exe";
const int kChromeFrameLongNavigationTimeoutInSeconds = 10;

// Callback function for EnumThreadWindows.
BOOL CALLBACK CloseWindowsThreadCallback(HWND hwnd, LPARAM param) {
  int& count = *reinterpret_cast<int*>(param);
  if (IsWindowVisible(hwnd)) {
    if (IsWindowEnabled(hwnd)) {
      DWORD results = 0;
      if (!::SendMessageTimeout(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0, SMTO_BLOCK,
                                10000, &results)) {
        LOG(WARNING) << "Window hung: " << StringPrintf(L"%08X", hwnd);
      }
      count++;
    } else {
      DLOG(WARNING) << "Skipping disabled window: "
                  << StringPrintf(L"%08X", hwnd);
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

  ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0));
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
  RegKey key(HKEY_LOCAL_MACHINE, (kAppPathsKey + file).c_str());
  if (key.Handle()) {
    key.ReadValue(NULL, &app_path);
  }

  return app_path;
}

std::wstring FormatCommandForApp(const std::wstring& exe_name,
                                 const std::wstring& argument) {
  std::wstring reg_path(StringPrintf(L"Applications\\%ls\\shell\\open\\command",
                                     exe_name.c_str()));
  RegKey key(HKEY_CLASSES_ROOT, reg_path.c_str());

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
      if (!base::LaunchApp(cmdline, false, false, &process)) {
        LOG(ERROR) << "LaunchApp failed: " << ::GetLastError();
      }
    }
  } else {
    CommandLine cmdline((FilePath(path)));
    cmdline.AppendLooseValue(argument);
    if (!base::LaunchApp(cmdline, false, false, &process)) {
      LOG(ERROR) << "LaunchApp failed: " << ::GetLastError();
    }
  }
  return process;
}

base::ProcessHandle LaunchFirefox(const std::wstring& url) {
  return LaunchExecutable(kFirefoxImageName, url);
}

base::ProcessHandle LaunchSafari(const std::wstring& url) {
  return LaunchExecutable(kSafariImageName, url);
}

base::ProcessHandle LaunchChrome(const std::wstring& url) {
  FilePath path;
  PathService::Get(base::DIR_MODULE, &path);
  path = path.AppendASCII(kChromeImageName);

  CommandLine cmd(path);
  std::wstring args = L"--";
  args += ASCIIToWide(switches::kNoFirstRun);
  args += L" ";
  args += url;
  cmd.AppendLooseValue(args);

  base::ProcessHandle process = NULL;
  base::LaunchApp(cmd, false, false, &process);
  return process;
}

base::ProcessHandle LaunchOpera(const std::wstring& url) {
  // NOTE: For Opera tests to work it must be configured to start up with
  // a blank page.  There is an command line switch, -nosession, that's supposed
  // to avoid opening up the previous session, but that switch is not working.
  // TODO(tommi): Include a special ini file (opera6.ini) for opera and launch
  //  with our required settings.  This file is by default stored here:
  // "%USERPROFILE%\Application Data\Opera\Opera\profile\opera6.ini"
  return LaunchExecutable(kOperaImageName, url);
}

base::ProcessHandle LaunchIEOnVista(const std::wstring& url) {
  typedef HRESULT (WINAPI* IELaunchURLPtr)(
      const wchar_t* url,
      PROCESS_INFORMATION *pi,
      VOID *info);

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
    LOG(ERROR) << ::StringPrintf("IELaunchURL failed: 0x%08X", hr);
  }
  return pi.hProcess;
}

base::ProcessHandle LaunchIE(const std::wstring& url) {
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA) {
    return LaunchIEOnVista(url);
  } else {
    return LaunchExecutable(kIEImageName, url);
  }
}

int CloseAllIEWindows() {
  int ret = 0;

  ScopedComPtr<IShellWindows> windows;
  HRESULT hr = ::CoCreateInstance(__uuidof(ShellWindows), NULL, CLSCTX_ALL,
      IID_IShellWindows, reinterpret_cast<void**>(windows.Receive()));
  DCHECK(SUCCEEDED(hr));

  if (SUCCEEDED(hr)) {
    long count = 0;  // NOLINT
    windows->get_Count(&count);
    VARIANT i = { VT_I4 };
    for (i.lVal = 0; i.lVal < count; ++i.lVal) {
      ScopedComPtr<IDispatch> folder;
      windows->Item(i, folder.Receive());
      if (folder != NULL) {
        ScopedComPtr<IWebBrowser2> browser;
        if (SUCCEEDED(browser.QueryFrom(folder))) {
          browser->Quit();
          ++ret;
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
    DLOG(ERROR) << "::OpenProcessToken failed: " << GetLastError();
    return ok;
  }

  ScopedHandle process_token(process_token_handle);
  // Create impersonation low integrity token.
  HANDLE impersonation_token_handle = NULL;
  ok = ::DuplicateTokenEx(process_token,
      TOKEN_QUERY | TOKEN_IMPERSONATE | TOKEN_ADJUST_DEFAULT, NULL,
      SecurityImpersonation, TokenImpersonation, &impersonation_token_handle);
  if (!ok) {
    DLOG(ERROR) << "::DuplicateTokenEx failed: " << GetLastError();
    return ok;
  }

  // TODO(stoyan): sandbox/src/restricted_token_utils.cc has
  // SetTokenIntegrityLevel function already.
  ScopedHandle impersonation_token(impersonation_token_handle);
  PSID integrity_sid = NULL;
  TOKEN_MANDATORY_LABEL tml = {0};
  ok = ::ConvertStringSidToSid(SDDL_ML_LOW, &integrity_sid);
  if (!ok) {
    DLOG(ERROR) << "::ConvertStringSidToSid failed: " << GetLastError();
    return ok;
  }

  tml.Label.Attributes = SE_GROUP_INTEGRITY | SE_GROUP_INTEGRITY_ENABLED;
  tml.Label.Sid = integrity_sid;
  ok = ::SetTokenInformation(impersonation_token, TokenIntegrityLevel,
      &tml, sizeof(tml) + ::GetLengthSid(integrity_sid));
  ::LocalFree(integrity_sid);
  if (!ok) {
    DLOG(ERROR) << "::SetTokenInformation failed: " << GetLastError();
    return ok;
  }

  // Switch current thread to low integrity.
  ok = ::ImpersonateLoggedOnUser(impersonation_token);
  if (ok) {
    impersonated_ = true;
  } else {
    DLOG(ERROR) << "::ImpersonateLoggedOnUser failed: " << GetLastError();
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

  AllowSetForegroundWindow(ASFW_ANY);

  HRESULT hr = S_OK;
  DWORD cocreate_flags = CLSCTX_LOCAL_SERVER;
  chrome_frame_test::LowIntegrityToken token;
  // Vista has a bug which manifests itself when a medium integrity process
  // launches a COM server like IE which runs in protected mode due to UAC.
  // This causes the IWebBrowser2 interface which is returned to be useless,
  // i.e it does not receive any events, etc. Our workaround for this is
  // to impersonate a low integrity token and then launch IE.
  if (win_util::GetWinVersion() == win_util::WINVERSION_VISTA &&
      GetInstalledIEVersion() == IE_7) {
    // Create medium integrity browser that will launch IE broker.
    ScopedComPtr<IWebBrowser2> medium_integrity_browser;
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

FilePath GetProfilePath(const std::wstring& profile_name) {
  FilePath profile_path;
  chrome::GetChromeFrameUserDataDirectory(&profile_path);
  return profile_path.Append(profile_name);
}

_ATL_FUNC_INFO WebBrowserEventSink::kNavigateErrorInfo = {
  CC_STDCALL, VT_EMPTY, 5, {
    VT_DISPATCH,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_BOOL | VT_BYREF,
  }
};

_ATL_FUNC_INFO WebBrowserEventSink::kNavigateComplete2Info = {
  CC_STDCALL, VT_EMPTY, 2, {
    VT_DISPATCH,
    VT_VARIANT | VT_BYREF
  }
};

_ATL_FUNC_INFO WebBrowserEventSink::kBeforeNavigate2Info = {
  CC_STDCALL, VT_EMPTY, 7, {
    VT_DISPATCH,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_BOOL | VT_BYREF
  }
};

_ATL_FUNC_INFO WebBrowserEventSink::kNewWindow2Info = {
  CC_STDCALL, VT_EMPTY, 2, {
    VT_DISPATCH | VT_BYREF,
    VT_BOOL | VT_BYREF,
  }
};

_ATL_FUNC_INFO WebBrowserEventSink::kNewWindow3Info = {
  CC_STDCALL, VT_EMPTY, 5, {
    VT_DISPATCH | VT_BYREF,
    VT_BOOL | VT_BYREF,
    VT_UINT,
    VT_BSTR,
    VT_BSTR
  }
};

_ATL_FUNC_INFO WebBrowserEventSink::kVoidMethodInfo = {
    CC_STDCALL, VT_EMPTY, 0, {NULL}};

_ATL_FUNC_INFO WebBrowserEventSink::kDocumentCompleteInfo = {
  CC_STDCALL, VT_EMPTY, 2, {
    VT_DISPATCH,
    VT_VARIANT | VT_BYREF
  }
};

_ATL_FUNC_INFO WebBrowserEventSink::kFileDownloadInfo = {
  CC_STDCALL, VT_EMPTY, 2, {
    VT_BOOL,
    VT_BOOL | VT_BYREF
  }
};

// WebBrowserEventSink member defines
void WebBrowserEventSink::Attach(IDispatch* browser_disp) {
  EXPECT_TRUE(NULL != browser_disp);
  if (browser_disp) {
    EXPECT_HRESULT_SUCCEEDED(web_browser2_.QueryFrom(browser_disp));
    EXPECT_TRUE(S_OK == DispEventAdvise(web_browser2_,
                                        &DIID_DWebBrowserEvents2));
  }
}

void WebBrowserEventSink::Uninitialize() {
  DisconnectFromChromeFrame();
  if (web_browser2_.get()) {
    if (m_dwEventCookie != 0xFEFEFEFE) {
      CoDisconnectObject(this, 0);
      DispEventUnadvise(web_browser2_);
    }

    ScopedHandle process;
    // process_id_to_wait_for_ is set when we receive OnQuit.
    // So, we should only attempt to wait for the browser if we know that
    // the browser is truly quitting and if this instance actually launched
    // the browser.
    if (process_id_to_wait_for_) {
      if (is_main_browser_object_) {
        process.Set(OpenProcess(SYNCHRONIZE, FALSE, process_id_to_wait_for_));
        DLOG_IF(ERROR, !process.IsValid())
            << StringPrintf("OpenProcess failed: %i", ::GetLastError());
      }
      process_id_to_wait_for_ = 0;
    } else {
      DLOG_IF(ERROR, is_main_browser_object_)
          << "Main browser event object did not have a valid the process id.";
      web_browser2_->Quit();
    }

    web_browser2_.Release();

    if (process) {
      DWORD max_wait = kDefaultWaitForIEToTerminateMs;
      while (true) {
        base::Time start = base::Time::Now();
        HANDLE wait_for = process;
        DWORD wait = MsgWaitForMultipleObjects(1, &wait_for, FALSE, max_wait,
                                               QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0 + 1) {
          MSG msg;
          while (PeekMessage(&msg, NULL, 0, 0, TRUE) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
          }
        } else if (wait == WAIT_OBJECT_0) {
          break;
        } else {
          DCHECK(wait == WAIT_TIMEOUT);
          DLOG(ERROR) << "Wait for IE timed out";
          break;
        }

        base::TimeDelta elapsed = base::Time::Now() - start;
        ULARGE_INTEGER ms;
        ms.QuadPart = elapsed.InMilliseconds();
        DCHECK_EQ(ms.HighPart, 0U);
        if (ms.LowPart > max_wait) {
          DLOG(ERROR) << "Wait for IE timed out (2)";
          break;
        } else {
          max_wait -= ms.LowPart;
        }
      }
    }
  }
}

STDMETHODIMP WebBrowserEventSink::OnBeforeNavigate2Internal(
    IDispatch* dispatch, VARIANT* url, VARIANT* flags,
    VARIANT* target_frame_name, VARIANT* post_data, VARIANT* headers,
    VARIANT_BOOL* cancel) {
  DLOG(INFO) << __FUNCTION__
      << StringPrintf("%ls - 0x%08X", url->bstrVal, this);
  // Reset any existing reference to chrome frame since this is a new
  // navigation.
  DisconnectFromChromeFrame();
  OnBeforeNavigate2(dispatch, url, flags, target_frame_name, post_data,
                    headers, cancel);
  return S_OK;
}

STDMETHODIMP_(void) WebBrowserEventSink::OnNavigateComplete2Internal(
    IDispatch* dispatch, VARIANT* url) {
  DLOG(INFO) << __FUNCTION__;
  ConnectToChromeFrame();
  OnNavigateComplete2(dispatch, url);
}

STDMETHODIMP_(void) WebBrowserEventSink::OnDocumentCompleteInternal(
    IDispatch* dispatch, VARIANT* url) {
  DLOG(INFO) << __FUNCTION__;
  OnDocumentComplete(dispatch, url);
}

STDMETHODIMP_(void) WebBrowserEventSink::OnFileDownloadInternal(
    VARIANT_BOOL active_doc, VARIANT_BOOL* cancel) {
  DLOG(INFO) << __FUNCTION__ << StringPrintf(" 0x%08X ad=%i", this, active_doc);
  OnFileDownload(active_doc, cancel);
  // Always cancel file downloads in tests.
  *cancel = VARIANT_TRUE;
}

STDMETHODIMP_(void) WebBrowserEventSink::OnNewWindow3Internal(
    IDispatch** dispatch, VARIANT_BOOL* cancel, DWORD flags, BSTR url_context,
    BSTR url) {
  DLOG(INFO) << __FUNCTION__;
  if (!dispatch) {
    NOTREACHED() << "Invalid argument - dispatch";
    return;
  }

  // Call the OnNewWindow3 with original args
  OnNewWindow3(dispatch, cancel, flags, url_context, url);

  // Note that |dispatch| is an [in/out] argument. IE is asking listeners if
  // they want to use a IWebBrowser2 of their choice for the new window.
  // Since we need to listen on events on the new browser, we create one
  // if needed.
  if (!*dispatch) {
    ScopedComPtr<IDispatch> new_browser;
    HRESULT hr = new_browser.CreateInstance(CLSID_InternetExplorer, NULL,
                                            CLSCTX_LOCAL_SERVER);
    DCHECK(SUCCEEDED(hr) && new_browser);
    *dispatch = new_browser.Detach();
  }

  if (*dispatch)
    OnNewBrowserWindow(*dispatch, url);
}

HRESULT WebBrowserEventSink::OnLoadInternal(const VARIANT* param) {
  DLOG(INFO) << __FUNCTION__ << " " << param->bstrVal;
  if (chrome_frame_) {
    OnLoad(param->bstrVal);
  } else {
    DLOG(WARNING) << "Invalid chrome frame pointer";
  }
  return S_OK;
}

HRESULT WebBrowserEventSink::OnLoadErrorInternal(const VARIANT* param) {
  DLOG(INFO) << __FUNCTION__ << " " << param->bstrVal;
  if (chrome_frame_) {
    OnLoadError(param->bstrVal);
  } else {
    DLOG(WARNING) << "Invalid chrome frame pointer";
  }
  return S_OK;
}

HRESULT WebBrowserEventSink::OnMessageInternal(const VARIANT* param) {
  DLOG(INFO) << __FUNCTION__ << " " << param;
  if (!chrome_frame_.get()) {
    DLOG(WARNING) << "Invalid chrome frame pointer";
    return S_OK;
  }

  ScopedVariant data, origin, source;
  if (param && (V_VT(param) == VT_DISPATCH)) {
    wchar_t* properties[] = { L"data", L"origin", L"source" };
    const int prop_count = arraysize(properties);
    DISPID ids[prop_count] = {0};

    HRESULT hr = param->pdispVal->GetIDsOfNames(IID_NULL, properties,
        prop_count, LOCALE_SYSTEM_DEFAULT, ids);
    if (SUCCEEDED(hr)) {
      DISPPARAMS params = { 0 };
      EXPECT_HRESULT_SUCCEEDED(param->pdispVal->Invoke(ids[0], IID_NULL,
          LOCALE_SYSTEM_DEFAULT, DISPATCH_PROPERTYGET, &params,
          data.Receive(), NULL, NULL));
      EXPECT_HRESULT_SUCCEEDED(param->pdispVal->Invoke(ids[1], IID_NULL,
          LOCALE_SYSTEM_DEFAULT, DISPATCH_PROPERTYGET, &params,
          origin.Receive(), NULL, NULL));
      EXPECT_HRESULT_SUCCEEDED(param->pdispVal->Invoke(ids[2], IID_NULL,
          LOCALE_SYSTEM_DEFAULT, DISPATCH_PROPERTYGET, &params,
          source.Receive(), NULL, NULL));
    }
  }

  OnMessage(V_BSTR(&data), V_BSTR(&origin), V_BSTR(&source));
  return S_OK;
}

HRESULT WebBrowserEventSink::LaunchIEAndNavigate(
    const std::wstring& navigate_url) {
  is_main_browser_object_ = true;
  HRESULT hr = LaunchIEAsComServer(web_browser2_.Receive());
  EXPECT_EQ(S_OK, hr);
  if (hr == S_OK) {
    web_browser2_->put_Visible(VARIANT_TRUE);
    hr = DispEventAdvise(web_browser2_, &DIID_DWebBrowserEvents2);
    EXPECT_TRUE(hr == S_OK);
    hr = Navigate(navigate_url);
  }

  DLOG_IF(WARNING, FAILED(hr)) << "Failed to launch IE. Error:" << hr;
  return hr;
}

HRESULT WebBrowserEventSink::Navigate(const std::wstring& navigate_url) {
  VARIANT empty = ScopedVariant::kEmptyVariant;
  ScopedVariant url;
  url.Set(navigate_url.c_str());

  HRESULT hr = S_OK;
  hr = web_browser2_->Navigate2(url.AsInput(), &empty, &empty, &empty, &empty);
  EXPECT_TRUE(hr == S_OK);
  return hr;
}

void WebBrowserEventSink::SetFocusToChrome() {
  simulate_input::SetKeyboardFocusToWindow(GetRendererWindow());
}

void WebBrowserEventSink::SendKeys(const wchar_t* input_string) {
  SetFocusToChrome();
  simulate_input::SendStringW(input_string);
}

void WebBrowserEventSink::SendMouseClick(int x, int y,
                                         simulate_input::MouseButton button) {
  simulate_input::SendMouseClick(GetRendererWindow(), x, y, button);
}

void WebBrowserEventSink::SendMouseClickToIE(int x, int y,
    simulate_input::MouseButton button) {
  simulate_input::SendMouseClick(GetIERendererWindow(), x, y, button);
}

void WebBrowserEventSink::ConnectToChromeFrame() {
  DCHECK(web_browser2_);
  if (chrome_frame_.get())
    return;
  ScopedComPtr<IShellBrowser> shell_browser;
  DoQueryService(SID_STopLevelBrowser, web_browser2_,
                 shell_browser.Receive());

  if (shell_browser) {
    ScopedComPtr<IShellView> shell_view;
    shell_browser->QueryActiveShellView(shell_view.Receive());
    if (shell_view) {
      shell_view->GetItemObject(SVGIO_BACKGROUND, __uuidof(IChromeFrame),
           reinterpret_cast<void**>(chrome_frame_.Receive()));
    }

    if (chrome_frame_) {
      ScopedVariant onmessage(onmessage_.ToDispatch());
      ScopedVariant onloaderror(onloaderror_.ToDispatch());
      ScopedVariant onload(onload_.ToDispatch());
      EXPECT_HRESULT_SUCCEEDED(chrome_frame_->put_onmessage(onmessage));
      EXPECT_HRESULT_SUCCEEDED(chrome_frame_->put_onloaderror(onloaderror));
      EXPECT_HRESULT_SUCCEEDED(chrome_frame_->put_onload(onload));
    }
  }
}

void WebBrowserEventSink::DisconnectFromChromeFrame() {
  if (chrome_frame_) {
    // Use a local ref counted copy of the IChromeFrame interface as the
    // outgoing calls could cause the interface to be deleted due to a message
    // pump running in the context of the outgoing call.
    ScopedComPtr<IChromeFrame> chrome_frame(chrome_frame_);
    chrome_frame_.Release();
    ScopedVariant dummy(static_cast<IDispatch*>(NULL));
    chrome_frame->put_onmessage(dummy);
    chrome_frame->put_onload(dummy);
    chrome_frame->put_onloaderror(dummy);
  }
}

HWND WebBrowserEventSink::GetRendererWindow() {
  DCHECK(chrome_frame_);
  HWND renderer_window = NULL;
  ScopedComPtr<IOleWindow> ole_window;
  ole_window.QueryFrom(chrome_frame_);
  EXPECT_TRUE(ole_window.get());

  if (ole_window) {
    HWND activex_window = NULL;
    ole_window->GetWindow(&activex_window);
    EXPECT_TRUE(IsWindow(activex_window));

    // chrome tab window is the first (and the only) child of activex
    HWND chrome_tab_window = GetWindow(activex_window, GW_CHILD);
    EXPECT_TRUE(IsWindow(chrome_tab_window));
    renderer_window = GetWindow(chrome_tab_window, GW_CHILD);
  }

  EXPECT_TRUE(IsWindow(renderer_window));
  return renderer_window;
}

HWND WebBrowserEventSink::GetIERendererWindow() {
  DCHECK(web_browser2_);
  HWND renderer_window = NULL;
  ScopedComPtr<IDispatch> doc;
  HRESULT hr = web_browser2_->get_Document(doc.Receive());
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_TRUE(doc);
  if (doc) {
    ScopedComPtr<IOleWindow> ole_window;
    ole_window.QueryFrom(doc);
    EXPECT_TRUE(ole_window);
    if (ole_window) {
      ole_window->GetWindow(&renderer_window);
    }
  }
  return renderer_window;
}

HRESULT WebBrowserEventSink::SetWebBrowser(IWebBrowser2* web_browser2) {
  DCHECK(web_browser2_.get() == NULL);
  DCHECK(!is_main_browser_object_);
  web_browser2_ = web_browser2;
  web_browser2_->put_Visible(VARIANT_TRUE);
  HRESULT hr = DispEventAdvise(web_browser2_, &DIID_DWebBrowserEvents2);
  return hr;
}

HRESULT WebBrowserEventSink::CloseWebBrowser() {
  DCHECK_EQ(process_id_to_wait_for_, 0u);
  if (!web_browser2_)
    return E_FAIL;

  DisconnectFromChromeFrame();
  web_browser2_->Quit();
  return S_OK;
}

void WebBrowserEventSink::ExpectRendererWindowHasFocus() {
  HWND renderer_window = GetRendererWindow();
  EXPECT_TRUE(IsWindow(renderer_window));

  for (HWND first_child = renderer_window;
      IsWindow(first_child); first_child = GetWindow(first_child, GW_CHILD)) {
    renderer_window = first_child;
  }

  wchar_t class_name[MAX_PATH] = {0};
  GetClassName(renderer_window, class_name, arraysize(class_name));
  EXPECT_EQ(0, _wcsicmp(class_name, L"Chrome_RenderWidgetHostHWND"));

  DWORD renderer_thread = 0;
  DWORD renderer_process = 0;
  renderer_thread = GetWindowThreadProcessId(renderer_window,
                                             &renderer_process);

  ASSERT_TRUE(AttachThreadInput(GetCurrentThreadId(), renderer_thread, TRUE));
  HWND focus_window = GetFocus();
  EXPECT_TRUE(focus_window == renderer_window);
  EXPECT_TRUE(AttachThreadInput(GetCurrentThreadId(), renderer_thread, FALSE));
}

void WebBrowserEventSink::ExpectIERendererWindowHasFocus() {
  HWND renderer_window = GetIERendererWindow();
  EXPECT_TRUE(IsWindow(renderer_window));

  DWORD renderer_thread = 0;
  DWORD renderer_process = 0;
  renderer_thread = GetWindowThreadProcessId(renderer_window,
                                             &renderer_process);

  ASSERT_TRUE(AttachThreadInput(GetCurrentThreadId(), renderer_thread, TRUE));
  HWND focus_window = GetFocus();
  EXPECT_TRUE(focus_window == renderer_window);
  EXPECT_TRUE(AttachThreadInput(GetCurrentThreadId(), renderer_thread, FALSE));
}

void WebBrowserEventSink::ExpectAddressBarUrl(
    const std::wstring& expected_url) {
  DCHECK(web_browser2_);
  if (web_browser2_) {
    ScopedBstr address_bar_url;
    EXPECT_EQ(S_OK, web_browser2_->get_LocationURL(address_bar_url.Receive()));
    EXPECT_EQ(expected_url, std::wstring(address_bar_url));
  }
}

void WebBrowserEventSink::Exec(const GUID* cmd_group_guid, DWORD command_id,
                               DWORD cmd_exec_opt, VARIANT* in_args,
                               VARIANT* out_args) {
  ScopedComPtr<IOleCommandTarget> shell_browser_cmd_target;
  DoQueryService(SID_STopLevelBrowser, web_browser2_,
                 shell_browser_cmd_target.Receive());
  ASSERT_TRUE(NULL != shell_browser_cmd_target);
  EXPECT_HRESULT_SUCCEEDED(shell_browser_cmd_target->Exec(cmd_group_guid,
      command_id, cmd_exec_opt, in_args, out_args));
}

void WebBrowserEventSink::WatchChromeWindow(const wchar_t* window_class) {
  DCHECK(window_class);
  window_watcher_.AddObserver(this, WideToUTF8(window_class));
}

void WebBrowserEventSink::StopWatching() {
  window_watcher_.RemoveObserver(this);
}

std::wstring GetExeVersion(const std::wstring& exe_path) {
  scoped_ptr<FileVersionInfo> ie_version_info(
      FileVersionInfo::CreateFileVersionInfo(FilePath(exe_path)));
  return ie_version_info->product_version();
}

IEVersion GetInstalledIEVersion() {
  std::wstring path = chrome_frame_test::GetExecutableAppPath(kIEImageName);
  std::wstring version = GetExeVersion(path);

  switch (version[0]) {
    case '6':
      return IE_6;
    case '7':
      return IE_7;
    case '8':
      return IE_8;
    default:
      break;
  }

  return IE_UNSUPPORTED;
}

FilePath GetProfilePathForIE() {
  FilePath profile_path;
  // Browsers without IDeleteBrowsingHistory in non-priv mode
  // have their profiles moved into "Temporary Internet Files".
  // The code below basically retrieves the version of IE and computes
  // the profile directory accordingly.
  if (GetInstalledIEVersion() == IE_8) {
    profile_path = GetProfilePath(kIEProfileName);
  } else {
    profile_path = GetIETemporaryFilesFolder();
    profile_path = profile_path.Append(L"Google Chrome Frame");
  }
  return profile_path;
}

FilePath GetTestDataFolder() {
  FilePath test_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &test_dir);
  test_dir = test_dir.Append(FILE_PATH_LITERAL("chrome_frame"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"));
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

bool AddCFMetaTag(std::string* html_data) {
  if (!html_data) {
    NOTREACHED();
    return false;
  }
  size_t head = html_data->find("<head>");
  if (head == std::string::npos) {
    // Add missing head section.
    size_t html = html_data->find("<html>");
    EXPECT_NE(std::string::npos, html) << "Meta tag will not be injected "
        << "because the html tag could not be found";
    if (html != std::string::npos) {
      html_data->insert(html + strlen("<html>"), "<head></head>");
      head = html_data->find("<head>");
    }
  }
  if (head != std::string::npos) {
    html_data->insert(
        head + strlen("<head>"),
        "<meta http-equiv=\"x-ua-compatible\" content=\"chrome=1\" />");
  }
  return head != std::string::npos;
}

void DelaySendExtendedKeysEnter(TimedMsgLoop* loop, int delay, char c,
                                int repeat, simulate_input::Modifier mod) {
  const unsigned int kInterval = 25;
  unsigned int next_delay = delay;
  for (int i = 0; i < repeat; i++) {
    loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
        simulate_input::SendExtendedKey, c, mod), next_delay);
    next_delay += kInterval;
  }

  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
    simulate_input::SendCharA, VK_RETURN, simulate_input::NONE), next_delay);
}

base::ProcessHandle StartCrashService() {
  FilePath exe_dir;
  if (!PathService::Get(base::DIR_EXE, &exe_dir)) {
    DCHECK(false);
    return NULL;
  }

  base::ProcessHandle crash_service = NULL;

  FilePath crash_service_path = exe_dir.AppendASCII("crash_service.exe");
  if (!base::LaunchApp(crash_service_path.value(), false, false,
                       &crash_service)) {
    DLOG(ERROR) << "Couldn't start crash_service.exe";
    return NULL;
  }

  DLOG(INFO) << "Started crash_service.exe so you know if a test crashes!";
  // This sleep is to ensure that the crash service is done initializing, i.e
  // the pipe creation, etc.
  Sleep(500);
  return crash_service;
}

}  // namespace chrome_frame_test
