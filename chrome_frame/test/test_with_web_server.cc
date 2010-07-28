// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/test_with_web_server.h"

#include "base/file_version_info.h"
#include "base/utf_string_conversions.h"
#include "base/win_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/helper.h"
#include "chrome_frame/utils.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/mock_ie_event_sink_actions.h"
#include "net/base/mime_util.h"

using chrome_frame_test::kChromeFrameLongNavigationTimeoutInSeconds;
using testing::_;
using testing::StrCaseEq;

const wchar_t kDocRoot[] = L"chrome_frame\\test\\data";
const int kLongWaitTimeout = 60 * 1000;
const int kShortWaitTimeout = 25 * 1000;

namespace {

// Helper method for creating the appropriate HTTP response headers.
std::string CreateHttpHeaders(CFInvocation invocation,
                              bool add_no_cache_header,
                              const std::string& content_type) {
  std::ostringstream ss;
  ss << "HTTP/1.1 200 OK\r\n"
     << "Connection: close\r\n"
     << "Content-Type: " << content_type << "\r\n";
  if (invocation.type() == CFInvocation::HTTP_HEADER)
    ss << "X-UA-Compatible: chrome=1\r\n";
  if (add_no_cache_header)
    ss << "Cache-Control: no-cache\r\n";
  return ss.str();
}

}  // namespace

class ChromeFrameTestEnvironment: public testing::Environment {
 public:
  ~ChromeFrameTestEnvironment() {}
  void SetUp() {
    ScopedChromeFrameRegistrar::RegisterDefaults();
  }
  void TearDown() {}
};

::testing::Environment* const chrome_frame_env =
    ::testing::AddGlobalTestEnvironment(new ChromeFrameTestEnvironment);

void ChromeFrameTestWithWebServer::CloseAllBrowsers() {
  // Web browsers tend to relaunch themselves in other processes, meaning the
  // KillProcess stuff above might not have actually cleaned up all our browser
  // instances, so make really sure browsers are dead.
  base::KillProcesses(chrome_frame_test::kIEImageName, 0, NULL);
  base::KillProcesses(chrome_frame_test::kIEBrokerImageName, 0, NULL);
  base::KillProcesses(chrome_frame_test::kFirefoxImageName, 0, NULL);
  base::KillProcesses(chrome_frame_test::kSafariImageName, 0, NULL);

  // Endeavour to only kill off Chrome Frame derived Chrome processes.
  KillAllNamedProcessesWithArgument(
      UTF8ToWide(chrome_frame_test::kChromeImageName),
      UTF8ToWide(switches::kChromeFrame));
  base::KillProcesses(chrome_frame_test::kChromeLauncher, 0, NULL);
}

