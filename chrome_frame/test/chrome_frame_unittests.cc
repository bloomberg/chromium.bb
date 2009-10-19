// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <windows.h>
#include <stdarg.h>

// IShellWindows includes.  Unfortunately we can't keep these in
// alphabetic order since exdisp will bark if some interfaces aren't fully
// defined.
#include <mshtml.h>
#include <exdisp.h>

#include "base/basictypes.h"
#include "base/file_version_info.h"
#include "base/file_util.h"
#include "base/scoped_bstr_win.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_variant_win.h"
#include "base/sys_info.h"
#include "gmock/gmock.h"
#include "net/url_request/url_request_unittest.h"
#include "chrome_frame/test/chrome_frame_unittests.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/chrome_frame_delegate.h"
#include "chrome_frame/crash_reporting/vectored_handler-impl.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/helper_gmock.h"
#include "chrome_frame/test_utils.h"
#include "chrome_frame/utils.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/helper.h"

const wchar_t kDocRoot[] = L"chrome_frame\\test\\data";
const int kLongWaitTimeout = 60 * 1000;
const int kShortWaitTimeout = 25 * 1000;

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



void ChromeFrameTestWithWebServer::SetUp() {
  server_.SetUp();
  results_dir_ = server_.GetDataDir();
  file_util::AppendToPath(&results_dir_, L"dump");
}

void ChromeFrameTestWithWebServer::TearDown() {
  CloseBrowser();

  // Web browsers tend to relaunch themselves in other processes, meaning the
  // KillProcess stuff above might not have actually cleaned up all our browser
  // instances, so make really sure browsers are dead.
  base::KillProcesses(chrome_frame_test::kIEImageName, 0, NULL);
  base::KillProcesses(chrome_frame_test::kIEBrokerImageName, 0, NULL);
  base::KillProcesses(chrome_frame_test::kFirefoxImageName, 0, NULL);
  base::KillProcesses(chrome_frame_test::kSafariImageName, 0, NULL);
  base::KillProcesses(chrome_frame_test::kChromeImageName, 0, NULL);

  server_.TearDown();
}

bool ChromeFrameTestWithWebServer::LaunchBrowser(BrowserKind browser,
                                               const wchar_t* page) {
  std::wstring url = UTF8ToWide(server_.Resolve(page).spec());
  browser_ = browser;
  if (browser == IE) {
    browser_handle_.Set(chrome_frame_test::LaunchIE(url));
  } else if (browser == FIREFOX) {
    browser_handle_.Set(chrome_frame_test::LaunchFirefox(url));
  } else if (browser == OPERA) {
    browser_handle_.Set(chrome_frame_test::LaunchOpera(url));
  } else if (browser == SAFARI) {
    browser_handle_.Set(chrome_frame_test::LaunchSafari(url));
  } else if (browser == CHROME) {
    browser_handle_.Set(chrome_frame_test::LaunchChrome(url));
  } else {
    NOTREACHED();
  }

  return browser_handle_.IsValid();
}

void ChromeFrameTestWithWebServer::CloseBrowser() {
  if (!browser_handle_.IsValid())
    return;

  int attempts = 0;
  if (browser_ == IE) {
    attempts = chrome_frame_test::CloseAllIEWindows();
  } else {
    attempts = chrome_frame_test::CloseVisibleWindowsOnAllThreads(
                                                               browser_handle_);
  }

  if (attempts > 0) {
    DWORD wait = ::WaitForSingleObject(browser_handle_, 20000);
    if (wait == WAIT_OBJECT_0) {
      browser_handle_.Close();
    } else {
      DLOG(ERROR) << "WaitForSingleObject returned " << wait;
    }
  } else {
    DLOG(ERROR) << "No attempts to close browser windows";
  }

  if (browser_handle_.IsValid()) {
    DWORD exit_code = 0;
    if (!::GetExitCodeProcess(browser_handle_, &exit_code) ||
        exit_code == STILL_ACTIVE) {
      DLOG(ERROR) << L"Forcefully killing browser process. Exit:" << exit_code;
      base::KillProcess(browser_handle_, 0, true);
    }
    browser_handle_.Close();
  }
}

bool ChromeFrameTestWithWebServer::BringBrowserToTop() {
  return chrome_frame_test::EnsureProcessInForeground(GetProcessId(
                                                              browser_handle_));
}

bool ChromeFrameTestWithWebServer::WaitForTestToComplete(int milliseconds) {
  return server_.WaitToFinish(milliseconds);
}

bool ChromeFrameTestWithWebServer::WaitForOnLoad(int milliseconds) {
  DWORD start = ::GetTickCount();
  std::string data;
  while (!ReadResultFile(L"OnLoadEvent", &data) || data.length() == 0) {
    DWORD now = ::GetTickCount();
    if (start > now) {
      // Very simple check for overflow. In that case we just restart the
      // wait.
      start = now;
    } else if (static_cast<int>(now - start) > milliseconds) {
      break;
    }
    Sleep(100);
  }

  return data.compare("loaded") == 0;
}

bool ChromeFrameTestWithWebServer::ReadResultFile(const std::wstring& file_name,
                                                std::string* data) {
  std::wstring full_path = results_dir_;
  file_util::AppendToPath(&full_path, file_name);
  return file_util::ReadFileToString(full_path, data);
}

bool ChromeFrameTestWithWebServer::CheckResultFile(
    const std::wstring& file_name, const std::string& expected_result) {
  std::string data;
  bool ret = ReadResultFile(file_name, &data);
  if (ret)
    ret = (data == expected_result);

  if (!ret) {
    DLOG(ERROR) << "Error text: " << (data.empty() ? "<empty>" : data.c_str());
  }

  return ret;
}

void ChromeFrameTestWithWebServer::SimpleBrowserTest(BrowserKind browser,
    const wchar_t* page, const wchar_t* result_file_to_check) {
  ASSERT_TRUE(LaunchBrowser(browser, page));
  ASSERT_TRUE(WaitForTestToComplete(kLongWaitTimeout));
  ASSERT_TRUE(CheckResultFile(result_file_to_check, "OK"));
}

void ChromeFrameTestWithWebServer::OptionalBrowserTest(BrowserKind browser,
    const wchar_t* page, const wchar_t* result_file_to_check) {
  if (!LaunchBrowser(browser, page)) {
    DLOG(ERROR) << "Failed to launch browser " << ToString(browser);
  } else {
    ASSERT_TRUE(WaitForTestToComplete(kLongWaitTimeout));
    ASSERT_TRUE(CheckResultFile(result_file_to_check, "OK"));
  }
}

