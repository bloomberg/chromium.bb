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
#include "base/scoped_variant_win.h"
#include "base/win_util.h"
#include "gmock/gmock.h"
#include "net/url_request/url_request_unittest.h"
#include "chrome_frame/test/chrome_frame_unittests.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/chrome_frame_delegate.h"
#include "chrome_frame/crash_reporting/vectored_handler-impl.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/helper.h"

#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

using testing::CreateFunctor;

const wchar_t kDocRoot[] = L"chrome_frame\\test\\data";
const int kLongWaitTimeout = 60 * 1000;
const int kShortWaitTimeout = 25 * 1000;

void ChromeFrameTestWithWebServer::CloseAllBrowsers() {
  // Web browsers tend to relaunch themselves in other processes, meaning the
  // KillProcess stuff above might not have actually cleaned up all our browser
  // instances, so make really sure browsers are dead.
  base::KillProcesses(chrome_frame_test::kIEImageName, 0, NULL);
  base::KillProcesses(chrome_frame_test::kIEBrokerImageName, 0, NULL);
  base::KillProcesses(chrome_frame_test::kFirefoxImageName, 0, NULL);
  base::KillProcesses(chrome_frame_test::kSafariImageName, 0, NULL);

  // Endeavour to only kill off Chrome Frame derived Chrome processes.
  KillAllNamedProcessesWithArgument(chrome_frame_test::kChromeImageName,
                                    UTF8ToWide(switches::kChromeFrame));
}

void ChromeFrameTestWithWebServer::SetUp() {
  // Make sure our playground is clean before we start.
  CloseAllBrowsers();

  server_.SetUp();
  results_dir_ = server_.GetDataDir();
  file_util::AppendToPath(&results_dir_, L"dump");
}

void ChromeFrameTestWithWebServer::TearDown() {
  CloseBrowser();

  CloseAllBrowsers();

  server_.TearDown();
}

bool ChromeFrameTestWithWebServer::LaunchBrowser(BrowserKind browser,
                                               const wchar_t* page) {
  std::wstring url = page;
  if (url.find(L"files/") != std::wstring::npos)
    url = UTF8ToWide(server_.Resolve(page).spec());

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
      LOG(ERROR) << L"Forcefully killing browser process. Exit:" << exit_code;
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
    LOG(ERROR) << "Error text: " << (data.empty() ? "<empty>" : data.c_str());
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
    LOG(ERROR) << "Failed to launch browser " << ToString(browser);
  } else {
    ASSERT_TRUE(WaitForTestToComplete(kLongWaitTimeout));
    ASSERT_TRUE(CheckResultFile(result_file_to_check, "OK"));
  }
}