void ChromeFrameTestWithWebServer::SetUp() {
  // Make sure our playground is clean before we start.
  CloseAllBrowsers();

  // Make sure that we are not accidently enabling gcf protocol.
  SetConfigBool(kEnableGCFProtocol, false);

  PathService::Get(base::DIR_SOURCE_ROOT, &test_file_path_);
  test_file_path_ = test_file_path_.Append(FILE_PATH_LITERAL("chrome_frame"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"));

  server_.SetUp();
  EXPECT_TRUE(server_.server() != NULL);
  if (server_.server()) {
    results_dir_ = server_.GetDataDir();
    results_dir_ = results_dir_.AppendASCII("dump");
  }
}

void ChromeFrameTestWithWebServer::TearDown() {
  CloseBrowser();
  CloseAllBrowsers();
  server_.TearDown();
}

bool ChromeFrameTestWithWebServer::LaunchBrowser(BrowserKind browser,
                                                 const wchar_t* page) {
  EXPECT_TRUE(server_.server() != NULL);
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
  return simulate_input::EnsureProcessInForeground(
      GetProcessId(browser_handle_));
}

bool ChromeFrameTestWithWebServer::WaitForTestToComplete(int milliseconds) {
  EXPECT_TRUE(server_.server() != NULL);
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
  FilePath full_path(results_dir_);
  full_path = full_path.Append(file_name);
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
  DCHECK(browser != CHROME) << "Chrome tests shouldn't be optional";
  if (!LaunchBrowser(browser, page)) {
    LOG(ERROR) << "Failed to launch browser " << ToString(browser);
  } else {
    ASSERT_TRUE(WaitForTestToComplete(kLongWaitTimeout));
    ASSERT_TRUE(CheckResultFile(result_file_to_check, "OK"));
  }
}

void ChromeFrameTestWithWebServer::VersionTest(BrowserKind browser,
    const wchar_t* page, const wchar_t* result_file_to_check) {
  FilePath plugin_path;
  PathService::Get(base::DIR_MODULE, &plugin_path);
  plugin_path = plugin_path.AppendASCII("servers");
  plugin_path = plugin_path.Append(kChromeFrameDllName);

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
    FilePath cf_dll_path = FilePath::FromWStringHack(
        installer::GetChromeInstallPath(system_install));
    cf_dll_path = cf_dll_path.Append(
        ver_system ? ver_system->GetString() : ver_user->GetString());
    cf_dll_path = cf_dll_path.Append(kChromeFrameDllName);
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

// MockWebServer methods
void MockWebServer::ExpectAndServeRequest(CFInvocation invocation,
                                          const std::wstring& url) {
  EXPECT_CALL(*this, Get(_, chrome_frame_test::UrlPathEq(url), _))
      .WillOnce(SendResponse(this, invocation));
}

void MockWebServer::ExpectAndServeRequestAllowCache(CFInvocation invocation,
                                                    const std::wstring &url) {
  EXPECT_CALL(*this, Get(_, chrome_frame_test::UrlPathEq(url), _))
      .WillOnce(SendResponse(this, invocation));
}

void MockWebServer::ExpectAndServeRequestAnyNumberTimes(
    CFInvocation invocation, const std::wstring& path_prefix) {
  EXPECT_CALL(*this, Get(_, testing::StartsWith(path_prefix), _))
      .WillRepeatedly(SendResponse(this, invocation));
}

void MockWebServer::SendResponseHelper(
    test_server::ConfigurableConnection* connection,
    const std::wstring& request_uri,
    CFInvocation invocation,
    bool add_no_cache_header) {
  // Convert |request_uri| to a path.
  std::wstring path = request_uri;
  size_t query_index = request_uri.find(L"?");
  if (query_index != std::string::npos) {
    path = path.erase(query_index);
  }
  FilePath file_path = root_dir_;
  if (path.size())
    file_path = file_path.Append(path.substr(1));  // remove first '/'

  std::string headers, body;
  if (file_util::PathExists(file_path)) {
    std::string content_type;
    EXPECT_TRUE(net::GetMimeTypeFromFile(file_path, &content_type));
    DLOG(INFO) << "Going to send file (" << WideToUTF8(file_path.value())
               << ") with content type (" << content_type << ")";
    headers = CreateHttpHeaders(invocation, add_no_cache_header, content_type);

    EXPECT_TRUE(file_util::ReadFileToString(file_path, &body))
        << "Could not read file (" << WideToUTF8(file_path.value()) << ")";
    if (invocation.type() == CFInvocation::META_TAG &&
        StartsWithASCII(content_type, "text/html", false)) {
      EXPECT_TRUE(chrome_frame_test::AddCFMetaTag(&body)) << "Could not add "
          << "meta tag to HTML file.";
    }
  } else {
    DLOG(INFO) << "Going to send 404 for non-existent file ("
               << WideToUTF8(file_path.value()) << ")";
    headers = "HTTP/1.1 404 Not Found";
    body = "";
  }
  connection->Send(headers, body);
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

  // If this test fails for IE8 then it is possible that prebinding is enabled.
  // A known workaround is to disable it until CF properly handles it.
  //
  // HKCU\Software\Microsoft\Internet Explorer\Main
  // Value name: EnablePreBinding (REG_DWORD)
  // Value: 0
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

// http://crbug.com/37087
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeFF_ObjectFocus) {
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
      simulate_input::SendMnemonic(VK_TAB, simulate_input::NONE, false, false);
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

// http://crbug.com/37085
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeFF_CFInstanceBasic) {
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

// http://crbug.com/37088
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeIE_CFInstanceFallback) {
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
  SimpleBrowserTest(CHROME, kCFIIfrPostPage, L"CFInstanceIfrPost");
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
  SimpleBrowserTest(CHROME, kCFIPostPage, L"CFInstancePost");
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
  SimpleBrowserTest(CHROME, kCFIRPCPage, L"CFInstanceRPC");
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

// http://code.google.com/p/chromium/issues/detail?id=37204
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeFF_CFInstanceRPCInternal) {
  SimpleBrowserTest(FIREFOX, kCFIRPCInternalPage, L"CFInstanceRPCInternal");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeChrome_CFInstanceRPCInternal) {
  SimpleBrowserTest(CHROME, kCFIRPCInternalPage, L"CFInstanceRPCInternal");
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

// Flaky on build bots. See http://crbug.com/37009
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeFF_InHead) {
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

const wchar_t kMetaTagPage[] = L"files/meta_tag.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_MetaTag) {
  SimpleBrowserTest(IE, kMetaTagPage, L"meta_tag");
}

const wchar_t kCFProtocolPage[] = L"files/cf_protocol.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_CFProtocol) {
  // Temporarily enable  gcf: protocol for this test.
  SetConfigBool(kEnableGCFProtocol, true);
  SimpleBrowserTest(IE, kCFProtocolPage, L"chrome_frame_protocol");
  SetConfigBool(kEnableGCFProtocol, false);
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

const wchar_t kXMLHttpRequestTestUrl[] =
    L"files/xmlhttprequest_test.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_XHRTest) {
  chrome_frame_test::TimedMsgLoop loop;
  ASSERT_TRUE(LaunchBrowser(IE, kXMLHttpRequestTestUrl));

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  chrome_frame_test::CloseAllIEWindows();
  ASSERT_TRUE(CheckResultFile(L"FullTab_XMLHttpRequestTest", "OK"));
}

const wchar_t kInstallFlowTestUrl[] =
    L"files/install_flow_test.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_InstallFlowTest) {
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
    chrome_frame_test::TimedMsgLoop loop;
    ScopedChromeFrameRegistrar::UnregisterAtPath(
        ScopedChromeFrameRegistrar::GetChromeFrameBuildPath().value());

    ASSERT_TRUE(LaunchBrowser(IE, kInstallFlowTestUrl));

    loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

    ScopedChromeFrameRegistrar::RegisterAtPath(
        ScopedChromeFrameRegistrar::GetChromeFrameBuildPath().value());

    loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

    chrome_frame_test::CloseAllIEWindows();
    ASSERT_TRUE(CheckResultFile(L"FullTab_InstallFlowTest", "OK"));
  }
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

const wchar_t kXHRAuthHeaderTestUrl[] =
    L"files/xmlhttprequest_authorization_header_test.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_XHRAuthHeaderTest) {
  chrome_frame_test::TimedMsgLoop loop;
  ASSERT_TRUE(LaunchBrowser(IE, kXHRAuthHeaderTestUrl));

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  chrome_frame_test::CloseAllIEWindows();
  ASSERT_TRUE(CheckResultFile(L"FullTabModeIE_XHRAuthHeaderTest", "OK"));
}