void ChromeFrameTestWithWebServer::VersionTest(BrowserKind browser,
    const wchar_t* page, const wchar_t* result_file_to_check) {
  std::wstring plugin_path;
  PathService::Get(base::DIR_MODULE, &plugin_path);
  file_util::AppendToPath(&plugin_path, L"servers/npchrome_tab.dll");

  static FileVersionInfo* version_info =
      FileVersionInfo::CreateFileVersionInfo(plugin_path);

  std::wstring version;
  if (version_info)
    version = version_info->product_version();

  // If we can't find the npchrome_tab.dll in the src tree, we turn to
  // the directory where chrome is installed.
  if (!version_info) {
    installer::Version* ver_system = InstallUtil::GetChromeVersion(true);
    installer::Version* ver_user = InstallUtil::GetChromeVersion(false);
    ASSERT_TRUE(ver_system || ver_user);

    bool system_install = ver_system ? true : false;
    std::wstring npchrome_path(installer::GetChromeInstallPath(system_install));
    file_util::AppendToPath(&npchrome_path,
        ver_system ? ver_system->GetString() : ver_user->GetString());
    file_util::AppendToPath(&npchrome_path, L"npchrome_tab.dll");
    version_info = FileVersionInfo::CreateFileVersionInfo(npchrome_path);
    if (version_info)
      version = version_info->product_version();
  }

  EXPECT_TRUE(version_info);
  EXPECT_FALSE(version.empty());
  EXPECT_TRUE(LaunchBrowser(browser, page));
  ASSERT_TRUE(WaitForTestToComplete(kLongWaitTimeout));
  ASSERT_TRUE(CheckResultFile(result_file_to_check, WideToUTF8(version)));
}

const wchar_t kPostMessageBasicPage[] = L"files/postmessage_basic_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_PostMessageBasic) {
  SimpleBrowserTest(IE, kPostMessageBasicPage, L"PostMessage");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_PostMessageBasic) {
  SimpleBrowserTest(FIREFOX, kPostMessageBasicPage, L"PostMessage");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_PostMessageBasic) {
  OptionalBrowserTest(OPERA, kPostMessageBasicPage, L"PostMessage");
}

TEST_F(ChromeFrameTestWithWebServer, FullTabIE_MIMEFilterBasic) {
  const wchar_t kMIMEFilterBasicPage[] =
      L"files/chrome_frame_mime_filter_test.html";

  SimpleBrowserTest(IE, kMIMEFilterBasicPage, L"MIMEFilter");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_Resize) {
  SimpleBrowserTest(IE, L"files/chrome_frame_resize.html", L"Resize");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_Resize) {
  SimpleBrowserTest(FIREFOX, L"files/chrome_frame_resize.html", L"Resize");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_Resize) {
  OptionalBrowserTest(OPERA, L"files/chrome_frame_resize.html", L"Resize");
}

const wchar_t kNavigateURLAbsolutePage[] =
    L"files/navigateurl_absolute_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_NavigateURLAbsolute) {
  SimpleBrowserTest(IE, kNavigateURLAbsolutePage, L"NavigateURL");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_NavigateURLAbsolute) {
  SimpleBrowserTest(FIREFOX, kNavigateURLAbsolutePage, L"NavigateURL");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_NavigateURLAbsolute) {
  OptionalBrowserTest(OPERA, kNavigateURLAbsolutePage, L"NavigateURL");
}

const wchar_t kNavigateURLRelativePage[] =
    L"files/navigateurl_relative_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_NavigateURLRelative) {
  SimpleBrowserTest(IE, kNavigateURLRelativePage, L"NavigateURL");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_NavigateURLRelative) {
  SimpleBrowserTest(FIREFOX, kNavigateURLRelativePage, L"NavigateURL");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_NavigateURLRelative) {
  OptionalBrowserTest(OPERA, kNavigateURLRelativePage, L"NavigateURL");
}

const wchar_t kNavigateSimpleObjectFocus[] = L"files/simple_object_focus.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_ObjectFocus) {
  SimpleBrowserTest(FIREFOX, kNavigateSimpleObjectFocus, L"ObjectFocus");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_ObjectFocus) {
  SimpleBrowserTest(IE, kNavigateSimpleObjectFocus, L"ObjectFocus");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_ObjectFocus) {
  if (!LaunchBrowser(OPERA, kNavigateSimpleObjectFocus)) {
    DLOG(ERROR) << "Failed to launch browser " << ToString(OPERA);
  } else {
    ASSERT_TRUE(WaitForOnLoad(kLongWaitTimeout));
    BringBrowserToTop();
    // Tab through a couple of times.  Once should be enough in theory
    // but in practice activating the browser can take a few milliseconds more.
    bool ok;
    for (int i = 0;
         i < 5 && (ok = CheckResultFile(L"ObjectFocus", "OK")) == false;
         ++i) {
      Sleep(300);
      chrome_frame_test::SendMnemonic(VK_TAB, false, false, false);
    }
    ASSERT_TRUE(ok);
  }
}

const wchar_t kiframeBasicPage[] = L"files/iframe_basic_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_iframeBasic) {
  SimpleBrowserTest(IE, kiframeBasicPage, L"PostMessage");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_iframeBasic) {
  SimpleBrowserTest(FIREFOX, kiframeBasicPage, L"PostMessage");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_iframeBasic) {
  OptionalBrowserTest(OPERA, kiframeBasicPage, L"PostMessage");
}

const wchar_t kSrcPropertyTestPage[] = L"files/src_property_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_SrcProperty) {
  SimpleBrowserTest(IE, kSrcPropertyTestPage, L"SrcProperty");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_SrcProperty) {
  SimpleBrowserTest(FIREFOX, kSrcPropertyTestPage, L"SrcProperty");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_SrcProperty) {
  OptionalBrowserTest(OPERA, kSrcPropertyTestPage, L"SrcProperty");
}

const wchar_t kCFInstanceBasicTestPage[] = L"files/CFInstance_basic_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceBasic) {
  SimpleBrowserTest(IE, kCFInstanceBasicTestPage, L"CFInstanceBasic");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceBasic) {
  SimpleBrowserTest(FIREFOX, kCFInstanceBasicTestPage, L"CFInstanceBasic");
}

const wchar_t kCFISingletonPage[] = L"files/CFInstance_singleton_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceSingleton) {
  SimpleBrowserTest(IE, kCFISingletonPage, L"CFInstanceSingleton");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceSingleton) {
  SimpleBrowserTest(FIREFOX, kCFISingletonPage, L"CFInstanceSingleton");
}

const wchar_t kCFIDelayPage[] = L"files/CFInstance_delay_host.html";

TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeIE_CFInstanceDelay) {
  SimpleBrowserTest(IE, kCFIDelayPage, L"CFInstanceDelay");
}

TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeFF_CFInstanceDelay) {
  SimpleBrowserTest(FIREFOX, kCFIDelayPage, L"CFInstanceDelay");
}

TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeOpera_CFInstanceDelay) {
  OptionalBrowserTest(OPERA, kCFIDelayPage, L"CFInstanceDelay");
}

const wchar_t kCFIFallbackPage[] = L"files/CFInstance_fallback_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceFallback) {
  SimpleBrowserTest(IE, kCFIFallbackPage, L"CFInstanceFallback");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceFallback) {
  SimpleBrowserTest(FIREFOX, kCFIFallbackPage, L"CFInstanceFallback");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_CFInstanceFallback) {
  OptionalBrowserTest(OPERA, kCFIFallbackPage, L"CFInstanceFallback");
}

