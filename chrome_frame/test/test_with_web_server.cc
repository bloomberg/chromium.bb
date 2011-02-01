// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/test_with_web_server.h"

#include "base/base_paths.h"
#include "base/file_version_info.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/helper.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/utils.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/mock_ie_event_sink_actions.h"
#include "chrome_frame/test/mock_ie_event_sink_test.h"
#include "net/base/mime_util.h"
#include "net/http/http_util.h"

using chrome_frame_test::kChromeFrameLongNavigationTimeoutInSeconds;
using testing::_;
using testing::StrCaseEq;

const wchar_t kDocRoot[] = L"chrome_frame\\test\\data";

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

std::string GetMockHttpHeaders(const FilePath& mock_http_headers_path) {
  std::string headers;
  file_util::ReadFileToString(mock_http_headers_path, &headers);
  return headers;
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

ChromeFrameTestWithWebServer::ChromeFrameTestWithWebServer()
    : loop_(),
      server_mock_(1337, L"127.0.0.1",
          chrome_frame_test::GetTestDataFolder()) {
}

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

  // Make sure that we are not accidentally enabling gcf protocol.
  SetConfigBool(kAllowUnsafeURLs, false);

  FilePath chrome_frame_source_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &chrome_frame_source_path);
  chrome_frame_source_path = chrome_frame_source_path.Append(
      FILE_PATH_LITERAL("chrome_frame"));

  test_file_path_ = chrome_frame_source_path;
  test_file_path_ = test_file_path_.Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"));

  results_dir_ = chrome_frame_test::GetTestDataFolder();
  results_dir_.AppendASCII("dump");

  // Copy the CFInstance.js and CFInstall.js files from src\chrome_frame to
  // src\chrome_frame\test\data.
  FilePath CFInstance_src_path;
  FilePath CFInstall_src_path;

  CFInstance_src_path = chrome_frame_source_path.AppendASCII("CFInstance.js");
  CFInstance_path_ = test_file_path_.AppendASCII("CFInstance.js");

  file_util::CopyFileW(CFInstance_src_path, CFInstance_path_);

  CFInstall_src_path = chrome_frame_source_path.AppendASCII("CFInstall.js");
  CFInstall_path_ = test_file_path_.AppendASCII("CFInstall.js");

  file_util::CopyFileW(CFInstall_src_path, CFInstall_path_);

  server_mock_.ExpectAndServeAnyRequests(CFInvocation(CFInvocation::NONE));
  server_mock_.set_expected_result("OK");
}

void ChromeFrameTestWithWebServer::TearDown() {
  // Make sure that the Firefox privilege mode is never forced either on or off
  // after the test completes.
  DeleteConfigValue(kEnableFirefoxPrivilegeMode);

  CloseBrowser();
  CloseAllBrowsers();
  file_util::Delete(CFInstall_path_, false);
  file_util::Delete(CFInstance_path_, false);
}

bool ChromeFrameTestWithWebServer::LaunchBrowser(BrowserKind browser,
                                                 const wchar_t* page) {
  std::wstring url = page;

  // We should resolve the URL only if it is a relative url.
  GURL parsed_url(WideToUTF8(page));
  if (!parsed_url.has_scheme()) {
    url = server_mock_.Resolve(page);
  }

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
  loop_.RunFor(milliseconds/1000);
  return true;
}

bool ChromeFrameTestWithWebServer::WaitForOnLoad(int milliseconds) {
  return false;
}

const wchar_t kPostedResultSubstring[] = L"/writefile/";

void ChromeFrameTestWithWebServer::SimpleBrowserTestExpectedResult(
    BrowserKind browser, const wchar_t* page, const char* result) {
  ASSERT_TRUE(LaunchBrowser(browser, page));
  server_mock_.ExpectAndHandlePostedResult(CFInvocation(CFInvocation::NONE),
                                           kPostedResultSubstring);
  WaitForTestToComplete(TestTimeouts::action_max_timeout_ms());
  ASSERT_EQ(result, server_mock_.posted_result());
}

void ChromeFrameTestWithWebServer::SimpleBrowserTest(BrowserKind browser,
    const wchar_t* page) {
  if (browser == FIREFOX &&
      base::win::GetVersion() == base::win::VERSION_WIN7) {
    LOG(INFO) << "Not running Firefox tests on Windows 7";
    return;
  }
  SimpleBrowserTestExpectedResult(browser, page, "OK");
}

void ChromeFrameTestWithWebServer::OptionalBrowserTest(BrowserKind browser,
    const wchar_t* page) {
  DCHECK(browser != CHROME) << "Chrome tests shouldn't be optional";
  if (!LaunchBrowser(browser, page)) {
    LOG(ERROR) << "Failed to launch browser " << ToString(browser);
  } else {
    server_mock_.ExpectAndHandlePostedResult(CFInvocation(CFInvocation::NONE),
                                             kPostedResultSubstring);
    WaitForTestToComplete(TestTimeouts::action_max_timeout_ms());
    ASSERT_EQ("OK", server_mock_.posted_result());
  }
}