void ChromeFrameTestWithWebServer::VersionTest(BrowserKind browser,
    const wchar_t* page, const wchar_t* result_file_to_check) {
  std::wstring plugin_path;
  PathService::Get(base::DIR_MODULE, &plugin_path);
  file_util::AppendToPath(&plugin_path, L"servers");
  file_util::AppendToPath(&plugin_path, kChromeFrameDllName);

  static FileVersionInfo* version_info =
      FileVersionInfo::CreateFileVersionInfo(plugin_path);

  std::wstring version;
  if (version_info)
    version = version_info->product_version();

  // If we can't find the Chrome Frame DLL in the src tree, we turn to
  // the directory where chrome is installed.
  if (!version_info) {
    installer::Version* ver_system = InstallUtil::GetChromeVersion(true);
    installer::Version* ver_user = InstallUtil::GetChromeVersion(false);
    ASSERT_TRUE(ver_system || ver_user);

    bool system_install = ver_system ? true : false;
    std::wstring cf_dll_path(installer::GetChromeInstallPath(system_install));
    file_util::AppendToPath(&cf_dll_path,
        ver_system ? ver_system->GetString() : ver_user->GetString());
    file_util::AppendToPath(&cf_dll_path, kChromeFrameDllName);
    version_info = FileVersionInfo::CreateFileVersionInfo(cf_dll_path);
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
    LOG(ERROR) << "Failed to launch browser " << ToString(OPERA);
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

// This test randomly fails on the ChromeFrame builder.
// Bug http://code.google.com/p/chromium/issues/detail?id=31532
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeFF_CFInstanceSingleton) {
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
TEST_F(ChromeFrameTestWithWebServer,
       DISABLED_WidgetModeIE_CFInstanceIfrOnLoad) {
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

// http://crbug.com/32321
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeIE_CFInstanceIfrPost) {
  SimpleBrowserTest(IE, kCFIIfrPostPage, L"CFInstanceIfrPost");
}

// Flakes out on the bots, http://crbug.com/26372
TEST_F(ChromeFrameTestWithWebServer,
       FLAKY_WidgetModeFF_CFInstanceIfrPost) {
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

// This test randomly fails on the ChromeFrame builder.
// Bug http://code.google.com/p/chromium/issues/detail?id=31532
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeFF_CFInstancePost) {
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

// This test randomly fails on the ChromeFrame builder.
// Bug http://code.google.com/p/chromium/issues/detail?id=31532
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeFF_CFInstanceRPC) {
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
TEST_F(ChromeFrameTestWithWebServer,
       DISABLED_WidgetModeFF_CFInstanceRPCInternal) {
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

const wchar_t kCFInstallBasicTestPage[] = L"files/CFInstall_basic.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabIE_CFInstallBasic) {
  SimpleBrowserTest(IE, kCFInstallBasicTestPage, L"CFInstallBasic");
}

const wchar_t kCFInstallPlaceTestPage[] = L"files/CFInstall_place.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabIE_CFInstallPlace) {
  SimpleBrowserTest(IE, kCFInstallPlaceTestPage, L"CFInstallPlace");
}

const wchar_t kCFInstallOverlayTestPage[] = L"files/CFInstall_overlay.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabIE_CFInstallOverlay) {
  SimpleBrowserTest(IE, kCFInstallOverlayTestPage, L"CFInstallOverlay");
}

const wchar_t kCFInstallDismissTestPage[] = L"files/CFInstall_dismiss.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabIE_CFInstallDismiss) {
  SimpleBrowserTest(IE, kCFInstallDismissTestPage, L"CFInstallDismiss");
}

const wchar_t kInitializeHiddenPage[] = L"files/initialize_hidden.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_InitializeHidden) {
  SimpleBrowserTest(IE, kInitializeHiddenPage, L"InitializeHidden");
}

const wchar_t kFullTabHttpHeaderPage[] = L"files/chrome_frame_http_header.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_CFHttpHeaderBasic) {
  SimpleBrowserTest(IE, kFullTabHttpHeaderPage, L"FullTabHttpHeader");
}

const wchar_t kFullTabHttpHeaderPageIFrame[] =
    L"files/chrome_frame_http_header_host.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_CFHttpHeaderIFrame) {
  SimpleBrowserTest(IE, kFullTabHttpHeaderPageIFrame,
                    L"FullTabHttpHeaderIFrame");
}

const wchar_t kFullTabHttpHeaderPageFrameset[] =
    L"files/chrome_frame_http_header_frameset.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_CFHttpHeaderFrameSet) {
  SimpleBrowserTest(IE, kFullTabHttpHeaderPageFrameset,
                    L"FullTabHttpHeaderFrameset");
}

// Flaky on the build bots. See http://crbug.com/30622
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeFF_InitializeHidden) {
  SimpleBrowserTest(FIREFOX, kInitializeHiddenPage, L"InitializeHidden");
}

// Disabled due to a problem with Opera.
// http://b/issue?id=1708275
TEST_F(ChromeFrameTestWithWebServer,
       DISABLED_WidgetModeOpera_InitializeHidden) {
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

// http://crbug.com/32321
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeIE_PrivilegedApis) {
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

  ChromeFrameLaunchParams params;
  params.automation_server_launch_timeout = 0;
  params.profile_name = L"Adam.N.Epilinter";
  params.extra_chrome_arguments = L"";
  params.perform_version_check = false;
  params.incognito_mode = false;

  void* id = NULL;
  f.GetAutomationServer(&d, params, &id);
  f.ReleaseAutomationServer(id);
}

TEST(ProxyFactoryTest, CreateSameProfile) {
  ProxyFactory f;
  LaunchDelegateMock d;
  EXPECT_CALL(d, LaunchComplete(testing::NotNull(), testing::_)).Times(2);

  ChromeFrameLaunchParams params;
  params.automation_server_launch_timeout = 0;
  params.profile_name = L"Dr. Gratiano Forbeson";
  params.extra_chrome_arguments = L"";
  params.perform_version_check = false;
  params.incognito_mode = false;

  void* i1 = NULL;
  void* i2 = NULL;

  f.GetAutomationServer(&d, params, &i1);
  f.GetAutomationServer(&d, params, &i2);

  EXPECT_EQ(i1, i2);
  f.ReleaseAutomationServer(i2);
  f.ReleaseAutomationServer(i1);
}