const wchar_t kCFINoSrcPage[] = L"files/CFInstance_no_src_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceNoSrc) {
  SimpleBrowserTest(IE, kCFINoSrcPage, L"CFInstanceNoSrc");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceNoSrc) {
  SimpleBrowserTest(FIREFOX, kCFINoSrcPage, L"CFInstanceNoSrc");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_CFInstanceNoSrc) {
  OptionalBrowserTest(OPERA, kCFINoSrcPage, L"CFInstanceNoSrc");
}

const wchar_t kCFIIfrOnLoadPage[] = L"files/CFInstance_iframe_onload_host.html";

// disabled since it's unlikely that we care about this case
TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeIE_CFInstanceIfrOnLoad) {
  SimpleBrowserTest(IE, kCFIIfrOnLoadPage, L"CFInstanceIfrOnLoad");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceIfrOnLoad) {
  SimpleBrowserTest(FIREFOX, kCFIIfrOnLoadPage, L"CFInstanceIfrOnLoad");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_CFInstanceIfrOnLoad) {
  OptionalBrowserTest(OPERA, kCFIIfrOnLoadPage, L"CFInstanceIfrOnLoad");
}

const wchar_t kCFIZeroSizePage[] = L"files/CFInstance_zero_size_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceZeroSize) {
  SimpleBrowserTest(IE, kCFIZeroSizePage, L"CFInstanceZeroSize");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceZeroSize) {
  SimpleBrowserTest(FIREFOX, kCFIZeroSizePage, L"CFInstanceZeroSize");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_CFInstanceZeroSize) {
  OptionalBrowserTest(OPERA, kCFIZeroSizePage, L"CFInstanceZeroSize");
}

const wchar_t kCFIIfrPostPage[] = L"files/CFInstance_iframe_post_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceIfrPost) {
  SimpleBrowserTest(IE, kCFIIfrPostPage, L"CFInstanceIfrPost");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceIfrPost) {
  SimpleBrowserTest(FIREFOX, kCFIIfrPostPage, L"CFInstanceIfrPost");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeChrome_CFInstanceIfrPost) {
  OptionalBrowserTest(CHROME, kCFIIfrPostPage, L"CFInstanceIfrPost");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeSafari_CFInstanceIfrPost) {
  OptionalBrowserTest(SAFARI, kCFIIfrPostPage, L"CFInstanceIfrPost");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_CFInstanceIfrPost) {
  OptionalBrowserTest(OPERA, kCFIIfrPostPage, L"CFInstanceIfrPost");
}

const wchar_t kCFIPostPage[] = L"files/CFInstance_post_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstancePost) {
  SimpleBrowserTest(IE, kCFIPostPage, L"CFInstancePost");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstancePost) {
  SimpleBrowserTest(FIREFOX, kCFIPostPage, L"CFInstancePost");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeChrome_CFInstancePost) {
  OptionalBrowserTest(CHROME, kCFIPostPage, L"CFInstancePost");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeSafari_CFInstancePost) {
  OptionalBrowserTest(SAFARI, kCFIPostPage, L"CFInstancePost");
}

TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeOpera_CFInstancePost) {
  OptionalBrowserTest(OPERA, kCFIPostPage, L"CFInstancePost");
}

const wchar_t kCFIRPCPage[] = L"files/CFInstance_rpc_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceRPC) {
  SimpleBrowserTest(IE, kCFIRPCPage, L"CFInstanceRPC");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceRPC) {
  SimpleBrowserTest(FIREFOX, kCFIRPCPage, L"CFInstanceRPC");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeChrome_CFInstanceRPC) {
  OptionalBrowserTest(CHROME, kCFIRPCPage, L"CFInstanceRPC");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeSafari_CFInstanceRPC) {
  OptionalBrowserTest(SAFARI, kCFIRPCPage, L"CFInstanceRPC");
}

TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeOpera_CFInstanceRPC) {
  OptionalBrowserTest(OPERA, kCFIRPCPage, L"CFInstanceRPC");
}

const wchar_t kCFIRPCInternalPage[] =
    L"files/CFInstance_rpc_internal_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceRPCInternal) {
  SimpleBrowserTest(IE, kCFIRPCInternalPage, L"CFInstanceRPCInternal");
}

// Disabled: http://b/issue?id=2050201
TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeFF_CFInstanceRPCInternal) {
  SimpleBrowserTest(FIREFOX, kCFIRPCInternalPage, L"CFInstanceRPCInternal");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeChrome_CFInstanceRPCInternal) {
  OptionalBrowserTest(CHROME, kCFIRPCInternalPage, L"CFInstanceRPCInternal");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeSafari_CFInstanceRPCInternal) {
  OptionalBrowserTest(SAFARI, kCFIRPCInternalPage, L"CFInstanceRPCInternal");
}

const wchar_t kCFIDefaultCtorPage[] =
    L"files/CFInstance_default_ctor_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceDefaultCtor) {
  SimpleBrowserTest(IE, kCFIDefaultCtorPage, L"CFInstanceDefaultCtor");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceDefaultCtor) {
  SimpleBrowserTest(FIREFOX, kCFIDefaultCtorPage, L"CFInstanceDefaultCtor");
}

// Class that mocks external call from VectoredHandlerT for testing purposes.
class EMock : public VEHTraitsBase {
 public:
  static inline bool WriteDump(EXCEPTION_POINTERS* p) {
    g_dump_made = true;
    return true;
  }

  static inline void* Register(PVECTORED_EXCEPTION_HANDLER func,
                               const void* module_start,
                               const void* module_end) {
    VEHTraitsBase::SetModule(module_start, module_end);
    // Return some arbitrary number, expecting to get the same on Unregister()
    return reinterpret_cast<void*>(4);
  }

  static inline ULONG Unregister(void* handle) {
    EXPECT_EQ(handle, reinterpret_cast<void*>(4));
    return 1;
  }

  static inline WORD RtlCaptureStackBackTrace(DWORD FramesToSkip,
      DWORD FramesToCapture, void** BackTrace, DWORD* BackTraceHash) {
    EXPECT_EQ(2, FramesToSkip);
    EXPECT_LE(FramesToSkip + FramesToCapture,
              VectoredHandlerBase::max_back_trace);
    memcpy(BackTrace, g_stack, g_stack_entries * sizeof(BackTrace[0]));
    return g_stack_entries;
  }

  static inline EXCEPTION_REGISTRATION_RECORD* RtlpGetExceptionList() {
    return g_seh_chain;
  }

  // Test helpers

  // Create fake SEH chain of random filters - with and without our module.
  static void SetHaveSEHFilter() {
    SetSEHChain(reinterpret_cast<const char*>(g_module_start) - 0x1000,
                reinterpret_cast<const char*>(g_module_start) + 0x1000,
                reinterpret_cast<const char*>(g_module_end) + 0x7127,
                NULL);
  }

  static void SetNoSEHFilter() {
    SetSEHChain(reinterpret_cast<const char*>(g_module_start) - 0x1000,
                reinterpret_cast<const char*>(g_module_end) + 0x7127,
                NULL);
  }

  // Create fake stack - with and without our module.
  static void SetOnStack() {
    SetStack(reinterpret_cast<const char*>(g_module_start) - 0x11283,
             reinterpret_cast<const char*>(g_module_start) - 0x278361,
             reinterpret_cast<const char*>(g_module_start) + 0x9171,
             reinterpret_cast<const char*>(g_module_end) + 1231,
             NULL);
  }