void ChromeFrameTestWithWebServer::VersionTest(BrowserKind browser,
    const wchar_t* page) {
  if (browser == FIREFOX &&
      base::win::GetVersion() == base::win::VERSION_WIN7) {
    LOG(INFO) << "Not running Firefox tests on Windows 7";
    return;
  }

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
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    scoped_ptr<Version> ver_system(InstallUtil::GetChromeVersion(dist, true));
    scoped_ptr<Version> ver_user(InstallUtil::GetChromeVersion(dist, false));
    ASSERT_TRUE(ver_system.get() || ver_user.get());

    bool system_install = ver_system.get() ? true : false;
    FilePath cf_dll_path(installer::GetChromeInstallPath(system_install, dist));
    cf_dll_path = cf_dll_path.Append(UTF8ToWide(
        ver_system.get() ? ver_system->GetString() : ver_user->GetString()));
    cf_dll_path = cf_dll_path.Append(kChromeFrameDllName);
    version_info = FileVersionInfo::CreateFileVersionInfo(cf_dll_path);
    if (version_info)
      version = version_info->product_version();
  }

  server_mock_.set_expected_result(WideToUTF8(version));

  EXPECT_TRUE(version_info);
  EXPECT_FALSE(version.empty());
  EXPECT_TRUE(LaunchBrowser(browser, page));
  server_mock_.ExpectAndHandlePostedResult(CFInvocation(CFInvocation::NONE),
                                           kPostedResultSubstring);
  WaitForTestToComplete(TestTimeouts::action_max_timeout_ms());
  ASSERT_EQ(version, UTF8ToWide(server_mock_.posted_result()));
}

void ChromeFrameTestWithWebServer::SessionIdTest(BrowserKind browser,
                                                 const wchar_t* page,
                                                 int privilege_mode,
                                                 const char* expected_result) {
  if (browser == FIREFOX &&
      base::win::GetVersion() == base::win::VERSION_WIN7) {
    LOG(INFO) << "Not running Firefox tests on Windows 7";
    return;
  }

  SetConfigInt(kEnableFirefoxPrivilegeMode, privilege_mode);
  EXPECT_TRUE(LaunchBrowser(browser, page));
  server_mock_.set_expected_result(expected_result);
  server_mock_.ExpectAndHandlePostedResult(CFInvocation(CFInvocation::NONE),
                                           kPostedResultSubstring);
  WaitForTestToComplete(TestTimeouts::action_max_timeout_ms());
  ASSERT_EQ(expected_result, server_mock_.posted_result());
}

// MockWebServer methods
void MockWebServer::ExpectAndServeRequest(CFInvocation invocation,
                                          const std::wstring& url) {
  ExpectAndServeRequestWithCardinality(invocation, url, testing::Exactly(1));
}

void MockWebServer::ExpectAndServeRequestWithCardinality(
    CFInvocation invocation, const std::wstring& url,
    testing::Cardinality cardinality) {
  EXPECT_CALL(*this, Get(_, chrome_frame_test::UrlPathEq(url), _))
      .Times(cardinality)
      .WillRepeatedly(SendResponse(this, invocation));
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

void MockWebServer::ExpectAndHandlePostedResult(
    CFInvocation invocation, const std::wstring& post_suffix) {
  EXPECT_CALL(*this, Post(_, testing::HasSubstr(post_suffix), _))
      .WillRepeatedly(HandlePostedResponseHelper(this, invocation));
}

void MockWebServer::HandlePostedResponse(
    test_server::ConfigurableConnection* connection,
    const test_server::Request& request) {
  posted_result_ = request.content();
  if (posted_result_ == expected_result_) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            new MessageLoop::QuitTask,
                                            100);
  }
  connection->Send("HTTP/1.1 200 OK\r\n", "");
}

