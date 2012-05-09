// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome_frame/test/test_scrubber.h"
#include "net/base/mime_util.h"
#include "net/http/http_util.h"

using chrome_frame_test::kChromeFrameLongNavigationTimeout;
using chrome_frame_test::kChromeFrameVeryLongNavigationTimeout;

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
  if (add_no_cache_header) {
    ss << "Cache-Control: no-cache\r\n";
    ss << "Expires: Tue, 15 Nov 1994 08:12:31 GMT\r\n";
  }
  return ss.str();
}

std::string GetMockHttpHeaders(const FilePath& mock_http_headers_path) {
  std::string headers;
  file_util::ReadFileToString(mock_http_headers_path, &headers);
  return headers;
}

class ChromeFrameTestEnvironment: public testing::Environment {
 public:
  virtual ~ChromeFrameTestEnvironment() {}
  virtual void SetUp() OVERRIDE {
    ScopedChromeFrameRegistrar::RegisterDefaults();
  }
};

::testing::Environment* const chrome_frame_env =
    ::testing::AddGlobalTestEnvironment(new ChromeFrameTestEnvironment);

}  // namespace

FilePath ChromeFrameTestWithWebServer::test_file_path_;
FilePath ChromeFrameTestWithWebServer::results_dir_;
FilePath ChromeFrameTestWithWebServer::CFInstall_path_;
FilePath ChromeFrameTestWithWebServer::CFInstance_path_;
ScopedTempDir ChromeFrameTestWithWebServer::temp_dir_;
FilePath ChromeFrameTestWithWebServer::chrome_user_data_dir_;
chrome_frame_test::TimedMsgLoop* ChromeFrameTestWithWebServer::loop_;
testing::StrictMock<MockWebServerListener>*
    ChromeFrameTestWithWebServer::listener_mock_;
testing::StrictMock<MockWebServer>* ChromeFrameTestWithWebServer::server_mock_;

ChromeFrameTestWithWebServer::ChromeFrameTestWithWebServer() {
}

// static
void ChromeFrameTestWithWebServer::SetUpTestCase() {
  FilePath chrome_frame_source_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &chrome_frame_source_path);
  chrome_frame_source_path = chrome_frame_source_path.Append(
      FILE_PATH_LITERAL("chrome_frame"));

  test_file_path_ = chrome_frame_source_path
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"));

  results_dir_ = chrome_frame_test::GetTestDataFolder().AppendASCII("dump");

  // Copy the CFInstance.js and CFInstall.js files from src\chrome_frame to
  // src\chrome_frame\test\data.
  FilePath CFInstance_src_path;
  FilePath CFInstall_src_path;

  CFInstance_src_path = chrome_frame_source_path.AppendASCII("CFInstance.js");
  CFInstance_path_ = test_file_path_.AppendASCII("CFInstance.js");

  ASSERT_TRUE(file_util::CopyFile(CFInstance_src_path, CFInstance_path_));

  CFInstall_src_path = chrome_frame_source_path.AppendASCII("CFInstall.js");
  CFInstall_path_ = test_file_path_.AppendASCII("CFInstall.js");

  ASSERT_TRUE(file_util::CopyFile(CFInstall_src_path, CFInstall_path_));

  loop_ = new chrome_frame_test::TimedMsgLoop();
  loop_->set_snapshot_on_timeout(true);
  listener_mock_ = new testing::StrictMock<MockWebServerListener>();
  server_mock_ = new testing::StrictMock<MockWebServer>(
      1337, ASCIIToWide(chrome_frame_test::GetLocalIPv4Address()),
      chrome_frame_test::GetTestDataFolder());
  server_mock_->set_listener(listener_mock_);
}

// static
void ChromeFrameTestWithWebServer::TearDownTestCase() {
  delete server_mock_;
  server_mock_ = NULL;
  delete listener_mock_;
  listener_mock_ = NULL;
  delete loop_;
  loop_ = NULL;
  file_util::Delete(CFInstall_path_, false);
  file_util::Delete(CFInstance_path_, false);
  if (temp_dir_.IsValid())
    EXPECT_TRUE(temp_dir_.Delete());
}