  static void SetNotOnStack() {
    SetStack(reinterpret_cast<const char*>(g_module_start) - 0x11283,
             reinterpret_cast<const char*>(g_module_start) - 0x278361,
             reinterpret_cast<const char*>(g_module_end) + 1231,
             NULL);
  }

  // Populate stack array
  static void SetStack(const void* p, ...) {
    va_list vl;
    va_start(vl, p);
    g_stack_entries = 0;
    for (; p; ++g_stack_entries) {
      CHECK(g_stack_entries < arraysize(g_stack));
      g_stack[g_stack_entries] = p;
      p = va_arg(vl, const void*);
    }
  }

  static void SetSEHChain(const void* p, ...) {
    va_list vl;
    va_start(vl, p);
    int i = 0;
    for (; p; ++i) {
      CHECK(i + 1 < arraysize(g_seh_chain));
      g_seh_chain[i].Handler = const_cast<void*>(p);
      g_seh_chain[i].Next = &g_seh_chain[i + 1];
      p = va_arg(vl, const void*);
    }

    g_seh_chain[i].Next = EXCEPTION_CHAIN_END;
  }

  static EXCEPTION_REGISTRATION_RECORD g_seh_chain[25];
  static const void* g_stack[VectoredHandlerBase::max_back_trace];
  static WORD g_stack_entries;
  static bool g_dump_made;
};

EXCEPTION_REGISTRATION_RECORD EMock::g_seh_chain[25];
const void* EMock::g_stack[VectoredHandlerBase::max_back_trace];
WORD EMock::g_stack_entries;
bool EMock::g_dump_made;

typedef VectoredHandlerT<EMock> VectoredHandlerMock;

class ExPtrsHelper : public _EXCEPTION_POINTERS {
 public:
  ExPtrsHelper() {
    ExceptionRecord = &er_;
    ContextRecord = &ctx_;
    ZeroMemory(&er_, sizeof(er_));
    ZeroMemory(&ctx_, sizeof(ctx_));
  }

  void Set(DWORD code, void* address, DWORD flags) {
    er_.ExceptionCode = code;
    er_.ExceptionAddress = address;
    er_.ExceptionFlags = flags;
  }

  EXCEPTION_RECORD er_;
  CONTEXT ctx_;
};


TEST(ChromeFrame, ExceptionReport) {
  char* s = reinterpret_cast<char*>(0x30000000);
  char* e = s + 0x10000;
  void* handler = VectoredHandlerMock::Register(s, e);
  char* our_code = s + 0x1111;
  char* not_our_code = s - 0x5555;
  char* not_our_code2 = e + 0x5555;

  ExPtrsHelper ex;
  // Exception in our code, but we have SEH filter
  ex.Set(STATUS_ACCESS_VIOLATION, our_code, 0);
  EMock::SetHaveSEHFilter();
  EMock::SetOnStack();
  EXPECT_EQ(ExceptionContinueSearch, VectoredHandlerMock::VectoredHandler(&ex));
  EXPECT_EQ(1, VectoredHandlerMock::g_exceptions_seen);
  EXPECT_FALSE(EMock::g_dump_made);

  // RPC_E_DISCONNECTED (0x80010108) is "The object invoked has disconnected
  // from its clients", shall not be caught since it's a warning only.
  ex.Set(RPC_E_DISCONNECTED, our_code, 0);
  EMock::SetHaveSEHFilter();
  EMock::SetOnStack();
  EXPECT_EQ(ExceptionContinueSearch, VectoredHandlerMock::VectoredHandler(&ex));
  EXPECT_EQ(1, VectoredHandlerMock::g_exceptions_seen);
  EXPECT_FALSE(EMock::g_dump_made);


  // Exception, not in our code, we do not have SEH and we are not in stack.
  ex.Set(STATUS_INTEGER_DIVIDE_BY_ZERO, not_our_code, 0);
  EMock::SetNoSEHFilter();
  EMock::SetNotOnStack();
  EXPECT_EQ(ExceptionContinueSearch, VectoredHandlerMock::VectoredHandler(&ex));
  EXPECT_EQ(2, VectoredHandlerMock::g_exceptions_seen);
  EXPECT_FALSE(EMock::g_dump_made);

  // Exception, not in our code, no SEH, but we are on the stack.
  ex.Set(STATUS_INTEGER_DIVIDE_BY_ZERO, not_our_code2, 0);
  EMock::SetNoSEHFilter();
  EMock::SetOnStack();
  EXPECT_EQ(ExceptionContinueSearch, VectoredHandlerMock::VectoredHandler(&ex));
  EXPECT_EQ(3, VectoredHandlerMock::g_exceptions_seen);
  EXPECT_TRUE(EMock::g_dump_made);
  EMock::g_dump_made = false;


  // Exception, in our code, no SEH, not on stack (assume FPO screwed us)
  ex.Set(STATUS_INTEGER_DIVIDE_BY_ZERO, our_code, 0);
  EMock::SetNoSEHFilter();
  EMock::SetNotOnStack();
  EXPECT_EQ(ExceptionContinueSearch, VectoredHandlerMock::VectoredHandler(&ex));
  EXPECT_EQ(4, VectoredHandlerMock::g_exceptions_seen);
  EXPECT_TRUE(EMock::g_dump_made);
  EMock::g_dump_made = false;

  VectoredHandlerMock::Unregister();
}

const wchar_t kInitializeHiddenPage[] = L"files/initialize_hidden.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_InitializeHidden) {
  SimpleBrowserTest(IE, kInitializeHiddenPage, L"InitializeHidden");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_InitializeHidden) {
  SimpleBrowserTest(FIREFOX, kInitializeHiddenPage, L"InitializeHidden");
}

// Disabled due to a problem with Opera.
// http://b/issue?id=1708275
TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeOpera_InitializeHidden) {
  OptionalBrowserTest(OPERA, kInitializeHiddenPage, L"InitializeHidden");
}

const wchar_t kInHeadPage[] = L"files/in_head.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_InHead) {
  SimpleBrowserTest(FIREFOX, kInHeadPage, L"InHead");
}

const wchar_t kVersionPage[] = L"files/version.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_Version) {
  VersionTest(IE, kVersionPage, L"version");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_Version) {
  VersionTest(FIREFOX, kVersionPage, L"version");
}

const wchar_t kEventListenerPage[] = L"files/event_listener.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_EventListener) {
  SimpleBrowserTest(IE, kEventListenerPage, L"EventListener");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_EventListener) {
  SimpleBrowserTest(FIREFOX, kEventListenerPage, L"EventListener");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_EventListener) {
  OptionalBrowserTest(OPERA, kEventListenerPage, L"EventListener");
}

const wchar_t kPrivilegedApisPage[] = L"files/privileged_apis_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_PrivilegedApis) {
  SimpleBrowserTest(IE, kPrivilegedApisPage, L"PrivilegedApis");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_PrivilegedApis) {
  SimpleBrowserTest(FIREFOX, kPrivilegedApisPage, L"PrivilegedApis");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_PrivilegedApis) {
  OptionalBrowserTest(OPERA, kPrivilegedApisPage, L"PrivilegedApis");
}

