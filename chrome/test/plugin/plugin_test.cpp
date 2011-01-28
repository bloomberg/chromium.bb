// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the top plugins to catch regressions in our plugin host code, as
// well as in the out of process code.  Currently this tests:
//  Flash
//  Real
//  QuickTime
//  Windows Media Player
//    -this includes both WMP plugins.  npdsplay.dll is the older one that
//     comes with XP.  np-mswmp.dll can be downloaded from Microsoft and
//     needs SP2 or Vista.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <comutil.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/browser/plugin_download_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/capturing_net_log.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_resolver.h"
#include "net/base/net_util.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/socket/client_socket_factory.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "third_party/npapi/bindings/npapi.h"
#include "webkit/plugins/npapi/plugin_constants_win.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/plugin_switches.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#endif

class PluginTest : public UITest {
 public:
  // Generate the URL for testing a particular test.
  // HTML for the tests is all located in test_directory\plugin\<testcase>
  // Set |mock_http| to true to use mock HTTP server.
  static GURL GetTestUrl(const std::string &test_case, bool mock_http) {
    static const FilePath::CharType kPluginPath[] = FILE_PATH_LITERAL("plugin");
    if (mock_http) {
      FilePath plugin_path = FilePath(kPluginPath).AppendASCII(test_case);
      return URLRequestMockHTTPJob::GetMockUrl(plugin_path);
    }

    FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.Append(kPluginPath).AppendASCII(test_case);
    return net::FilePathToFileURL(path);
  }

 protected:
  virtual void SetUp() {
#if defined(OS_WIN)
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(), "MediaPlayerNew") == 0) {
      // The installer adds our process names to the registry key below.  Since
      // the installer might not have run on this machine, add it manually.
      base::win::RegKey regkey;
      if (regkey.Open(HKEY_LOCAL_MACHINE,
                      L"Software\\Microsoft\\MediaPlayer\\ShimInclusionList",
                      KEY_WRITE) == ERROR_SUCCESS) {
        regkey.CreateKey(L"CHROME.EXE", KEY_READ);
      }
    } else if (strcmp(test_info->name(), "MediaPlayerOld") == 0) {
      // When testing the old WMP plugin, we need to force Chrome to not load
      // the new plugin.
      launch_arguments_.AppendSwitch(switches::kUseOldWMPPlugin);
    } else if (strcmp(test_info->name(), "FlashSecurity") == 0) {
      launch_arguments_.AppendSwitchASCII(switches::kTestSandbox,
                                          "security_tests.dll");
    }
#endif  // defined(OS_WIN)

    launch_arguments_.AppendSwitch(switches::kAllowOutdatedPlugins);
    launch_arguments_.AppendSwitch(switches::kAlwaysAuthorizePlugins);

    UITest::SetUp();
  }

  void TestPlugin(const std::string& test_case,
                  int timeout,
                  bool mock_http) {
    GURL url = GetTestUrl(test_case, mock_http);
    NavigateToURL(url);
    WaitForFinish(timeout, mock_http);
  }

  // Waits for the test case to finish.
  void WaitForFinish(const int wait_time, bool mock_http) {
    static const char kTestCompleteCookie[] = "status";
    static const char kTestCompleteSuccess[] = "OK";

    GURL url = GetTestUrl("done", mock_http);
    scoped_refptr<TabProxy> tab(GetActiveTab());

    const std::string result =
        WaitUntilCookieNonEmpty(tab, url, kTestCompleteCookie, wait_time);
    ASSERT_EQ(kTestCompleteSuccess, result);
  }
};

TEST_F(PluginTest, Flash) {
  // Note: This does not work with the npwrapper on 64-bit Linux. Install the
  // native 64-bit Flash to run the test.
  // TODO(thestig) Update this list if we decide to only test against internal
  // Flash plugin in the future?
  std::string kFlashQuery =
#if defined(OS_WIN)
      "npswf32.dll"
#elif defined(OS_MACOSX)
      "Flash Player.plugin"
#elif defined(OS_POSIX)
      "libflashplayer.so"
#endif
      ;
  TestPlugin("flash.html?" + kFlashQuery,
             TestTimeouts::action_max_timeout_ms(), false);
}

class ClickToPlayPluginTest : public PluginTest {
 public:
  ClickToPlayPluginTest() {
    dom_automation_enabled_ = true;
  }
};

TEST_F(ClickToPlayPluginTest, Flash) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  ASSERT_TRUE(browser->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                                CONTENT_SETTING_BLOCK));

  GURL url = GetTestUrl("flash-clicktoplay.html", true);
  NavigateToURL(url);

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());

  ASSERT_TRUE(tab->LoadBlockedPlugins());

  WaitForFinish(TestTimeouts::action_max_timeout_ms(), true);
}

TEST_F(ClickToPlayPluginTest, FlashDocument) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  ASSERT_TRUE(browser->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                                CONTENT_SETTING_BLOCK));

  scoped_refptr<TabProxy> tab(browser->GetTab(0));
  ASSERT_TRUE(tab.get());
  GURL url = GetTestUrl("js-invoker.swf?callback=done", true);
  NavigateToURL(url);

  // Inject the callback function into the HTML page generated by the browser.
  ASSERT_TRUE(tab->ExecuteJavaScript("window.done = function() {"
                                     "  window.location = \"done.html\";"
                                     "}"));

  ASSERT_TRUE(tab->LoadBlockedPlugins());

  WaitForFinish(TestTimeouts::action_max_timeout_ms(), true);
}