TEST(ProxyFactoryTest, CreateDifferentProfiles) {
  ProxyFactory f;
  LaunchDelegateMock d;
  EXPECT_CALL(d, LaunchComplete(testing::NotNull(), testing::_)).Times(2);

  ChromeFrameLaunchParams params1;
  params1.automation_server_launch_timeout = 0;
  params1.profile_name = L"Adam.N.Epilinter";
  params1.extra_chrome_arguments = L"";
  params1.perform_version_check = false;
  params1.incognito_mode = false;

  ChromeFrameLaunchParams params2;
  params2.automation_server_launch_timeout = 0;
  params2.profile_name = L"Dr. Gratiano Forbeson";
  params2.extra_chrome_arguments = L"";
  params2.perform_version_check = false;
  params2.incognito_mode = false;

  void* i1 = NULL;
  void* i2 = NULL;

  f.GetAutomationServer(&d, params1, &i1);
  f.GetAutomationServer(&d, params2, &i2);

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
  MOCK_METHOD4(OnHandleContextMenu, void(int tab_handle, HANDLE menu_handle,
      int align_flags, const IPC::ContextMenuParams& params));
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

  IPC::Message::Sender* automation_;
};

class MockProxyFactory : public ProxyFactory {
 public:
  MOCK_METHOD3(GetAutomationServer, void (ProxyFactory::LaunchDelegate*,
      const ChromeFrameLaunchParams& params, void** automation_server_id));
  MOCK_METHOD1(ReleaseAutomationServer, bool(void* id));

  MockProxyFactory() : thread_("mock factory worker") {
    thread_.Start();
    loop_ = thread_.message_loop();
  }

  // Fake implementation
  void GetServerImpl(ChromeFrameAutomationProxy* pxy,
                     void* proxy_id,
                     AutomationLaunchResult result,
                     LaunchDelegate* d,
                     const ChromeFrameLaunchParams& params,
                     void** automation_server_id) {
    *automation_server_id = proxy_id;
    Task* task = NewRunnableMethod(d,
        &ProxyFactory::LaunchDelegate::LaunchComplete, pxy, result);
    loop_->PostDelayedTask(FROM_HERE, task,
                           params.automation_server_launch_timeout/2);
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
  MOCK_METHOD1(SetEnableExtensionAutomation,
      void(const std::vector<std::string>&));

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

template <> struct RunnableMethodTraits<chrome_frame_test::TimedMsgLoop> {
  void RetainCallee(chrome_frame_test::TimedMsgLoop* obj) {}
  void ReleaseCallee(chrome_frame_test::TimedMsgLoop* obj) {}
};

// Saves typing. It's somewhat hard to create a wrapper around
// testing::InvokeWithoutArgs since it returns a
// non-public (testing::internal) type.
#define QUIT_LOOP(loop) testing::InvokeWithoutArgs(\
    CreateFunctor(&loop, &chrome_frame_test::TimedMsgLoop::Quit))

#define QUIT_LOOP_SOON(loop, seconds) testing::InvokeWithoutArgs(\
    CreateFunctor(&loop, &chrome_frame_test::TimedMsgLoop::QuitAfter, \
                  seconds))

// We mock ChromeFrameDelegate only. The rest is with real AutomationProxy
TEST(CFACWithChrome, CreateTooFast) {
  MockCFDelegate cfd;
  chrome_frame_test::TimedMsgLoop loop;
  int timeout = 0;  // Chrome cannot send Hello message so fast.
  const std::wstring profile = L"Adam.N.Epilinter";

  scoped_refptr<ChromeFrameAutomationClient> client;
  client = new ChromeFrameAutomationClient();

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
  chrome_frame_test::TimedMsgLoop loop;
  const std::wstring profile = L"Adam.N.Epilinter";
  int timeout = 10000;

  scoped_refptr<ChromeFrameAutomationClient> client;
  client = new ChromeFrameAutomationClient;

  EXPECT_CALL(cfd, OnAutomationServerReady())
      .Times(1)
      .WillOnce(QUIT_LOOP(loop));

  EXPECT_CALL(cfd, OnAutomationServerLaunchFailed(testing::_, testing::_))
      .Times(0);

  EXPECT_TRUE(client->Initialize(&cfd, timeout, false, profile, L"", false));

  loop.RunFor(11);
  client->Uninitialize();
  client = NULL;
}