class ChromeFrameTestEnvironment: public testing::Environment {
  public:
    ~ChromeFrameTestEnvironment() {
    }

    void SetUp() {
      ScopedChromeFrameRegistrar::RegisterDefaults();
    }

    void TearDown() {
    }
};

::testing::Environment* const chrome_frame_env =
    ::testing::AddGlobalTestEnvironment(new ChromeFrameTestEnvironment);

// TODO(stoyan): - Move everything below in separate file(s).
struct LaunchDelegateMock : public ProxyFactory::LaunchDelegate {
  MOCK_METHOD2(LaunchComplete, void(ChromeFrameAutomationProxy*,
    AutomationLaunchResult));
};

TEST(ProxyFactoryTest, CreateDestroy) {
  ProxyFactory f;
  LaunchDelegateMock d;
  EXPECT_CALL(d, LaunchComplete(testing::NotNull(), testing::_)).Times(1);
  void* id = f.GetAutomationServer(0, L"Adam.N.Epilinter", L"", false, &d);
  f.ReleaseAutomationServer(id);
}

TEST(ProxyFactoryTest, CreateSameProfile) {
  ProxyFactory f;
  LaunchDelegateMock d;
  EXPECT_CALL(d, LaunchComplete(testing::NotNull(), testing::_)).Times(2);
  void* i1 = f.GetAutomationServer(0, L"Dr. Gratiano Forbeson", L"", false, &d);
  void* i2 = f.GetAutomationServer(0, L"Dr. Gratiano Forbeson", L"", false, &d);
  EXPECT_EQ(i1, i2);
  f.ReleaseAutomationServer(i2);
  f.ReleaseAutomationServer(i1);
}

TEST(ProxyFactoryTest, CreateDifferentProfiles) {
  ProxyFactory f;
  LaunchDelegateMock d;
  EXPECT_CALL(d, LaunchComplete(testing::NotNull(), testing::_)).Times(2);
  void* i1 = f.GetAutomationServer(0, L"Adam.N.Epilinter", L"", false, &d);
  void* i2 = f.GetAutomationServer(0, L"Dr. Gratiano Forbeson", L"", false, &d);
  EXPECT_NE(i1, i2);
  f.ReleaseAutomationServer(i2);
  f.ReleaseAutomationServer(i1);
}

// ChromeFrameAutomationClient [CFAC] tests.
struct MockCFDelegate : public ChromeFrameDelegateImpl {
  MOCK_CONST_METHOD0(GetWindow, WindowType());
  MOCK_METHOD1(GetBounds, void(RECT* bounds));
  MOCK_METHOD0(GetDocumentUrl, std::string());
  MOCK_METHOD2(ExecuteScript, bool(const std::string& script,
                                   std::string* result));
  MOCK_METHOD0(OnAutomationServerReady, void());
  MOCK_METHOD2(OnAutomationServerLaunchFailed, void(
      AutomationLaunchResult reason, const std::string& server_version));
  // This remains in interface since we call it if Navigate()
  // returns immediate error.
  MOCK_METHOD2(OnLoadFailed, void(int error_code, const std::string& url));

  // Do not mock this method. :) Use it as message demuxer and dispatcher
  // to the following methods (which we mock)
  // MOCK_METHOD1(OnMessageReceived, void(const IPC::Message&));


  MOCK_METHOD2(OnNavigationStateChanged, void(int tab_handle, int flags));
  MOCK_METHOD2(OnUpdateTargetUrl, void(int tab_handle,
      const std::wstring& new_target_url));
  MOCK_METHOD2(OnAcceleratorPressed, void(int tab_handle,
      const MSG& accel_message));
  MOCK_METHOD2(OnTabbedOut, void(int tab_handle, bool reverse));
  MOCK_METHOD3(OnOpenURL, void(int tab_handle, const GURL& url,
      int open_disposition));
  MOCK_METHOD2(OnDidNavigate, void(int tab_handle,
      const IPC::NavigationInfo& navigation_info));
  MOCK_METHOD3(OnNavigationFailed, void(int tab_handle, int error_code,
      const GURL& gurl));
  MOCK_METHOD2(OnLoad, void(int tab_handle, const GURL& url));
  MOCK_METHOD4(OnMessageFromChromeFrame, void(int tab_handle,
      const std::string& message,
      const std::string& origin,
      const std::string& target));
  MOCK_METHOD5(OnHandleContextMenu, void(int tab_handle, HANDLE menu_handle,
      int x_pos, int y_pos, int align_flags));
  MOCK_METHOD3(OnRequestStart, void(int tab_handle, int request_id,
      const IPC::AutomationURLRequest& request));
  MOCK_METHOD3(OnRequestRead, void(int tab_handle, int request_id,
      int bytes_to_read));
  MOCK_METHOD3(OnRequestEnd, void(int tab_handle, int request_id,
      const URLRequestStatus& status));
  MOCK_METHOD3(OnSetCookieAsync, void(int tab_handle, const GURL& url,
      const std::string& cookie));

  // Use for sending network responses
  void SetAutomationSender(IPC::Message::Sender* automation) {
    automation_ = automation;
  }

  // Set-expectation helpers
  void SetOnNavigationStateChanged(int tab_handle) {
    EXPECT_CALL(*this,
        OnNavigationStateChanged(testing::Eq(tab_handle), testing::_))
        .Times(testing::AnyNumber());
  }

  // Response sender helpers
  void ReplyStarted(const IPC::AutomationURLResponse* response,
                    int tab_handle, int request_id,
                    const IPC::AutomationURLRequest& request) {
    automation_->Send(new AutomationMsg_RequestStarted(0, tab_handle,
        request_id, *response));
  }

  void ReplyData(const std::string* data, int tab_handle, int request_id,
                 int bytes_to_read) {
    automation_->Send(new AutomationMsg_RequestData(0, tab_handle,
        request_id, *data));
  }

  void ReplyEOF(int tab_handle, int request_id) {
    automation_->Send(new AutomationMsg_RequestEnd(0, tab_handle,
        request_id, URLRequestStatus()));
  }

  void Reply404(int tab_handle, int request_id,
                const IPC::AutomationURLRequest& request) {
    const IPC::AutomationURLResponse notfound = {"", "HTTP/1.1 404\r\n\r\n"};
    automation_->Send(new AutomationMsg_RequestStarted(0, tab_handle,
        request_id, notfound));
    automation_->Send(new AutomationMsg_RequestEnd(0, tab_handle,
        request_id, URLRequestStatus()));
  }

  IPC::Message::Sender* automation_;
};

class MockProxyFactory : public ProxyFactory {
 public:
  MOCK_METHOD5(GetAutomationServer, void*(int, const std::wstring&,
      const std::wstring& extra_argument, bool, ProxyFactory::LaunchDelegate*));
  MOCK_METHOD1(ReleaseAutomationServer, bool(void* id));

  MockProxyFactory() : thread_("mock factory worker") {
    thread_.Start();
    loop_ = thread_.message_loop();
  }