// static
const FilePath& ChromeFrameTestWithWebServer::GetChromeUserDataDirectory() {
  if (!temp_dir_.IsValid()) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    chrome_user_data_dir_ = temp_dir_.path().AppendASCII("User Data");
  }
  return chrome_user_data_dir_;
}

void ChromeFrameTestWithWebServer::SetUp() {
  // Make sure that we are not accidentally enabling gcf protocol.
  SetConfigBool(kAllowUnsafeURLs, false);

  server_mock().ClearResults();
  server_mock().ExpectAndServeAnyRequests(CFInvocation(CFInvocation::NONE));
  server_mock().set_expected_result("OK");
}

void ChromeFrameTestWithWebServer::TearDown() {
  CloseBrowser();
  loop().RunAllPending();
  testing::Mock::VerifyAndClear(listener_mock_);
  testing::Mock::VerifyAndClear(server_mock_);
}

bool ChromeFrameTestWithWebServer::LaunchBrowser(BrowserKind browser,
                                                 const wchar_t* page) {
  std::wstring url = page;

  // We should resolve the URL only if it is a relative url.
  GURL parsed_url(WideToUTF8(page));
  if (!parsed_url.has_scheme()) {
    url = server_mock().Resolve(page);
  }

  browser_ = browser;
  if (browser == IE) {
    browser_handle_.Set(chrome_frame_test::LaunchIE(url));
  } else if (browser == CHROME) {
    const FilePath& user_data_dir = GetChromeUserDataDirectory();
    chrome_frame_test::OverrideDataDirectoryForThisTest(user_data_dir.value());
    browser_handle_.Set(chrome_frame_test::LaunchChrome(url, user_data_dir));
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
      LOG(ERROR) << "WaitForSingleObject returned " << wait;
    }
  } else {
    LOG(ERROR) << "No attempts to close browser windows";
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

bool ChromeFrameTestWithWebServer::WaitForTestToComplete(
    base::TimeDelta duration) {
  loop().RunFor(duration);
  return !loop().WasTimedOut();
}

bool ChromeFrameTestWithWebServer::WaitForOnLoad(int milliseconds) {
  return false;
}

const wchar_t kPostedResultSubstring[] = L"/writefile/";

void ChromeFrameTestWithWebServer::SimpleBrowserTestExpectedResult(
    BrowserKind browser, const wchar_t* page, const char* result) {
  int tries = 0;
  ExpectAndHandlePostedResult();
  // Retry tests that timeout once; see http://crbug.com/96449.
  do {
    // NOTE: Failed ASSERTs cause this function to exit immediately.
    // Don't take a snapshot on the first try.
    loop().set_snapshot_on_timeout(tries != 0);
    ASSERT_TRUE(LaunchBrowser(browser, page));
    if (WaitForTestToComplete(TestTimeouts::action_max_timeout())) {
      // The test exited without timing out.  Confirm that the expected response
      // was posted and return.
      ASSERT_EQ(result, server_mock().posted_result());
      break;
    }
    ASSERT_EQ(std::string(), server_mock().posted_result())
        << "Test timed out yet provided a result.";
    ASSERT_EQ(0, tries++) << "Failing test due to two timeouts.";
    // Close the browser and try a second time.
    CloseBrowser();
    LOG(ERROR) << "Retrying test once since it timed out.";
  } while (true);
  loop().set_snapshot_on_timeout(true);
}

void ChromeFrameTestWithWebServer::SimpleBrowserTest(BrowserKind browser,
    const wchar_t* page) {
  SimpleBrowserTestExpectedResult(browser, page, "OK");
}

void ChromeFrameTestWithWebServer::ExpectAndHandlePostedResult() {
  EXPECT_CALL(listener_mock(), OnExpectedResponse())
      .WillRepeatedly(QUIT_LOOP_SOON(loop(),
                                     base::TimeDelta::FromMilliseconds(100)));
  server_mock().ExpectAndHandlePostedResult(CFInvocation(CFInvocation::NONE),
                                            kPostedResultSubstring);
}

void ChromeFrameTestWithWebServer::VersionTest(BrowserKind browser,
    const wchar_t* page) {
  FilePath plugin_path;
  PathService::Get(base::DIR_MODULE, &plugin_path);
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

    bool system_install = ver_system != NULL;
    FilePath cf_dll_path(installer::GetChromeInstallPath(system_install, dist));
    cf_dll_path = cf_dll_path.Append(UTF8ToWide(
        ver_system.get() ? ver_system->GetString() : ver_user->GetString()));
    cf_dll_path = cf_dll_path.Append(kChromeFrameDllName);
    version_info = FileVersionInfo::CreateFileVersionInfo(cf_dll_path);
    if (version_info)
      version = version_info->product_version();
  }

  server_mock().set_expected_result(WideToUTF8(version));

  EXPECT_TRUE(version_info);
  EXPECT_FALSE(version.empty());

  SimpleBrowserTestExpectedResult(browser, page, WideToASCII(version).c_str());
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
  if (listener_ && posted_result_ == expected_result_)
    listener_->OnExpectedResponse();
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
      VLOG(1) << "Going to send file (" << WideToUTF8(file_path.value())
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
    VLOG(1) << "Going to send 404 for non-existent file ("
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

const wchar_t kNavigateURLAbsolutePage[] =
    L"navigateurl_absolute_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_NavigateURLAbsolute) {
  SimpleBrowserTest(IE, kNavigateURLAbsolutePage);
}