#if defined(OS_WIN)
// Windows only test
TEST_F(PluginTest, DISABLED_FlashSecurity) {
  TestPlugin("flash.html", TestTimeouts::action_max_timeout_ms(), false);
}
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
// TODO(port) Port the following tests to platforms that have the required
// plugins.
// Flaky: http://crbug.com/55915
TEST_F(PluginTest, FLAKY_Quicktime) {
  TestPlugin("quicktime.html", TestTimeouts::action_max_timeout_ms(), false);
}

// Disabled - http://crbug.com/44662
TEST_F(PluginTest, DISABLED_MediaPlayerNew) {
  TestPlugin("wmp_new.html", TestTimeouts::action_max_timeout_ms(), false);
}

// http://crbug.com/4809
TEST_F(PluginTest, DISABLED_MediaPlayerOld) {
  TestPlugin("wmp_old.html", TestTimeouts::action_max_timeout_ms(), false);
}

// Disabled - http://crbug.com/44673
TEST_F(PluginTest, DISABLED_Real) {
  TestPlugin("real.html", TestTimeouts::action_max_timeout_ms(), false);
}

TEST_F(PluginTest, FlashOctetStream) {
  TestPlugin("flash-octet-stream.html",
             TestTimeouts::action_max_timeout_ms(), false);
}

#if defined(OS_WIN)
// http://crbug.com/53926
TEST_F(PluginTest, FLAKY_FlashLayoutWhilePainting) {
#else
TEST_F(PluginTest, FlashLayoutWhilePainting) {
#endif
  TestPlugin("flash-layout-while-painting.html",
             TestTimeouts::action_max_timeout_ms(), true);
}

// http://crbug.com/8690
TEST_F(PluginTest, DISABLED_Java) {
  TestPlugin("Java.html", TestTimeouts::action_max_timeout_ms(), false);
}

TEST_F(PluginTest, Silverlight) {
  TestPlugin("silverlight.html", TestTimeouts::action_max_timeout_ms(), false);
}

// This class provides functionality to test the plugin installer download
// file functionality.
class PluginInstallerDownloadTest
    : public PluginDownloadUrlHelper::DownloadDelegate,
      public testing::Test {
 public:
  // This class provides HTTP request context information for the downloads.
  class UploadRequestContext : public net::URLRequestContext {
   public:
    UploadRequestContext() {
      Initialize();
    }

    ~UploadRequestContext() {
      DVLOG(1) << __FUNCTION__;
      delete http_transaction_factory_;
      delete http_auth_handler_factory_;
      delete cert_verifier_;
      delete host_resolver_;
    }

    void Initialize() {
      host_resolver_ =
          net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                        NULL, NULL);
      cert_verifier_ = new net::CertVerifier;
      net::ProxyConfigService* proxy_config_service =
          net::ProxyService::CreateSystemProxyConfigService(NULL, NULL);
      DCHECK(proxy_config_service);

      const size_t kNetLogBound = 50u;
      net_log_.reset(new net::CapturingNetLog(kNetLogBound));

      proxy_service_ = net::ProxyService::CreateUsingSystemProxyResolver(
          proxy_config_service, 0, net_log_.get());
      DCHECK(proxy_service_);

      ssl_config_service_ = new net::SSLConfigServiceDefaults;
      http_auth_handler_factory_ = net::HttpAuthHandlerFactory::CreateDefault(
          host_resolver_);
      scoped_refptr<net::HttpNetworkSession> network_session(
          new net::HttpNetworkSession(
              host_resolver_,
              cert_verifier_,
              NULL /* dnsrr_resolver */,
              NULL /* dns_cert_checker */,
              NULL /* ssl_host_info_factory */,
              proxy_service_,
              net::ClientSocketFactory::GetDefaultFactory(),
              ssl_config_service_,
              new net::SpdySessionPool(NULL),
              http_auth_handler_factory_,
              network_delegate_,
              NULL /* net_log */));
      http_transaction_factory_ = new net::HttpCache(
          network_session,
          net::HttpCache::DefaultBackend::InMemory(0));
    }

   private:
    scoped_ptr<net::NetLog> net_log_;
    scoped_ptr<net::URLSecurityManager> url_security_manager_;
  };

  PluginInstallerDownloadTest()
      : success_(false),
        download_helper_(NULL) {}
  ~PluginInstallerDownloadTest() {}

  void Start() {
    initial_download_path_ = PluginTest::GetTestUrl("flash.html", false);
    download_helper_ = new PluginDownloadUrlHelper(
        initial_download_path_.spec(), base::GetCurrentProcId(), NULL,
        static_cast<PluginDownloadUrlHelper::DownloadDelegate*>(this));
    download_helper_->InitiateDownload(new UploadRequestContext);

    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, new MessageLoop::QuitTask,
        TestTimeouts::action_max_timeout_ms());
  }

  virtual void OnDownloadCompleted(const FilePath& download_path,
                                   bool success) {
    success_ = success;
    final_download_path_ = download_path;
    MessageLoop::current()->Quit();
    download_helper_ = NULL;
  }

  FilePath final_download_path() const {
    return final_download_path_;
  }

  FilePath initial_download_path() const {
    return final_download_path_;
  }

  bool success() const {
    return success_;
  }

 private:
  FilePath final_download_path_;
  PluginDownloadUrlHelper* download_helper_;
  bool success_;
  GURL initial_download_path_;
};

// This test validates that the plugin downloader downloads the specified file
// to a temporary path with the same file name.
TEST_F(PluginInstallerDownloadTest, PluginInstallerDownloadPathTest) {
  MessageLoop loop(MessageLoop::TYPE_IO);
  Start();
  loop.Run();

  EXPECT_TRUE(success());
  EXPECT_TRUE(initial_download_path().BaseName().value() ==
              final_download_path().BaseName().value());
}
#endif  // defined(OS_WIN)