void MockWebServer::SendResponseHelper(
    test_server::ConfigurableConnection* connection,
    const std::wstring& request_uri,
    const test_server::Request& request,
    CFInvocation invocation,
    bool add_no_cache_header) {
  static const wchar_t kEchoHeader[] = L"/echoheader?";
  if (request_uri.find(kEchoHeader) != std::wstring::npos) {
    std::wstring header = request_uri.substr(
        wcslen(kEchoHeader),
        request_uri.length() - wcslen(kEchoHeader));

    std::string header_value = http_utils::GetHttpHeaderFromHeaderList(
        WideToUTF8(header), request.headers());
    connection->Send(header_value, "");
    return;
  }
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
  std::string content_type;
  if (file_util::PathExists(file_path) &&
      !file_util::DirectoryExists(file_path)) {
    FilePath mock_http_headers(file_path.value() + L".mock-http-headers");
    if (file_util::PathExists(mock_http_headers)) {
      headers = GetMockHttpHeaders(mock_http_headers);
      content_type = http_utils::GetHttpHeaderFromHeaderList("Content-type",
                                                             headers);
    } else {
      EXPECT_TRUE(net::GetMimeTypeFromFile(file_path, &content_type));
      DVLOG(1) << "Going to send file (" << WideToUTF8(file_path.value())
               << ") with content type (" << content_type << ")";
      headers = CreateHttpHeaders(invocation, add_no_cache_header,
                                  content_type);
    }

    EXPECT_FALSE(headers.empty());

    EXPECT_TRUE(file_util::ReadFileToString(file_path, &body))
        << "Could not read file (" << WideToUTF8(file_path.value()) << ")";
    if (invocation.type() == CFInvocation::META_TAG &&
        StartsWithASCII(content_type, "text/html", false)) {
      EXPECT_TRUE(chrome_frame_test::AddCFMetaTag(&body)) << "Could not add "
          << "meta tag to HTML file.";
    }
  } else {
    DVLOG(1) << "Going to send 404 for non-existent file ("
             << WideToUTF8(file_path.value()) << ")";
    headers = "HTTP/1.1 404 Not Found";
    body = "";
  }
  connection->Send(headers, body);
}

const wchar_t kPostMessageBasicPage[] = L"postmessage_basic_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_PostMessageBasic) {
  SimpleBrowserTest(IE, kPostMessageBasicPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_PostMessageBasic) {
  SimpleBrowserTest(FIREFOX, kPostMessageBasicPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_PostMessageBasic) {
  OptionalBrowserTest(OPERA, kPostMessageBasicPage);
}

TEST_F(ChromeFrameTestWithWebServer, FullTabIE_MIMEFilterBasic) {
  const wchar_t kMIMEFilterBasicPage[] =
      L"chrome_frame_mime_filter_test.html";

  // If this test fails for IE8 then it is possible that prebinding is enabled.
  // A known workaround is to disable it until CF properly handles it.
  //
  // HKCU\Software\Microsoft\Internet Explorer\Main
  // Value name: EnablePreBinding (REG_DWORD)
  // Value: 0
  SimpleBrowserTest(IE, kMIMEFilterBasicPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_Resize) {
  SimpleBrowserTest(IE, L"chrome_frame_resize.html");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_Resize) {
  SimpleBrowserTest(FIREFOX, L"chrome_frame_resize.html");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_Resize) {
  OptionalBrowserTest(OPERA, L"chrome_frame_resize.html");
}

const wchar_t kNavigateURLAbsolutePage[] =
    L"navigateurl_absolute_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_NavigateURLAbsolute) {
  SimpleBrowserTest(IE, kNavigateURLAbsolutePage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_NavigateURLAbsolute) {
  SimpleBrowserTest(FIREFOX, kNavigateURLAbsolutePage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_NavigateURLAbsolute) {
  OptionalBrowserTest(OPERA, kNavigateURLAbsolutePage);
}

const wchar_t kNavigateURLRelativePage[] =
    L"navigateurl_relative_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_NavigateURLRelative) {
  SimpleBrowserTest(IE, kNavigateURLRelativePage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_NavigateURLRelative) {
  SimpleBrowserTest(FIREFOX, kNavigateURLRelativePage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_NavigateURLRelative) {
  OptionalBrowserTest(OPERA, kNavigateURLRelativePage);
}

const wchar_t kNavigateSimpleObjectFocus[] = L"simple_object_focus.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_ObjectFocus) {
  SimpleBrowserTest(FIREFOX, kNavigateSimpleObjectFocus);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_ObjectFocus) {
  SimpleBrowserTest(IE, kNavigateSimpleObjectFocus);
}

// TODO(ananta)
// Rewrite this test for opera.
TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeOpera_ObjectFocus) {
  if (!LaunchBrowser(OPERA, kNavigateSimpleObjectFocus)) {
    LOG(ERROR) << "Failed to launch browser " << ToString(OPERA);
  } else {
    ASSERT_TRUE(WaitForOnLoad(TestTimeouts::action_max_timeout_ms()));
    server_mock_.ExpectAndHandlePostedResult(CFInvocation(CFInvocation::NONE),
                                             kPostedResultSubstring);
    BringBrowserToTop();
    // Tab through a couple of times.  Once should be enough in theory
    // but in practice activating the browser can take a few milliseconds more.
    bool ok;
    for (int i = 0;
         i < 5 && (ok = (server_mock_.posted_result() == "OK")) == false;
         ++i) {
      Sleep(300);
      simulate_input::SendMnemonic(VK_TAB, simulate_input::NONE, false, false);
    }
    ASSERT_TRUE(ok);
  }
}

const wchar_t kiframeBasicPage[] = L"iframe_basic_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_iframeBasic) {
  SimpleBrowserTest(IE, kiframeBasicPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_iframeBasic) {
  SimpleBrowserTest(FIREFOX, kiframeBasicPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_iframeBasic) {
  OptionalBrowserTest(OPERA, kiframeBasicPage);
}

const wchar_t kSrcPropertyTestPage[] = L"src_property_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_SrcProperty) {
  SimpleBrowserTest(IE, kSrcPropertyTestPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_SrcProperty) {
  SimpleBrowserTest(FIREFOX, kSrcPropertyTestPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_SrcProperty) {
  OptionalBrowserTest(OPERA, kSrcPropertyTestPage);
}

const wchar_t kCFInstanceBasicTestPage[] = L"CFInstance_basic_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceBasic) {
  SimpleBrowserTest(IE, kCFInstanceBasicTestPage);
}

// http://crbug.com/37085
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeFF_CFInstanceBasic) {
  SimpleBrowserTest(FIREFOX, kCFInstanceBasicTestPage);
}

const wchar_t kCFISingletonPage[] = L"CFInstance_singleton_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceSingleton) {
  SimpleBrowserTest(IE, kCFISingletonPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceSingleton) {
  SimpleBrowserTest(FIREFOX, kCFISingletonPage);
}

const wchar_t kCFIDelayPage[] = L"CFInstance_delay_host.html";

TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeIE_CFInstanceDelay) {
  SimpleBrowserTest(IE, kCFIDelayPage);
}

TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeFF_CFInstanceDelay) {
  SimpleBrowserTest(FIREFOX, kCFIDelayPage);
}

TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeOpera_CFInstanceDelay) {
  OptionalBrowserTest(OPERA, kCFIDelayPage);
}

const wchar_t kCFIFallbackPage[] = L"CFInstance_fallback_host.html";

// http://crbug.com/37088
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeIE_CFInstanceFallback) {
  SimpleBrowserTest(IE, kCFIFallbackPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceFallback) {
  SimpleBrowserTest(FIREFOX, kCFIFallbackPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_CFInstanceFallback) {
  OptionalBrowserTest(OPERA, kCFIFallbackPage);
}

const wchar_t kCFINoSrcPage[] = L"CFInstance_no_src_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceNoSrc) {
  SimpleBrowserTest(IE, kCFINoSrcPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceNoSrc) {
  SimpleBrowserTest(FIREFOX, kCFINoSrcPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_CFInstanceNoSrc) {
  OptionalBrowserTest(OPERA, kCFINoSrcPage);
}

const wchar_t kCFIIfrOnLoadPage[] = L"CFInstance_iframe_onload_host.html";

// disabled since it's unlikely that we care about this case
TEST_F(ChromeFrameTestWithWebServer,
       DISABLED_WidgetModeIE_CFInstanceIfrOnLoad) {
  SimpleBrowserTest(IE, kCFIIfrOnLoadPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceIfrOnLoad) {
  SimpleBrowserTest(FIREFOX, kCFIIfrOnLoadPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_CFInstanceIfrOnLoad) {
  OptionalBrowserTest(OPERA, kCFIIfrOnLoadPage);
}

const wchar_t kCFIZeroSizePage[] = L"CFInstance_zero_size_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceZeroSize) {
  SimpleBrowserTest(IE, kCFIZeroSizePage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceZeroSize) {
  SimpleBrowserTest(FIREFOX, kCFIZeroSizePage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_CFInstanceZeroSize) {
  OptionalBrowserTest(OPERA, kCFIZeroSizePage);
}

const wchar_t kCFIIfrPostPage[] = L"CFInstance_iframe_post_host.html";

// http://crbug.com/32321
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeIE_CFInstanceIfrPost) {
  SimpleBrowserTest(IE, kCFIIfrPostPage);
}

// Flakes out on the bots, http://crbug.com/26372
TEST_F(ChromeFrameTestWithWebServer,
       FLAKY_WidgetModeFF_CFInstanceIfrPost) {
  SimpleBrowserTest(FIREFOX, kCFIIfrPostPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeChrome_CFInstanceIfrPost) {
  SimpleBrowserTest(CHROME, kCFIIfrPostPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeSafari_CFInstanceIfrPost) {
  OptionalBrowserTest(SAFARI, kCFIIfrPostPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_CFInstanceIfrPost) {
  OptionalBrowserTest(OPERA, kCFIIfrPostPage);
}

const wchar_t kCFIPostPage[] = L"CFInstance_post_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstancePost) {
  if (chrome_frame_test::GetInstalledIEVersion() == IE_9) {
    LOG(INFO) << "Not running test on Vista/Windows 7 with IE9";
    return;
  }
  SimpleBrowserTest(IE, kCFIPostPage);
}

// This test randomly fails on the ChromeFrame builder.
// Bug http://code.google.com/p/chromium/issues/detail?id=31532
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeFF_CFInstancePost) {
  SimpleBrowserTest(FIREFOX, kCFIPostPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeChrome_CFInstancePost) {
  SimpleBrowserTest(CHROME, kCFIPostPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeSafari_CFInstancePost) {
  OptionalBrowserTest(SAFARI, kCFIPostPage);
}

TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeOpera_CFInstancePost) {
  OptionalBrowserTest(OPERA, kCFIPostPage);
}

const wchar_t kCFIRPCPage[] = L"CFInstance_rpc_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceRPC) {
  if (chrome_frame_test::GetInstalledIEVersion() == IE_9) {
    LOG(INFO) << "Not running test on Vista/Windows 7 with IE9";
    return;
  }
  SimpleBrowserTest(IE, kCFIRPCPage);
}

// This test randomly fails on the ChromeFrame builder.
// Bug http://code.google.com/p/chromium/issues/detail?id=31532
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeFF_CFInstanceRPC) {
  SimpleBrowserTest(FIREFOX, kCFIRPCPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeChrome_CFInstanceRPC) {
  SimpleBrowserTest(CHROME, kCFIRPCPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeSafari_CFInstanceRPC) {
  OptionalBrowserTest(SAFARI, kCFIRPCPage);
}

TEST_F(ChromeFrameTestWithWebServer, DISABLED_WidgetModeOpera_CFInstanceRPC) {
  OptionalBrowserTest(OPERA, kCFIRPCPage);
}

const wchar_t kCFIRPCInternalPage[] =
    L"CFInstance_rpc_internal_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceRPCInternal) {
  if (chrome_frame_test::GetInstalledIEVersion() == IE_9) {
    LOG(INFO) << "Not running test on Vista/Windows 7 with IE9";
    return;
  }
  SimpleBrowserTest(IE, kCFIRPCInternalPage);
}

// http://code.google.com/p/chromium/issues/detail?id=37204
TEST_F(ChromeFrameTestWithWebServer, FLAKY_WidgetModeFF_CFInstanceRPCInternal) {
  SimpleBrowserTest(FIREFOX, kCFIRPCInternalPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeChrome_CFInstanceRPCInternal) {
  SimpleBrowserTest(CHROME, kCFIRPCInternalPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeSafari_CFInstanceRPCInternal) {
  OptionalBrowserTest(SAFARI, kCFIRPCInternalPage);
}

const wchar_t kCFIDefaultCtorPage[] =
    L"CFInstance_default_ctor_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceDefaultCtor) {
  SimpleBrowserTest(IE, kCFIDefaultCtorPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_CFInstanceDefaultCtor) {
  SimpleBrowserTest(FIREFOX, kCFIDefaultCtorPage);
}

const wchar_t kCFInstallBasicTestPage[] = L"CFInstall_basic.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabIE_CFInstallBasic) {
  SimpleBrowserTest(IE, kCFInstallBasicTestPage);
}

const wchar_t kCFInstallPlaceTestPage[] = L"CFInstall_place.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabIE_CFInstallPlace) {
  SimpleBrowserTest(IE, kCFInstallPlaceTestPage);
}

const wchar_t kCFInstallOverlayTestPage[] = L"CFInstall_overlay.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabIE_CFInstallOverlay) {
  SimpleBrowserTest(IE, kCFInstallOverlayTestPage);
}

const wchar_t kCFInstallDismissTestPage[] = L"CFInstall_dismiss.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabIE_CFInstallDismiss) {
  SimpleBrowserTest(IE, kCFInstallDismissTestPage);
}

const wchar_t kInitializeHiddenPage[] = L"initialize_hidden.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_InitializeHidden) {
  SimpleBrowserTest(IE, kInitializeHiddenPage);
}

const wchar_t kFullTabHttpHeaderPage[] = L"chrome_frame_http_header.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_CFHttpHeaderBasic) {
  SimpleBrowserTest(IE, kFullTabHttpHeaderPage);
}

const wchar_t kFullTabHttpHeaderPageIFrame[] =
    L"chrome_frame_http_header_host.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_CFHttpHeaderIFrame) {
  SimpleBrowserTest(IE, kFullTabHttpHeaderPageIFrame);
}

const wchar_t kFullTabHttpHeaderPageFrameset[] =
    L"chrome_frame_http_header_frameset.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_CFHttpHeaderFrameSet) {
  SimpleBrowserTest(IE, kFullTabHttpHeaderPageFrameset);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_InitializeHidden) {
  SimpleBrowserTest(FIREFOX, kInitializeHiddenPage);
}