  // Fake implementation
  void GetServerImpl(ChromeFrameAutomationProxy* pxy,
                     AutomationLaunchResult result,
                     int timeout,
                     ProxyFactory::LaunchDelegate* d) {
    Task* task = NewRunnableMethod(d,
        &ProxyFactory::LaunchDelegate::LaunchComplete, pxy, result);
    loop_->PostDelayedTask(FROM_HERE, task, timeout/2);
  }

  base::Thread thread_;
  MessageLoop* loop_;
};

class MockAutomationProxy : public ChromeFrameAutomationProxy {
 public:
  MOCK_METHOD1(Send, bool(IPC::Message*));
  MOCK_METHOD3(SendAsAsync, void(IPC::SyncMessage* msg, void* callback,
                                 void* key));
  MOCK_METHOD1(CancelAsync, void(void* key));
  MOCK_METHOD1(CreateTabProxy, scoped_refptr<TabProxy>(int handle));
  MOCK_METHOD0(server_version, std::string(void));
  MOCK_METHOD1(SendProxyConfig, void(const std::string&));
  MOCK_METHOD1(SetEnableExtensionAutomation, void(bool enable));

  ~MockAutomationProxy() {}
};

struct MockAutomationMessageSender : public AutomationMessageSender {
  MOCK_METHOD1(Send, bool(IPC::Message*));
  MOCK_METHOD3(SendWithTimeout, bool(IPC::Message* , int , bool*));

  void ForwardTo(MockAutomationProxy *p) {
    proxy_ = p;
    ON_CALL(*this, Send(testing::_))
        .WillByDefault(testing::Invoke(proxy_, &MockAutomationProxy::Send));
  }

  MockAutomationProxy* proxy_;
};

template <> struct RunnableMethodTraits<ProxyFactory::LaunchDelegate> {
  void RetainCallee(ProxyFactory::LaunchDelegate* obj) {}
  void ReleaseCallee(ProxyFactory::LaunchDelegate* obj) {}
};

template <> struct RunnableMethodTraits<MockProxyFactory> {
  void RetainCallee(MockProxyFactory* obj) {}
  void ReleaseCallee(MockProxyFactory* obj) {}
};

template <> struct RunnableMethodTraits<ChromeFrameAutomationClient> {
  void RetainCallee(ChromeFrameAutomationClient* obj) {}
  void ReleaseCallee(ChromeFrameAutomationClient* obj) {}
};

// MessageLoopForUI wrapper that runs only for a limited time.
// We need a UI message loop in the main thread.
struct TimedMsgLoop {
 public:
  void RunFor(int seconds) {
    loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask, 1000 * seconds);
    loop_.MessageLoop::Run();
  }

  void Quit() {
    loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask);
  }

  MessageLoopForUI loop_;
};

template <> struct RunnableMethodTraits<TimedMsgLoop> {
  void RetainCallee(TimedMsgLoop* obj) {}
  void ReleaseCallee(TimedMsgLoop* obj) {}
};

// Saves typing. It's somewhat hard to create a wrapper around
// testing::InvokeWithoutArgs since it returns a
// non-public (testing::internal) type.
#define QUIT_LOOP(loop) testing::InvokeWithoutArgs(\
    CreateFunctor(&loop, &TimedMsgLoop::Quit))

// We mock ChromeFrameDelegate only. The rest is with real AutomationProxy
TEST(CFACWithChrome, CreateTooFast) {
  MockCFDelegate cfd;
  TimedMsgLoop loop;
  int timeout = 0;  // Chrome cannot send Hello message so fast.
  const std::wstring profile = L"Adam.N.Epilinter";

  scoped_ptr<ChromeFrameAutomationClient> client;
  client.reset(new ChromeFrameAutomationClient());

  EXPECT_CALL(cfd, OnAutomationServerLaunchFailed(AUTOMATION_TIMEOUT,
                                                  testing::_))
      .Times(1)
      .WillOnce(QUIT_LOOP(loop));

  EXPECT_TRUE(client->Initialize(&cfd, timeout, false, profile, L"", false));
  loop.RunFor(10);
  client->Uninitialize();
}

// This test may fail if Chrome take more that 10 seconds (timeout var) to
// launch. In this case GMock shall print something like "unexpected call to
// OnAutomationServerLaunchFailed". I'm yet to find out how to specify
// that this is an unexpected call, and still to execute and action.
TEST(CFACWithChrome, CreateNotSoFast) {
  MockCFDelegate cfd;
  TimedMsgLoop loop;
  const std::wstring profile = L"Adam.N.Epilinter";
  int timeout = 10000;

  scoped_ptr<ChromeFrameAutomationClient> client;
  client.reset(new ChromeFrameAutomationClient);

  EXPECT_CALL(cfd, OnAutomationServerReady())
      .Times(1)
      .WillOnce(QUIT_LOOP(loop));

  EXPECT_CALL(cfd, OnAutomationServerLaunchFailed(testing::_, testing::_))
      .Times(0);

  EXPECT_TRUE(client->Initialize(&cfd, timeout, false, profile, L"", false));

  loop.RunFor(11);
  client->Uninitialize();
  client.reset(NULL);
}

MATCHER_P(MsgType, msg_type, "IPC::Message::type()") {
  const IPC::Message& m = arg;
  return (m.type() == msg_type);
}

MATCHER_P(EqNavigationInfoUrl, url, "IPC::NavigationInfo matcher") {
  if (url.is_valid() && url != arg.url)
    return false;
  // TODO: other members
  return true;
}

TEST(CFACWithChrome, NavigateOk) {
  MockCFDelegate cfd;
  TimedMsgLoop loop;
  const std::wstring profile = L"Adam.N.Epilinter";
  const std::string url = "about:version";
  int timeout = 10000;

  scoped_ptr<ChromeFrameAutomationClient> client;
  client.reset(new ChromeFrameAutomationClient);

  EXPECT_CALL(cfd, OnAutomationServerReady())
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
          client.get(), &ChromeFrameAutomationClient::InitiateNavigation,
          url, std::string(), false))));

//  cfd.SetOnNavigationStateChanged();
  EXPECT_CALL(cfd,
      OnNavigationStateChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());

  {
    testing::InSequence s;

    EXPECT_CALL(cfd, OnDidNavigate(testing::_, EqNavigationInfoUrl(GURL())))
        .Times(1);

    EXPECT_CALL(cfd, OnUpdateTargetUrl(testing::_, testing::_)).Times(1);

    EXPECT_CALL(cfd, OnLoad(testing::_, testing::_))
        .Times(1)
        .WillOnce(QUIT_LOOP(loop));
  }

  EXPECT_TRUE(client->Initialize(&cfd, timeout, false, profile, L"", false));
  loop.RunFor(10);
  client->Uninitialize();
  client.reset(NULL);
}