MATCHER_P(MsgType, msg_type, "IPC::Message::type()") {
  const IPC::Message& m = arg;
  return (m.type() == msg_type);
}

MATCHER_P(EqNavigationInfoUrl, url, "IPC::NavigationInfo matcher") {
  if (url.is_valid() && url != arg.url)
    return false;
  // TODO(stevet): other members
  return true;
}

TEST(CFACWithChrome, NavigateOk) {
  MockCFDelegate cfd;
  chrome_frame_test::TimedMsgLoop loop;
  const std::wstring profile = L"Adam.N.Epilinter";
  const std::string url = "about:version";
  int timeout = 10000;

  scoped_refptr<ChromeFrameAutomationClient> client;
  client = new ChromeFrameAutomationClient;

  EXPECT_CALL(cfd, OnAutomationServerReady())
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
          client.get(), &ChromeFrameAutomationClient::InitiateNavigation,
          url, std::string(), false))));

  EXPECT_CALL(cfd, GetBounds(testing::_)).Times(testing::AnyNumber());

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
  client = NULL;
}

// Bug: http://b/issue?id=2033644
TEST(CFACWithChrome, DISABLED_NavigateFailed) {
  MockCFDelegate cfd;
  chrome_frame_test::TimedMsgLoop loop;
  const std::wstring profile = L"Adam.N.Epilinter";
  const std::string url = "http://127.0.0.3:65412/";
  int timeout = 10000;

  scoped_refptr<ChromeFrameAutomationClient> client;
  client = new ChromeFrameAutomationClient;

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
  client = NULL;
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
  chrome_frame_test::TimedMsgLoop loop_;
  MockAutomationProxy proxy_;
  scoped_ptr<AutomationHandleTracker> tracker_;
  MockAutomationMessageSender dummy_sender_;
  scoped_refptr<TabProxy> tab_;
  // the victim of all tests
  scoped_refptr<ChromeFrameAutomationClient> client_;

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
    EXPECT_CALL(factory_, GetAutomationServer(testing::NotNull(),
        testing::Field(&ChromeFrameLaunchParams::profile_name,
                       testing::StrEq(profile_)),
        testing::NotNull()))
    .Times(1)
    .WillOnce(testing::Invoke(
        CreateFunctor(&factory_, &MockProxyFactory::GetServerImpl,
                      get_proxy(),
                      id_,
                      AUTOMATION_SUCCESS)));

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

    client_ = new ChromeFrameAutomationClient;
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

  EXPECT_CALL(proxy_, CancelAsync(testing::_)).Times(testing::AnyNumber());

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

  EXPECT_CALL(proxy_, CancelAsync(testing::_)).Times(testing::AnyNumber());

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

const wchar_t kSubFrameTestPage[] = L"files/full_tab_sub_frame_main.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_SubFrame) {
  SimpleBrowserTest(IE, kSubFrameTestPage, L"sub_frame");
}

const wchar_t kSubIFrameTestPage[] = L"files/full_tab_sub_iframe_main.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_SubIFrame) {
  SimpleBrowserTest(IE, kSubIFrameTestPage, L"sub_frame");
}

const int kChromeFrameLaunchDelay = 5;
const int kChromeFrameLongNavigationTimeoutInSeconds = 10;

// This class provides functionality to add expectations to IE full tab mode
// tests.
class MockWebBrowserEventSink : public chrome_frame_test::WebBrowserEventSink {
 public:
  // Needed to support PostTask.
  static bool ImplementsThreadSafeReferenceCounting() {
    return true;
  }

  MOCK_METHOD7_WITH_CALLTYPE(__stdcall, OnBeforeNavigate2,
                             HRESULT (IDispatch* dispatch,  // NOLINT
                                      VARIANT* url,
                                      VARIANT* flags,
                                      VARIANT* target_frame_name,
                                      VARIANT* post_data,
                                      VARIANT* headers,
                                      VARIANT_BOOL* cancel));

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, OnNavigateComplete2,
                             void (IDispatch* dispatch,  // NOLINT
                                   VARIANT* url));

  MOCK_METHOD5_WITH_CALLTYPE(__stdcall, OnNewWindow3,
                             void (IDispatch** dispatch,  // NOLINT
                                   VARIANT_BOOL* Cancel,
                                   DWORD flags,
                                   BSTR url_context,
                                   BSTR url));

  MOCK_METHOD5_WITH_CALLTYPE(__stdcall, OnNavigateError,
                             void (IDispatch* dispatch,  // NOLINT
                                   VARIANT* url,
                                   VARIANT* frame_name,
                                   VARIANT* status_code,
                                   VARIANT* cancel));

  MOCK_METHOD1(OnLoad, void (const wchar_t* url));    // NOLINT
  MOCK_METHOD1(OnLoadError, void (const wchar_t* url));    // NOLINT
  MOCK_METHOD1(OnMessage, void (const wchar_t* message));    // NOLINT
};