// Disabled due to a problem with Opera.
// http://b/issue?id=1708275
TEST_F(ChromeFrameTestWithWebServer,
       DISABLED_WidgetModeOpera_InitializeHidden) {
  OptionalBrowserTest(OPERA, kInitializeHiddenPage);
}

const wchar_t kInHeadPage[] = L"in_head.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_InHead) {
  SimpleBrowserTest(FIREFOX, kInHeadPage);
}

const wchar_t kVersionPage[] = L"version.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_Version) {
  VersionTest(IE, kVersionPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_Version) {
  VersionTest(FIREFOX, kVersionPage);
}

const wchar_t kSessionIdPage[] = L"sessionid.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_SessionIdPrivilege) {
  SessionIdTest(FIREFOX, kSessionIdPage, 1, "OK");
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_SessionIdNoPrivilege) {
  SessionIdTest(FIREFOX, kSessionIdPage, 0, "no sessionId");
}

const wchar_t kEventListenerPage[] = L"event_listener.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_EventListener) {
  SimpleBrowserTest(IE, kEventListenerPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_EventListener) {
  SimpleBrowserTest(FIREFOX, kEventListenerPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_EventListener) {
  OptionalBrowserTest(OPERA, kEventListenerPage);
}

const wchar_t kPrivilegedApisPage[] = L"privileged_apis_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_PrivilegedApis) {
  SimpleBrowserTest(IE, kPrivilegedApisPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeFF_PrivilegedApis) {
  SimpleBrowserTest(FIREFOX, kPrivilegedApisPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeOpera_PrivilegedApis) {
  OptionalBrowserTest(OPERA, kPrivilegedApisPage);
}

const wchar_t kMetaTagPage[] = L"meta_tag.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_MetaTag) {
  SimpleBrowserTest(IE, kMetaTagPage);
}

const wchar_t kCFProtocolPage[] = L"cf_protocol.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_CFProtocol) {
  // Temporarily enable  gcf: protocol for this test.
  SetConfigBool(kAllowUnsafeURLs, true);
  SimpleBrowserTest(IE, kCFProtocolPage);
  SetConfigBool(kAllowUnsafeURLs, false);
}