// Bug: http://b/issue?id=2033644
TEST(CFACWithChrome, DISABLED_NavigateFailed) {
  MockCFDelegate cfd;
  TimedMsgLoop loop;
  const std::wstring profile = L"Adam.N.Epilinter";
  const std::string url = "http://127.0.0.3:65412/";
  int timeout = 10000;

  scoped_ptr<ChromeFrameAutomationClient> client;
  client.reset(new ChromeFrameAutomationClient);

  EXPECT_CALL(cfd, OnAutomationServerReady())
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
          client.get(), &ChromeFrameAutomationClient::InitiateNavigation,
          url, std::string(), false))));

  EXPECT_CALL(cfd,
    OnNavigationStateChanged(testing::_, testing::_))
    .Times(testing::AnyNumber());

  EXPECT_CALL(cfd, OnNavigationFailed(testing::_, testing::_, testing::_))
      .Times(1);

  EXPECT_CALL(cfd, OnUpdateTargetUrl(testing::_, testing::_))
      .Times(testing::AnyNumber());

  EXPECT_CALL(cfd, OnLoad(testing::_, testing::_))
      .Times(0);

  EXPECT_TRUE(client->Initialize(&cfd, timeout, false, profile, L"", false));

  loop.RunFor(10);
  client->Uninitialize();
  client.reset(NULL);
}

MATCHER_P(EqURLRequest, x, "IPC::AutomationURLRequest matcher") {
  if (arg.url != x.url)
    return false;
  if (arg.method != x.method)
    return false;
  if (arg.referrer != x.referrer)
    return false;
  if (arg.extra_request_headers != x.extra_request_headers)
    return false;
  // TODO: uploaddata member
  return true;
}

MATCHER_P(EqUrlGet, url, "Quick URL matcher for 'HTTP GET' request") {
  if (arg.url != url)
    return false;
  if (arg.method != "GET")
    return false;
  return true;
}

TEST(CFACWithChrome, UseHostNetworkStack) {
  MockCFDelegate cfd;
  TimedMsgLoop loop;
  const std::wstring profile = L"Adam.N.Epilinter";
  const std::string url = "http://bongo.com";
  int timeout = 10000;

  scoped_ptr<ChromeFrameAutomationClient> client;
  client.reset(new ChromeFrameAutomationClient);
  client->set_use_chrome_network(false);
  cfd.SetAutomationSender(client.get());

  EXPECT_CALL(cfd, OnAutomationServerReady())
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
          client.get(), &ChromeFrameAutomationClient::InitiateNavigation,
          url, std::string(), false))));

  EXPECT_CALL(cfd, OnNavigationStateChanged(testing::_, testing::_))
      .Times(testing::AnyNumber());

  EXPECT_CALL(cfd, GetBounds(testing::_))
      .Times(testing::AtMost(1));

  EXPECT_CALL(cfd, OnUpdateTargetUrl(testing::_, testing::_))
      .Times(testing::AnyNumber());

  // Note slash appending to the url string, because of GURL inside chrome
  const IPC::AutomationURLResponse found = {"", "HTTP/0.9 200\r\n\r\n\r\n", };

  // Hard coded tab and request ids
  static const int tab_id = 1;
  int request_id = 2;

  EXPECT_CALL(cfd, OnRequestStart(tab_id, request_id, EqUrlGet(url + '/')))
      .Times(1)
      .WillOnce(testing::Invoke(CreateFunctor(&cfd,
                                              &MockCFDelegate::ReplyStarted,
                                              &found)));

  // Return some trivial page, that have a link to a "logo.gif" image
  const std::string data = "<!DOCTYPE html><title>Hello</title>"
                           "<img src=\"logo.gif\">";
  EXPECT_CALL(cfd, OnRequestRead(tab_id, request_id, testing::Ge(0)))
      .Times(2)
      .WillOnce(testing::Invoke(CreateFunctor(&cfd, &MockCFDelegate::ReplyData,
                                &data)))
      .WillOnce(testing::WithArgs<0, 1>(testing::Invoke(CreateFunctor(&cfd,
                                                 &MockCFDelegate::ReplyEOF))));

  EXPECT_CALL(cfd, OnDidNavigate(tab_id, EqNavigationInfoUrl(GURL(url))))
      .Times(1);
  EXPECT_CALL(cfd, OnLoad(tab_id, GURL(url)))
      .Times(1);

  // Expect request for logo.gif
  request_id++;
  EXPECT_CALL(cfd,
      OnRequestStart(tab_id, request_id, EqUrlGet(url + "/logo.gif")))
          .Times(1)
          .WillOnce(testing::Invoke(CreateFunctor(&cfd,
                                                  &MockCFDelegate::Reply404)));

  EXPECT_CALL(cfd, OnRequestRead(tab_id, request_id, testing::_))
      .Times(testing::AtMost(1));

  // Chrome makes a brave request for favicon.ico
  request_id++;
  EXPECT_CALL(cfd,
      OnRequestStart(tab_id, request_id, EqUrlGet(url + "/favicon.ico")))
          .Times(1)
          .WillOnce(testing::Invoke(CreateFunctor(&cfd,
                                                  &MockCFDelegate::Reply404)));

  EXPECT_CALL(cfd, OnRequestRead(tab_id, request_id, testing::_))
      .Times(testing::AtMost(1));

  bool incognito = true;
  EXPECT_TRUE(client->Initialize(&cfd, timeout, false, profile, L"",
                                 incognito));

  loop.RunFor(10);
  client->Uninitialize();
  client.reset(NULL);
}


// [CFAC] -- uses a ProxyFactory for creation of ChromeFrameAutomationProxy
// -- uses ChromeFrameAutomationProxy
// -- uses TabProxy obtained from ChromeFrameAutomationProxy
// -- uses ChromeFrameDelegate as outgoing interface
//
// We mock ProxyFactory to return mock object (MockAutomationProxy) implementing
// ChromeFrameAutomationProxy interface.
// Since CFAC uses TabProxy for few calls and TabProxy is not easy mockable,
// we create 'real' TabProxy but with fake AutomationSender (the one responsible
// for sending messages over channel).
// Additionally we have mock implementation ChromeFrameDelagate interface -
// MockCFDelegate.

// Test fixture, saves typing all of it's members.
class CFACMockTest : public testing::Test {
 public:
  MockProxyFactory factory_;
  MockCFDelegate   cfd_;
  TimedMsgLoop loop_;
  MockAutomationProxy proxy_;
  scoped_ptr<AutomationHandleTracker> tracker_;
  MockAutomationMessageSender dummy_sender_;
  scoped_refptr<TabProxy> tab_;
  scoped_ptr<ChromeFrameAutomationClient> client_;  // the victim of all tests

  std::wstring profile_;
  int timeout_;
  void* id_;  // Automation server id we are going to return
  int tab_handle_;   // Tab handle. Any non-zero value is Ok.

  inline ChromeFrameAutomationProxy* get_proxy() {
    return static_cast<ChromeFrameAutomationProxy*>(&proxy_);
  }

  inline void CreateTab() {
    ASSERT_EQ(NULL, tab_.get());
    tab_ = new TabProxy(&dummy_sender_, tracker_.get(), tab_handle_);
  }

  // Easy methods to set expectations.
  void SetAutomationServerOk() {
    EXPECT_CALL(factory_, GetAutomationServer(testing::Eq(timeout_),
        testing::StrEq(profile_),
        testing::_,
        testing::_,
        testing::NotNull()))
    .Times(1)
    .WillOnce(testing::DoAll(
        testing::WithArgs<0, 4>(
            testing::Invoke(CreateFunctor(&factory_,
                                          &MockProxyFactory::GetServerImpl,
                                          get_proxy(), AUTOMATION_SUCCESS))),
        testing::Return(id_)));

    EXPECT_CALL(factory_, ReleaseAutomationServer(testing::Eq(id_))).Times(1);
  }