using testing::_;

const wchar_t kChromeFrameFileUrl[] = L"cf:file:///C:/";

TEST(ChromeFrameTest, FullTabModeIE_DisallowedUrls) {
  chrome_frame_test::TimedMsgLoop loop;
  // If a navigation fails then IE issues a navigation to an interstitial
  // page. Catch this to track navigation errors as the NavigateError
  // notification does not seem to fire reliably.
  CComObjectStackEx<MockWebBrowserEventSink> mock;

  EXPECT_CALL(mock,
              OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                testing::StrCaseEq(kChromeFrameFileUrl)),
                                _, _, _, _, _))
      .Times(1)
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock,
              OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                testing::StartsWith(L"res:")),
                                _, _, _, _, _))
      .Times(1)
      .WillOnce(testing::DoAll(
          QUIT_LOOP(loop),
          testing::Return(S_OK)));

  HRESULT hr = mock.LaunchIEAndNavigate(kChromeFrameFileUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  mock.Uninitialize();
  chrome_frame_test::CloseAllIEWindows();
}

const wchar_t kChromeFrameFullTabWindowOpenTestUrl[] =
    L"http://localhost:1337/files/chrome_frame_window_open.html";

const wchar_t kChromeFrameFullTabWindowOpenPopupUrl[] =
    L"http://localhost:1337/files/chrome_frame_window_open_popup.html";

// This test checks if window.open calls issued by a full tab mode ChromeFrame
// instance make it back to IE and then transitions back to Chrome as the
// window.open target page is supposed to render within Chrome.
TEST_F(ChromeFrameTestWithWebServer, DISABLED_FullTabModeIE_WindowOpen) {
  chrome_frame_test::TimedMsgLoop loop;
  CComObjectStackEx<MockWebBrowserEventSink> mock;

  ::testing::InSequence sequence;
  EXPECT_CALL(mock,
              OnBeforeNavigate2(
                  _, testing::Field(&VARIANT::bstrVal,
                  testing::StrCaseEq(kChromeFrameFullTabWindowOpenTestUrl)),
                  _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock,
              OnLoad(testing::StrEq(kChromeFrameFullTabWindowOpenTestUrl)))
      .WillOnce(testing::Return());

  EXPECT_CALL(mock,
              OnBeforeNavigate2(
                  _, testing::Field(&VARIANT::bstrVal,
                  testing::StrCaseEq(kChromeFrameFullTabWindowOpenPopupUrl)),
                  _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock,
              OnLoad(testing::StrEq(kChromeFrameFullTabWindowOpenPopupUrl)))
      .WillOnce(QUIT_LOOP_SOON(loop, 2));

  HRESULT hr = mock.LaunchIEAndNavigate(kChromeFrameFullTabWindowOpenTestUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  ASSERT_TRUE(CheckResultFile(L"ChromeFrameWindowOpenPopup", "OK"));

  mock.Uninitialize();
  chrome_frame_test::CloseAllIEWindows();
}

const wchar_t kSubFrameUrl1[] =
    L"http://localhost:1337/files/sub_frame1.html";

const wchar_t kChromeFrameAboutVersion[] =
    L"cf:about:version";

// This test launches chrome frame in full tab mode in IE by having IE navigate
// to cf:about:blank. It then looks for the chrome renderer window and posts
// the WM_RBUTTONDOWN/WM_RBUTTONUP messages to it, which bring up the context
// menu. This is followed by keyboard messages sent via SendInput to select
// the About chrome frame menu option, which would then bring up a new window
// with the chrome revision. The test finally checks for success by comparing
// the URL of the window being opened with cf:about:version, which indicates
// that the operation succeeded.
// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_AboutChromeFrame) {
  chrome_frame_test::TimedMsgLoop loop;
  CComObjectStackEx<MockWebBrowserEventSink> mock;

  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl1)),
                                _, _, _, _, _))
      .Times(2).WillRepeatedly(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .Times(2).WillRepeatedly(testing::Return());

  EXPECT_CALL(mock, OnLoad(testing::StrEq(kSubFrameUrl1)))
      .WillOnce(testing::InvokeWithoutArgs(
          &chrome_frame_test::ShowChromeFrameContextMenu));

  EXPECT_CALL(mock,
              OnNewWindow3(_, _, _, _,
                           testing::StrCaseEq(kChromeFrameAboutVersion)))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kSubFrameUrl1);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  mock.Uninitialize();
  chrome_frame_test::CloseAllIEWindows();
}