const wchar_t kPersistentCookieTest[] =
    L"persistent_cookie_test_page.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_PersistentCookieTest) {
  SimpleBrowserTest(IE, kPersistentCookieTest);
}

const wchar_t kNavigateOutPage[] = L"navigate_out.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_NavigateOut) {
  SimpleBrowserTest(IE, kNavigateOutPage);
}

const wchar_t kReferrerMainTest[] = L"referrer_main.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_ReferrerTest) {
  SimpleBrowserTest(IE, kReferrerMainTest);
}

const wchar_t kSubFrameTestPage[] = L"full_tab_sub_frame_main.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_SubFrame) {
  SimpleBrowserTest(IE, kSubFrameTestPage);
}

const wchar_t kSubIFrameTestPage[] = L"full_tab_sub_iframe_main.html";
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_SubIFrame) {
  SimpleBrowserTest(IE, kSubIFrameTestPage);
}

const wchar_t kXMLHttpRequestTestUrl[] =
    L"xmlhttprequest_test.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_XHRTest) {
  SimpleBrowserTest(IE, kXMLHttpRequestTestUrl);
}

const wchar_t kInstallFlowTestUrl[] =
    L"install_flow_test.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_InstallFlowTest) {
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    ScopedChromeFrameRegistrar::UnregisterAtPath(
        ScopedChromeFrameRegistrar::GetChromeFrameBuildPath().value(),
        chrome_frame_test::GetTestBedType());

    ASSERT_TRUE(LaunchBrowser(IE, kInstallFlowTestUrl));

    loop_.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

    ScopedChromeFrameRegistrar::RegisterAtPath(
        ScopedChromeFrameRegistrar::GetChromeFrameBuildPath().value(),
        chrome_frame_test::GetTestBedType());

    server_mock_.ExpectAndHandlePostedResult(CFInvocation(CFInvocation::NONE),
                                             kPostedResultSubstring);
    loop_.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

    chrome_frame_test::CloseAllIEWindows();
    ASSERT_EQ("OK", server_mock_.posted_result());
  }
}

const wchar_t kMultipleCFInstancesTestUrl[] =
    L"multiple_cf_instances_main.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_MultipleCFInstances) {
  SimpleBrowserTest(IE, kMultipleCFInstancesTestUrl);
}

// TODO(ananta)
// Disabled until I figure out why this does not work on Firefox.
TEST_F(ChromeFrameTestWithWebServer,
       DISABLED_WidgetModeFF_MultipleCFInstances) {
  SimpleBrowserTest(FIREFOX, kMultipleCFInstancesTestUrl);
}

const wchar_t kXHRHeaderTestUrl[] =
    L"xmlhttprequest_header_test.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_XHRHeaderTest) {
  SimpleBrowserTest(IE, kXHRHeaderTestUrl);
}

const wchar_t kDeleteCookieTest[] =
    L"fulltab_delete_cookie_test.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_DeleteCookieTest) {
  SimpleBrowserTest(IE, kDeleteCookieTest);
}

const wchar_t kAnchorUrlNavigate[] =
    L"fulltab_anchor_url_navigate.html#chrome_frame";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_AnchorUrlNavigateTest) {
  SimpleBrowserTest(IE, kAnchorUrlNavigate);
}