const wchar_t kDeleteCookieTest[] =
    L"files/fulltab_delete_cookie_test.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_DeleteCookieTest) {
  chrome_frame_test::TimedMsgLoop loop;
  ASSERT_TRUE(LaunchBrowser(IE, kDeleteCookieTest));

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  chrome_frame_test::CloseAllIEWindows();
  ASSERT_TRUE(CheckResultFile(L"FullTab_DeleteCookieTest", "OK"));
}

const wchar_t kAnchorUrlNavigate[] =
    L"files/fulltab_anchor_url_navigate.html#chrome_frame";

// http://code.google.com/p/chromium/issues/detail?id=35341
TEST_F(ChromeFrameTestWithWebServer,
       FLAKY_FullTabModeIE_AnchorUrlNavigateTest) {
  if (IsIBrowserServicePatchEnabled()) {
    LOG(ERROR) << "Not running test. IBrowserServicePatch is in place.";
    return;
  }

  chrome_frame_test::TimedMsgLoop loop;
  ASSERT_TRUE(LaunchBrowser(IE, kAnchorUrlNavigate));

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  chrome_frame_test::CloseAllIEWindows();
  ASSERT_TRUE(CheckResultFile(L"FullTab_AnchorURLNavigateTest", "OK"));
}

// Test whether POST-ing a form from an mshtml page to a CF page will cause
// the request to get reissued.  It should not.
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_TestPostReissue) {
  if (IsIBrowserServicePatchEnabled()) {
    LOG(ERROR) << "Not running test. IBrowserServicePatch is in place.";
    return;
  }

  chrome_frame_test::TimedMsgLoop loop;  // must come before the server.

  // The order of pages in this array is assumed to be mshtml, cf, script.
  const wchar_t* kPages[] = {
    L"full_tab_post_mshtml.html",
    L"full_tab_post_target_cf.html",
    L"chrome_frame_tester_helpers.js",
  };

  SimpleWebServerTest server(46664);
  server.PopulateStaticFileList(kPages, arraysize(kPages), GetCFTestFilePath());

  ASSERT_TRUE(LaunchBrowser(IE, server.FormatHttpPath(kPages[0]).c_str()));

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  const test_server::Request* request = NULL;
  server.FindRequest("/quit?OK", &request);
  ASSERT_TRUE(request != NULL);
  EXPECT_EQ("OK", request->arguments());

  if (request->arguments().compare("OK") == 0) {
    // Check how many requests we got for the cf page.  Also expect it to be
    // a POST.
    int requests = server.GetRequestCountForPage(kPages[1], "POST");
    EXPECT_EQ(1, requests);
  }
}