const wchar_t kChromeFrameFullTabModeKeyEventUrl[] = L"files/keyevent.html";

// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_ChromeFrameKeyboardTest) {
  chrome_frame_test::TimedMsgLoop loop;

  ASSERT_TRUE(LaunchBrowser(IE, kChromeFrameFullTabModeKeyEventUrl));

  // Allow some time for chrome to be launched.
  loop.RunFor(kChromeFrameLaunchDelay);

  HWND renderer_window = chrome_frame_test::GetChromeRendererWindow();
  EXPECT_TRUE(IsWindow(renderer_window));

  chrome_frame_test::SetKeyboardFocusToWindow(renderer_window, 1, 1);
  chrome_frame_test::SendInputToWindow(renderer_window, "Chrome");

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  chrome_frame_test::CloseAllIEWindows();
  ASSERT_TRUE(CheckResultFile(L"FullTab_KeyboardTest", "OK"));
}

const wchar_t kSubFrameUrl2[] =
    L"http://localhost:1337/files/sub_frame2.html";
const wchar_t kSubFrameUrl3[] =
    L"http://localhost:1337/files/sub_frame3.html";

// Hack to pass a reference to the argument instead of value. Passing by
// value evaluates the argument at the mock specification time which is
// not always ideal. For e.g. At the time of mock creation, web_browser2_
// pointer is not set up yet so by passing a reference to it instead of
// a value we allow it to be created later.
template <typename T> T** ReceivePointer(scoped_refptr<T>& p) {  // NOLINT
  return reinterpret_cast<T**>(&p);
}

// Full tab mode back/forward test
// Launch and navigate chrome frame to a set of URLs and test back forward
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_BackForward) {
  chrome_frame_test::TimedMsgLoop loop;
  CComObjectStackEx<MockWebBrowserEventSink> mock;
  ::testing::InSequence sequence;   // Everything in sequence

  // We will get two BeforeNavigate2/OnNavigateComplete2 notifications due to
  // switching from IE to CF.
  // Note that when going backwards, we don't expect that since the extra
  // navigational entries in the travel log should have been removed.

  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl1)),
                                _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl1)),
                                _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // Navigate to url 2 after the previous navigation is complete.
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kSubFrameUrl1)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(
              &mock, &chrome_frame_test::WebBrowserEventSink::Navigate,
              std::wstring(kSubFrameUrl2)))));

  // Expect BeforeNavigate/NavigateComplete twice here as well.
  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl2)),
                                _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl2)),
                                _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // Navigate to url 3 after the previous navigation is complete
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kSubFrameUrl2)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(
              &mock, &chrome_frame_test::WebBrowserEventSink::Navigate,
              std::wstring(kSubFrameUrl3)))));

  // We have reached url 3 and have two back entries for url 1 & 2
  // Go back to url 2 now
  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl3)),
                                _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl3)),
                                _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // Go back.
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kSubFrameUrl3)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(ReceivePointer(mock.web_browser2_),
                        &IWebBrowser::GoBack))));

  // We have reached url 2 and have 1 back & 1 forward entries for url 1 & 3
  // Go back to url 1 now
  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl2)),
                                _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  EXPECT_CALL(mock, OnLoad(testing::StrEq(kSubFrameUrl2)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(ReceivePointer(mock.web_browser2_),
                        &IWebBrowser::GoBack))));

  // We have reached url 1 and have 0 back & 2 forward entries for url 2 & 3
  // Go back to url 1 now
  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl1)),
                                _, _, _, _, _));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  EXPECT_CALL(mock, OnLoad(testing::StrEq(kSubFrameUrl1)))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(&mock,
              &chrome_frame_test::WebBrowserEventSink::Uninitialize)),
          testing::IgnoreResult(testing::InvokeWithoutArgs(
              &chrome_frame_test::CloseAllIEWindows)),
          QUIT_LOOP_SOON(loop, 2)));

  HRESULT hr = mock.LaunchIEAndNavigate(kSubFrameUrl1);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  mock.Uninitialize();
  chrome_frame_test::CloseAllIEWindows();
}