const wchar_t kNavigateURLRelativePage[] =
    L"navigateurl_relative_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_NavigateURLRelative) {
  SimpleBrowserTest(IE, kNavigateURLRelativePage);
}

const wchar_t kNavigateSimpleObjectFocus[] = L"simple_object_focus.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_ObjectFocus) {
  SimpleBrowserTest(IE, kNavigateSimpleObjectFocus);
}

const wchar_t kiframeBasicPage[] = L"iframe_basic_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_iframeBasic) {
  SimpleBrowserTest(IE, kiframeBasicPage);
}

const wchar_t kSrcPropertyTestPage[] = L"src_property_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_SrcProperty) {
  SimpleBrowserTest(IE, kSrcPropertyTestPage);
}

const wchar_t kCFInstanceBasicTestPage[] = L"CFInstance_basic_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceBasic) {
  SimpleBrowserTest(IE, kCFInstanceBasicTestPage);
}

const wchar_t kCFISingletonPage[] = L"CFInstance_singleton_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceSingleton) {
  SimpleBrowserTest(IE, kCFISingletonPage);
}

const wchar_t kCFIDelayPage[] = L"CFInstance_delay_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceDelay) {
  SimpleBrowserTest(IE, kCFIDelayPage);
}

const wchar_t kCFIFallbackPage[] = L"CFInstance_fallback_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceFallback) {
  SimpleBrowserTest(IE, kCFIFallbackPage);
}

const wchar_t kCFINoSrcPage[] = L"CFInstance_no_src_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceNoSrc) {
  SimpleBrowserTest(IE, kCFINoSrcPage);
}

const wchar_t kCFIIfrOnLoadPage[] = L"CFInstance_iframe_onload_host.html";

// disabled since it's unlikely that we care about this case
TEST_F(ChromeFrameTestWithWebServer,
       DISABLED_WidgetModeIE_CFInstanceIfrOnLoad) {
  SimpleBrowserTest(IE, kCFIIfrOnLoadPage);
}

const wchar_t kCFIZeroSizePage[] = L"CFInstance_zero_size_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceZeroSize) {
  SimpleBrowserTest(IE, kCFIZeroSizePage);
}