// Test whether POST-ing a form from an mshtml page to a CF page will cause
// the request to get reissued.  It should not.
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_TestPostReissue) {
  // The order of pages in this array is assumed to be mshtml, cf, script.
  const wchar_t* kPages[] = {
    L"full_tab_post_mshtml.html",
    L"full_tab_post_target_cf.html",
    L"chrome_frame_tester_helpers.js",
  };

  SimpleWebServerTest server(46664);
  server.PopulateStaticFileListT<test_server::FileResponse>(kPages,
      arraysize(kPages), GetCFTestFilePath());

  ASSERT_TRUE(LaunchBrowser(IE, server.FormatHttpPath(kPages[0]).c_str()));

  loop_.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

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
  // The order of pages in this array is assumed to be mshtml, cf, script.
  const wchar_t* kPages[] = {
    L"full_tab_get_mshtml.html",
    L"full_tab_get_target_cf.html",
    L"chrome_frame_tester_helpers.js",
  };

  SimpleWebServerTest server(46664);

  server.PopulateStaticFileListT<test_server::FileResponse>(kPages,
      arraysize(kPages), GetCFTestFilePath());

  ASSERT_TRUE(LaunchBrowser(IE, server.FormatHttpPath(kPages[0]).c_str()));

  loop_.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

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
    L"fulltab_set_cookie_test.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_SetCookieTest) {
  SimpleBrowserTest(IE, kSetCookieTest);
}

const wchar_t kXHRConditionalHeaderTestUrl[] =
    L"xmlhttprequest_conditional_header_test.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_XHRConditionalHeaderTest) {
  SimpleBrowserTest(IE, kXHRConditionalHeaderTestUrl);
}

const wchar_t kWindowCloseTestUrl[] =
    L"window_close.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_WindowClose) {
  SimpleBrowserTest(IE, kWindowCloseTestUrl);
}

TEST_F(ChromeFrameTestWithWebServer, FullTabModeFF_WindowClose) {
  SimpleBrowserTest(FIREFOX, kWindowCloseTestUrl);
}

std::string GetHeaderValue(const std::string& headers,
                           const char* header_name) {
  net::HttpUtil::HeadersIterator it(headers.begin(), headers.end(),
                                    "\r\n");
  while (it.GetNext()) {
    if (lstrcmpiA(it.name().c_str(), header_name) == 0) {
      return it.values();
    }
  }
  return "";
}

// Specialized implementation of test_server::FileResponse that supports
// adding the request's User-Agent header to the returned document.
// The class also supports $request_id$ which will be replaced with an
// id that's incremented each time the response is sent over a socket.
class UaTemplateFileResponse : public test_server::FileResponse {
 public:
  typedef test_server::FileResponse SuperClass;

  UaTemplateFileResponse(const char* request_path, const FilePath& file_path)
      : test_server::FileResponse(request_path, file_path), request_id_(0) {
  }

  virtual bool Matches(const test_server::Request& r) const {
    bool ret = SuperClass::Matches(r);
    if (ret)
      ua_ = GetHeaderValue(r.headers(), "User-Agent");
    return ret;
  }

  virtual size_t ContentLength() const {
    const char kRequestIdTemplate[] = "$request_id$";
    const char kUserAgentTemplate[] = "$UA$";

    size_t length = SuperClass::ContentLength();
    DCHECK(length);
    content_.assign(reinterpret_cast<const char*>(file_->data()),
                    file_->length());
    size_t i = content_.find(kUserAgentTemplate);
    if (i != std::string::npos)
      content_.replace(i, arraysize(kUserAgentTemplate) - 1, ua_);
    i = content_.find(kRequestIdTemplate);
    if (i != std::string::npos) {
      content_.replace(i, arraysize(kRequestIdTemplate) - 1,
                       base::StringPrintf("%i", request_id_));
    }
    return content_.length();
  }

  virtual void WriteContents(ListenSocket* socket) const {
    DCHECK(content_.length());
    socket->Send(content_.c_str(), content_.length(), false);
    request_id_++;
  }

 protected:
  mutable std::string ua_;
  mutable std::string content_;
  mutable int request_id_;
};

// This test simulates a URL that on first request returns a document
// that should be rendered in mshtml, then pops up a sign-in page that
// after signing in, refreshes the original page that should then return
// a page that needs to be rendered in GCF.
//
// This test currently fails because GCF does not add the chromeframe header
// to requests that mshtml initiates via IInternetSession::CreateBinding.
TEST_F(ChromeFrameTestWithWebServer, FAILS_FullTabModeIE_RefreshMshtmlTest) {
  const wchar_t* kPages[] = {
    L"mshtml_refresh_test.html",
    L"mshtml_refresh_test_popup.html",
  };

  SimpleWebServerTest server(46664);
  server.PopulateStaticFileListT<UaTemplateFileResponse>(kPages,
      arraysize(kPages), GetCFTestFilePath());

  ASSERT_TRUE(LaunchBrowser(IE, server.FormatHttpPath(kPages[0]).c_str()));

  loop_.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  test_server::SimpleWebServer* ws = server.web_server();
  const test_server::ConnectionList& connections = ws->connections();
  test_server::ConnectionList::const_iterator it = connections.begin();
  int requests_for_first_page = 0;
  for (; it != connections.end(); ++it) {
    test_server::Connection* c = (*it);
    const test_server::Request& r = c->request();
    if (!r.path().empty() &&
        ASCIIToWide(r.path().substr(1)).compare(kPages[0]) == 0) {
      requests_for_first_page++;
      std::string ua(GetHeaderValue(r.headers(), "User-Agent"));
      EXPECT_NE(std::string::npos, ua.find("chromeframe"));
    }
  }
  EXPECT_GT(requests_for_first_page, 1);
}