  void Set_CFD_LaunchFailed(AutomationLaunchResult result) {
    EXPECT_CALL(cfd_, OnAutomationServerLaunchFailed(
        testing::Eq(result), testing::_))
        .Times(1)
        .WillOnce(QUIT_LOOP(loop_));
  }

 protected:
  CFACMockTest() : tracker_(NULL), timeout_(500),
      profile_(L"Adam.N.Epilinter") {
    id_ = reinterpret_cast<void*>(5);
    tab_handle_ = 3;
  }

  virtual void SetUp() {
    dummy_sender_.ForwardTo(&proxy_);
    tracker_.reset(new AutomationHandleTracker(&dummy_sender_));

    client_.reset(new ChromeFrameAutomationClient);
    client_->set_proxy_factory(&factory_);
  }
};

// Could be implemented as MockAutomationProxy member (we have WithArgs<>!)
ACTION_P3(HandleCreateTab, tab_handle, external_tab_container, tab_wnd) {
  // arg0 - message
  // arg1 - callback
  // arg2 - key
  CallbackRunner<Tuple3<HWND, HWND, int> >* c =
      reinterpret_cast<CallbackRunner<Tuple3<HWND, HWND, int> >*>(arg1);
  c->Run(external_tab_container, tab_wnd, tab_handle);
  delete c;
  delete arg0;
}

TEST_F(CFACMockTest, MockedCreateTabOk) {
  int timeout = 500;
  CreateTab();
  SetAutomationServerOk();

  EXPECT_CALL(proxy_, server_version()).Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(""));

  // We need some valid HWNDs, when responding to CreateExternalTab
  HWND h1 = ::GetDesktopWindow();
  HWND h2 = ::GetDesktopWindow();
  EXPECT_CALL(proxy_, SendAsAsync(testing::Property(&IPC::SyncMessage::type,
                                 AutomationMsg_CreateExternalTab__ID),
                                 testing::NotNull(), testing::_))
      .Times(1)
      .WillOnce(HandleCreateTab(tab_handle_, h1, h2));

  EXPECT_CALL(proxy_, CreateTabProxy(testing::Eq(tab_handle_)))
      .WillOnce(testing::Return(tab_));

  EXPECT_CALL(cfd_, OnAutomationServerReady())
      .WillOnce(QUIT_LOOP(loop_));

  // Here we go!
  EXPECT_TRUE(client_->Initialize(&cfd_, timeout, false, profile_, L"", false));
  loop_.RunFor(10);
  client_->Uninitialize();
}

TEST_F(CFACMockTest, MockedCreateTabFailed) {
  HWND null_wnd = NULL;
  SetAutomationServerOk();

  EXPECT_CALL(proxy_, server_version()).Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(""));

  EXPECT_CALL(proxy_, SendAsAsync(testing::Property(&IPC::SyncMessage::type,
                                  AutomationMsg_CreateExternalTab__ID),
                                  testing::NotNull(), testing::_))
      .Times(1)
      .WillOnce(HandleCreateTab(tab_handle_, null_wnd, null_wnd));

  EXPECT_CALL(proxy_, CreateTabProxy(testing::_)).Times(0);

  Set_CFD_LaunchFailed(AUTOMATION_CREATE_TAB_FAILED);

  // Here we go!
  EXPECT_TRUE(client_->Initialize(&cfd_, timeout_, false, profile_, L"",
              false));
  loop_.RunFor(4);
  client_->Uninitialize();
}

const wchar_t kMetaTagPage[] = L"files/meta_tag.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_MetaTag) {
  SimpleBrowserTest(IE, kMetaTagPage, L"meta_tag");
}

const wchar_t kCFProtocolPage[] = L"files/cf_protocol.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_CFProtocol) {
  SimpleBrowserTest(IE, kCFProtocolPage, L"chrome_frame_protocol");
}

const wchar_t kPersistentCookieTest[] =
    L"files/persistent_cookie_test_page.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_PersistentCookieTest) {
  SimpleBrowserTest(IE, kPersistentCookieTest, L"PersistentCookieTest");
}

const wchar_t kNavigateOutPage[] = L"files/navigate_out.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_NavigateOut) {
  SimpleBrowserTest(IE, kNavigateOutPage, L"navigate_out");
}

const wchar_t kReferrerMainTest[] = L"files/referrer_main.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_ReferrerTest) {
  SimpleBrowserTest(IE, kReferrerMainTest, L"FullTab_ReferrerTest");
}

HRESULT LaunchIEAsComServer(IWebBrowser2** web_browser) {
  if (!web_browser)
    return E_INVALIDARG;

  ScopedComPtr<IWebBrowser2> web_browser2;
  HRESULT hr = CoCreateInstance(
      CLSID_InternetExplorer, NULL, CLSCTX_LOCAL_SERVER, IID_IWebBrowser2,
      reinterpret_cast<void**>(web_browser2.Receive()));

  if (SUCCEEDED(hr)) {
    *web_browser = web_browser2.Detach();
  }

  return hr;
}

TEST(ChromeFrameTest, FullTabModeIE_DisallowedUrls) {
  int major_version = 0;
  int minor_version = 0;
  int bugfix_version = 0;

  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);
  if (major_version > 5) {
    DLOG(INFO) << __FUNCTION__ << " Not running test on Windows version: "
               << major_version;
    return;
  }

  IEVersion ie_version = GetIEVersion();
  if (ie_version == IE_8) {
    DLOG(INFO) << __FUNCTION__ << " Not running test on IE8";
    return;
  }

  HRESULT hr = CoInitialize(NULL);
  bool should_uninit = SUCCEEDED(hr);

  ScopedComPtr<IWebBrowser2> web_browser2;
  EXPECT_TRUE(S_OK == LaunchIEAsComServer(web_browser2.Receive()));
  web_browser2->put_Visible(VARIANT_TRUE);

  CComObject<WebBrowserEventSink>* web_browser_sink = NULL;
  CComObject<WebBrowserEventSink>::CreateInstance(&web_browser_sink);

  // Pass the main thread id to the browser sink so that it can notify
  // us about test completion.
  web_browser_sink->set_main_thread_id(GetCurrentThreadId());

  hr = web_browser_sink->DispEventAdvise(web_browser2,
                                         &DIID_DWebBrowserEvents2);
  EXPECT_TRUE(hr == S_OK);

  VARIANT empty = ScopedVariant::kEmptyVariant;
  ScopedVariant url;
  url.Set(L"cf:file:///C:/");

  TimedMsgLoop loop;

  hr = web_browser2->Navigate2(url.AsInput(), &empty, &empty, &empty, &empty);
  EXPECT_TRUE(hr == S_OK);

  loop.RunFor(10);

  EXPECT_TRUE(web_browser_sink->navigation_failed());

  hr = web_browser_sink->DispEventUnadvise(web_browser2);
  EXPECT_TRUE(hr == S_OK);

  web_browser2.Release();
  chrome_frame_test::CloseAllIEWindows();

  if (should_uninit) {
    CoUninitialize();
  }
}