const wchar_t kCFIIfrPostPage[] = L"CFInstance_iframe_post_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceIfrPost) {
  SimpleBrowserTest(IE, kCFIIfrPostPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeChrome_CFInstanceIfrPost) {
  SimpleBrowserTest(CHROME, kCFIIfrPostPage);
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
TEST_F(ChromeFrameTestWithWebServer, WidgetModeChrome_CFInstancePost) {
  SimpleBrowserTest(CHROME, kCFIPostPage);
}

const wchar_t kCFIRPCPage[] = L"CFInstance_rpc_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceRPC) {
  if (chrome_frame_test::GetInstalledIEVersion() == IE_9) {
    LOG(INFO) << "Not running test on Vista/Windows 7 with IE9";
    return;
  }
  SimpleBrowserTest(IE, kCFIRPCPage);
}

TEST_F(ChromeFrameTestWithWebServer, WidgetModeChrome_CFInstanceRPC) {
  SimpleBrowserTest(CHROME, kCFIRPCPage);
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

TEST_F(ChromeFrameTestWithWebServer, WidgetModeChrome_CFInstanceRPCInternal) {
  SimpleBrowserTest(CHROME, kCFIRPCInternalPage);
}

const wchar_t kCFIDefaultCtorPage[] =
    L"CFInstance_default_ctor_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_CFInstanceDefaultCtor) {
  SimpleBrowserTest(IE, kCFIDefaultCtorPage);
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

const wchar_t kVersionPage[] = L"version.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_Version) {
  VersionTest(IE, kVersionPage);
}

const wchar_t kEventListenerPage[] = L"event_listener.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_EventListener) {
  SimpleBrowserTest(IE, kEventListenerPage);
}

const wchar_t kPrivilegedApisPage[] = L"privileged_apis_host.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_PrivilegedApis) {
  SimpleBrowserTest(IE, kPrivilegedApisPage);
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
        GetChromeFrameBuildPath().value(),
        chrome_frame_test::GetTestBedType());

    ASSERT_TRUE(LaunchBrowser(IE, kInstallFlowTestUrl));

    loop().RunFor(kChromeFrameLongNavigationTimeout);

    ScopedChromeFrameRegistrar::RegisterAtPath(
        GetChromeFrameBuildPath().value(),
        chrome_frame_test::GetTestBedType());

    ExpectAndHandlePostedResult();
    loop().RunFor(kChromeFrameLongNavigationTimeout);

    chrome_frame_test::CloseAllIEWindows();
    ASSERT_EQ("OK", server_mock().posted_result());
  }
}

const wchar_t kMultipleCFInstancesTestUrl[] =
    L"multiple_cf_instances_main.html";

TEST_F(ChromeFrameTestWithWebServer, WidgetModeIE_MultipleCFInstances) {
  SimpleBrowserTest(IE, kMultipleCFInstancesTestUrl);
}

const wchar_t kXHRHeaderTestUrl[] =
    L"xmlhttprequest_header_test.html";

// Marking as flaky since it occasionally times out. crbug.com/127395.
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_XHRHeaderTest) {
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

  loop().RunFor(kChromeFrameLongNavigationTimeout);

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

  loop().RunFor(kChromeFrameVeryLongNavigationTimeout);

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

  virtual void WriteContents(net::ListenSocket* socket) const {
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

  loop().RunFor(kChromeFrameLongNavigationTimeout);

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

    virtual void WriteContents(net::ListenSocket* socket) const {
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
      .WillOnce(QUIT_LOOP(loop()));

  SimpleWebServerTest server(46664);
  CustomResponse* response = new CustomResponse("/form.html");
  server.web_server()->AddResponse(response);

  std::wstring url(server.FormatHttpPath(L"form.html"));

  ASSERT_TRUE(LaunchBrowser(IE, url.c_str()));
  loop().RunFor(kChromeFrameLongNavigationTimeout);

  EXPECT_EQ(1, response->get_request_count());
  EXPECT_EQ(1, response->post_request_count());
}