// See bug 36694 for details.  http://crbug.com/36694
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_TestDownloadFromForm) {
  chrome_frame_test::MockWindowObserver win_observer_mock;
  win_observer_mock.WatchWindow("File Download", "");
  win_observer_mock.WatchWindow("View Downloads*", "");

  // The content of our HTML test page.  This will be returned whenever
  // we reply to a GET request.
  static const char kHtml[] =
      "<html><head>\n"
      "<title>ChromeFrame Form Download Test</title>\n"
      // To see how this test runs with only IE (no CF in the picture), comment
      // out this meta tag.  The outcome of the test should be identical.
      "<meta http-equiv=\"X-UA-Compatible\" content=\"chrome=1\" />\n"
      "</head>\n"
      "<script language=\"javascript\">\n"
      "function SubmitForm() {\n"
      "  var form = document.forms['myform'];\n"
      "  form.action = document.location;\n"
      "  form.submit();\n"
      "  return true;\n"
      "}\n"
      "</script>\n"
      "<body onload=\"SubmitForm();\">\n"
      "<form method=\"post\" action=\"foo.html\" id=\"myform\">\n"
      "  <input type=\"hidden\" name=\"Field1\" value=\"myvalue\" />\n"
      "  <input type=\"button\" name=\"btn\" value=\"Test Download\" "
          "onclick=\"return SubmitForm();\" id=\"Button1\"/>\n"
      "</form></body></html>\n";

  // The content of our HTML test page.  This will be returned whenever
  // we reply to a POST request.
  static const char kText[] =
      "This is a text file (in case you were wondering).";

  // This http response class will return an HTML document that contains
  // a form whenever it receives a GET request.  Whenever it gets a POST
  // request, it will respond with a text file that needs to be downloaded
  // (content-disposition is "attachment").
  class CustomResponse : public test_server::ResponseForPath {
   public:
    explicit CustomResponse(const char* path)
      : test_server::ResponseForPath(path), is_post_(false),
        post_requests_(0), get_requests_(0) {
    }

    virtual bool GetContentType(std::string* content_type) const {
      DCHECK(!is_post_);
      return false;
    }

    virtual size_t ContentLength() const {
      DCHECK(!is_post_);
      return sizeof(kHtml) - 1;
    }

    virtual bool GetCustomHeaders(std::string* headers) const {
      if (!is_post_)
        return false;
      *headers = StringPrintf(
          "HTTP/1.1 200 OK\r\n"
          "Content-Disposition: attachment;filename=\"test.txt\"\r\n"
          "Content-Type: application/text\r\n"
          "Connection: close\r\n"
          "Content-Length: %i\r\n\r\n", sizeof(kText) - 1);
      return true;
    }

    virtual bool Matches(const test_server::Request& r) const {
      bool match = __super::Matches(r);
      if (match) {
        is_post_ = LowerCaseEqualsASCII(r.method().c_str(), "post");
      }
      return match;
    }

    virtual void WriteContents(ListenSocket* socket) const {
      if (is_post_) {
        socket->Send(kText, sizeof(kText) - 1, false);
      } else {
        socket->Send(kHtml, sizeof(kHtml) - 1, false);
      }
    }

    virtual void IncrementAccessCounter() {
      __super::IncrementAccessCounter();
      if (is_post_) {
        post_requests_++;
      } else {
        get_requests_++;
      }
    }

    size_t get_request_count() const {
      return get_requests_;
    }

    size_t post_request_count() const {
      return get_requests_;
    }

   protected:
    mutable bool is_post_;
    size_t post_requests_;
    size_t get_requests_;
  };

  EXPECT_CALL(win_observer_mock, OnWindowOpen(_))
      .Times(testing::AtMost(1))
      .WillOnce(chrome_frame_test::DoCloseWindow());

  EXPECT_CALL(win_observer_mock, OnWindowClose(_))
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop_));

  SimpleWebServerTest server(46664);
  CustomResponse* response = new CustomResponse("/form.html");
  server.web_server()->AddResponse(response);

  std::wstring url(server.FormatHttpPath(L"form.html"));

  ASSERT_TRUE(LaunchBrowser(IE, url.c_str()));
  loop_.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  EXPECT_EQ(1, response->get_request_count());
  EXPECT_EQ(1, response->post_request_count());
}