const wchar_t kChromeFrameAboutBlankUrl[] = L"cf:about:blank";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_ChromeFrameFocusTest) {
  chrome_frame_test::TimedMsgLoop loop;

  ASSERT_TRUE(LaunchBrowser(IE, kChromeFrameAboutBlankUrl));

  // Allow some time for chrome to be launched.
  loop.RunFor(kChromeFrameLaunchDelay);

  HWND renderer_window = chrome_frame_test::GetChromeRendererWindow();
  EXPECT_TRUE(IsWindow(renderer_window));

  DWORD renderer_thread_id = 0;
  DWORD renderer_process_id = 0;
  renderer_thread_id = GetWindowThreadProcessId(renderer_window,
                                                &renderer_process_id);

  AttachThreadInput(GetCurrentThreadId(), renderer_thread_id, TRUE);
  HWND focus_window = GetFocus();
  EXPECT_TRUE(focus_window == renderer_window);
  AttachThreadInput(GetCurrentThreadId(), renderer_thread_id, FALSE);

  chrome_frame_test::CloseAllIEWindows();
}

const wchar_t kAnchorUrl[] = L"http://localhost:1337/files/anchor.html";
const wchar_t kAnchor1Url[] = L"http://localhost:1337/files/anchor.html#a1";
const wchar_t kAnchor2Url[] = L"http://localhost:1337/files/anchor.html#a2";
const wchar_t kAnchor3Url[] = L"http://localhost:1337/files/anchor.html#a3";

// Full tab mode back/forward test
// Launch and navigate chrome frame to a set of URLs and test back forward
// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_BackForwardAnchor) {
  const char tab_enter_keystrokes[] = { VK_TAB, VK_RETURN, 0 };
  static const std::string tab_enter(tab_enter_keystrokes);
  chrome_frame_test::TimedMsgLoop loop;
  CComObjectStackEx<MockWebBrowserEventSink> mock;
  ::testing::InSequence sequence;   // Everything in sequence

  // We will get two BeforeNavigate2/OnNavigateComplete2 notifications due to
  // switching from IE to CF.
  // Note that when going backwards, we don't expect that since the extra
  // navigational entries in the travel log should have been removed.
  // Same for navigating to anchors within a page that's already loaded.

  // Back/Forward state at this point:
  // Back: 0
  // Forward: 0
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchorUrl)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchorUrl)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // Navigate to anchor 1:
  // - First set focus to chrome renderer window
  //   Call WebBrowserEventSink::SetFocusToChrome only once
  //   in the beginning. Calling it again will change focus from the
  //   current location to an element near the simulated mouse.click.
  // - Then send keyboard input of TAB + ENTER to cause navigation.
  //   It's better to send input as PostDelayedTask since the Activex
  //   message loop on the other side might be blocked when we get
  //   called in Onload.
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchorUrl)))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(&mock,
              &chrome_frame_test::WebBrowserEventSink::SetFocusToChrome)),
          testing::InvokeWithoutArgs(CreateFunctor(&loop,
              &chrome_frame_test::TimedMsgLoop::PostDelayedTask, FROM_HERE,
              NewRunnableMethod(
                  &mock,
                  &chrome_frame_test::WebBrowserEventSink::SendInputToChrome,
                  std::string(tab_enter)), 0))));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor1Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // Navigate to anchor 2 after the previous navigation is complete
  // Back/Forward state at this point:
  // Back: 1 (kAnchorUrl)
  // Forward: 0
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor1Url)))
      .WillOnce(testing::InvokeWithoutArgs(
          CreateFunctor(
              &loop, &chrome_frame_test::TimedMsgLoop::PostDelayedTask,
              FROM_HERE,
              NewRunnableMethod(
                  &mock,
                  &chrome_frame_test::WebBrowserEventSink::SendInputToChrome,
                  std::string(tab_enter)), 0)));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor2Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // Navigate to anchor 3 after the previous navigation is complete
  // Back/Forward state at this point:
  // Back: 2 (kAnchorUrl, kAnchor1Url)
  // Forward: 0
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor2Url)))
      .WillOnce(testing::InvokeWithoutArgs(
          CreateFunctor(
              &loop, &chrome_frame_test::TimedMsgLoop::PostDelayedTask,
              FROM_HERE,
              NewRunnableMethod(
                  &mock,
                  &chrome_frame_test::WebBrowserEventSink::SendInputToChrome,
                  std::string(tab_enter)), 0)));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor3Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // We will reach anchor 3 once the navigation is complete,
  // then go back to anchor 2
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 0
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor3Url)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(ReceivePointer(mock.web_browser2_),
                        &IWebBrowser::GoBack))));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor2Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // We will reach anchor 2 once the navigation is complete,
  // then go back to anchor 1
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 1 (kAnchor3Url)
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor2Url)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(ReceivePointer(mock.web_browser2_),
                        &IWebBrowser::GoBack))));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor1Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // We will reach anchor 1 once the navigation is complete,
  // now go forward to anchor 2
  // Back/Forward state at this point:
  // Back: 2 (kAnchorUrl, kAnchor1Url)
  // Forward: 2 (kAnchor2Url, kAnchor3Url)
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor1Url)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(ReceivePointer(mock.web_browser2_),
                        &IWebBrowser::GoForward))));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor2Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // We have reached anchor 2, go forward to anchor 3 again
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 1 (kAnchor3Url)
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor2Url)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(ReceivePointer(mock.web_browser2_),
                        &IWebBrowser::GoForward))));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor3Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // We have gone a few steps back and forward, this should be enough for now.
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor3Url)))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(&mock,
              &chrome_frame_test::WebBrowserEventSink::Uninitialize)),
          testing::IgnoreResult(testing::InvokeWithoutArgs(
              &chrome_frame_test::CloseAllIEWindows)),
          QUIT_LOOP_SOON(loop, 2)));

  HRESULT hr = mock.LaunchIEAndNavigate(kAnchorUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  mock.Uninitialize();
  chrome_frame_test::CloseAllIEWindows();
}