// Test whether following a link from an mshtml page to a CF page will cause
// multiple network requests.  It should not.
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_TestMultipleGet) {
  if (IsIBrowserServicePatchEnabled()) {
    LOG(ERROR) << "Not running test. IBrowserServicePatch is in place.";
    return;
  }

  chrome_frame_test::TimedMsgLoop loop;  // must come before the server.

  // The order of pages in this array is assumed to be mshtml, cf, script.
  const wchar_t* kPages[] = {
    L"full_tab_get_mshtml.html",
    L"full_tab_get_target_cf.html",
    L"chrome_frame_tester_helpers.js",
  };

  SimpleWebServerTest server(46664);

  server.PopulateStaticFileList(kPages, arraysize(kPages), GetCFTestFilePath());

  ASSERT_TRUE(LaunchBrowser(IE, server.FormatHttpPath(kPages[0]).c_str()));

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  const test_server::Request* request = NULL;
  server.FindRequest("/quit?OK", &request);
  ASSERT_TRUE(request != NULL);
  EXPECT_EQ("OK", request->arguments());

  if (request->arguments().compare("OK") == 0) {
    // Check how many requests we got for the cf page and check that it was
    // a GET.
    int requests = server.GetRequestCountForPage(kPages[1], "GET");
    EXPECT_EQ(1, requests);
  }
}

const wchar_t kSetCookieTest[] =
    L"files/fulltab_set_cookie_test.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_SetCookieTest) {
  chrome_frame_test::TimedMsgLoop loop;
  ASSERT_TRUE(LaunchBrowser(IE, kSetCookieTest));

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  chrome_frame_test::CloseAllIEWindows();
  ASSERT_TRUE(CheckResultFile(L"FullTab_SetCookieTest", "OK"));
}

const wchar_t kXHRConditionalHeaderTestUrl[] =
    L"files/xmlhttprequest_conditional_header_test.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_XHRConditionalHeaderTest) {
  chrome_frame_test::TimedMsgLoop loop;
  ASSERT_TRUE(LaunchBrowser(IE, kXHRConditionalHeaderTestUrl));

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  chrome_frame_test::CloseAllIEWindows();
  ASSERT_TRUE(CheckResultFile(L"FullTab_XMLHttpRequestConditionalHeaderTest",
                              "OK"));
}