const wchar_t kChromeFrameFullTabModeXMLHttpRequestTestUrl[] =
    L"files/xmlhttprequest_test.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_ChromeFrameXHRTest) {
  chrome_frame_test::TimedMsgLoop loop;

  ASSERT_TRUE(LaunchBrowser(IE, kChromeFrameFullTabModeXMLHttpRequestTestUrl));

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  chrome_frame_test::CloseAllIEWindows();
  ASSERT_TRUE(CheckResultFile(L"FullTab_XMLHttpRequestTest", "OK"));
}

const wchar_t kMultipleCFInstancesTestUrl[] =
    L"files/multiple_cf_instances_main.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_MultipleCFInstances) {
  SimpleBrowserTest(IE, kMultipleCFInstancesTestUrl,
                    L"WidgetMode_MultipleInstancesTest");
}

// TODO(ananta)
// Disabled until I figure out why this does not work on Firefox.
TEST_F(ChromeFrameTestWithWebServer,
       DISABLED_WidgetModeFF_MultipleCFInstances) {
  SimpleBrowserTest(FIREFOX, kMultipleCFInstancesTestUrl,
                    L"WidgetMode_MultipleInstancesTest");
}

const wchar_t kChromeFrameFullTabModeXMLHttpRequestAuthHeaderTestUrl[] =
    L"files/xmlhttprequest_authorization_header_test.html";

TEST_F(ChromeFrameTestWithWebServer,
       FullTabModeIE_ChromeFrameXHRAuthHeaderTest) {
  chrome_frame_test::TimedMsgLoop loop;

  ASSERT_TRUE(LaunchBrowser(
                  IE, kChromeFrameFullTabModeXMLHttpRequestAuthHeaderTestUrl));

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  chrome_frame_test::CloseAllIEWindows();
  ASSERT_TRUE(
      CheckResultFile(L"FullTab_XMLHttpRequestAuthorizationHeaderTest", "OK"));
}

const wchar_t kChromeFrameFullTabModeDeleteCookieTest[] =
    L"files/fulltab_delete_cookie_test.html";

TEST_F(ChromeFrameTestWithWebServer,
       FullTabModeIE_ChromeFrameDeleteCookieTest) {
  chrome_frame_test::TimedMsgLoop loop;

  ASSERT_TRUE(LaunchBrowser(IE, kChromeFrameFullTabModeDeleteCookieTest));

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  chrome_frame_test::CloseAllIEWindows();
  ASSERT_TRUE(CheckResultFile(L"FullTab_DeleteCookieTest", "OK"));
}

