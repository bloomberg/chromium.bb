// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_mock_http_job.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

using content::BrowserThread;
using net::URLRequestMockHTTPJob;

namespace {

const LocalSharedObjectsContainer* GetSiteSettingsCookieContainer(
    Browser* browser) {
  TabSpecificContentSettings* settings =
      TabSpecificContentSettings::FromWebContents(
          browser->tab_strip_model()->GetWebContentsAt(0));
  return static_cast<const LocalSharedObjectsContainer*>(
      &settings->allowed_local_shared_objects());
}

}  // namespace

class ContentSettingsTest : public InProcessBrowserTest {
 public:
  ContentSettingsTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
  }

  void SetUpOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

  // Check the cookie for the given URL in an incognito window.
  void CookieCheckIncognitoWindow(const GURL& url, bool cookies_enabled) {
    ASSERT_TRUE(content::GetCookies(browser()->profile(), url).empty());

    Browser* incognito = CreateIncognitoBrowser();
    ASSERT_TRUE(content::GetCookies(incognito->profile(), url).empty());
    ui_test_utils::NavigateToURL(incognito, url);
    ASSERT_EQ(cookies_enabled,
              !content::GetCookies(incognito->profile(), url).empty());

    // Ensure incognito cookies don't leak to regular profile.
    ASSERT_TRUE(content::GetCookies(browser()->profile(), url).empty());

    // Ensure cookies get wiped after last incognito window closes.
    CloseBrowserSynchronously(incognito);

    incognito = CreateIncognitoBrowser();
    ASSERT_TRUE(content::GetCookies(incognito->profile(), url).empty());
    CloseBrowserSynchronously(incognito);
  }

  void PreBasic(const GURL& url) {
    ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());

    CookieCheckIncognitoWindow(url, true);

    ui_test_utils::NavigateToURL(browser(), url);
    ASSERT_FALSE(GetCookies(browser()->profile(), url).empty());
  }

  void Basic(const GURL& url) {
    ASSERT_FALSE(GetCookies(browser()->profile(), url).empty());
  }

  net::EmbeddedTestServer https_server_;
};

// Sanity check on cookies before we do other tests. While these can be written
// in content_browsertests, we want to verify Chrome's cookie storage and how it
// handles incognito windows.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, PRE_BasicCookies) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL http_url = embedded_test_server()->GetURL("/setcookie.html");
  PreBasic(http_url);
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest, BasicCookies) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL http_url = embedded_test_server()->GetURL("/setcookie.html");
  Basic(http_url);
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest, PRE_BasicCookiesHttps) {
  ASSERT_TRUE(https_server_.Start());
  GURL https_url = https_server_.GetURL("/setcookie.html");
  PreBasic(https_url);
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest, BasicCookiesHttps) {
  ASSERT_TRUE(https_server_.Start());
  GURL https_url = https_server_.GetURL("/setcookie.html");
  Basic(https_url);
}

// Verify that cookies are being blocked.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, PRE_BlockCookies) {
  ASSERT_TRUE(embedded_test_server()->Start());
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  GURL url = embedded_test_server()->GetURL("/setcookie.html");
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());
  CookieCheckIncognitoWindow(url, false);
}

// Ensure that the setting persists.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, BlockCookies) {
  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            CookieSettingsFactory::GetForProfile(browser()->profile())
                ->GetDefaultCookieSetting(NULL));
}

// Verify that cookies can be allowed and set using exceptions for particular
// website(s) when all others are blocked.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, AllowCookiesUsingExceptions) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/setcookie.html");
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());

  settings->SetCookieSetting(url, CONTENT_SETTING_ALLOW);

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_FALSE(GetCookies(browser()->profile(), url).empty());
}

// Verify that cookies can be blocked for a specific website using exceptions.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, BlockCookiesUsingExceptions) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/setcookie.html");
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(url, CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());

  ASSERT_TRUE(https_server_.Start());
  GURL unblocked_url = https_server_.GetURL("/cookie1.html");

  ui_test_utils::NavigateToURL(browser(), unblocked_url);
  ASSERT_FALSE(GetCookies(browser()->profile(), unblocked_url).empty());
}

// This fails on ChromeOS because kRestoreOnStartup is ignored and the startup
// preference is always "continue where I left off.
#if !defined(OS_CHROMEOS)

// Verify that cookies can be allowed and set using exceptions for particular
// website(s) only for a session when all others are blocked.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest,
                       PRE_AllowCookiesForASessionUsingExceptions) {
  // NOTE: don't use test_server here, since we need the port to be the same
  // across the restart.
  GURL url = URLRequestMockHTTPJob::GetMockUrl("setcookie.html");
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());

  settings->SetCookieSetting(url, CONTENT_SETTING_SESSION_ONLY);
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_FALSE(GetCookies(browser()->profile(), url).empty());
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest,
                       AllowCookiesForASessionUsingExceptions) {
  GURL url = URLRequestMockHTTPJob::GetMockUrl("setcookie.html");
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());
}

#endif // !CHROME_OS

// Regression test for http://crbug.com/63649.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, RedirectLoopCookies) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url = embedded_test_server()->GetURL("/redirect-loop.html");

  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), test_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(base::UTF8ToUTF16(test_url.spec() + " failed to load"),
            web_contents->GetTitle());

  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)->
      IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

// TODO(jww): This should be removed after strict secure cookies is enabled for
// all and this test should be moved into ContentSettingsTest above.
class ContentSettingsStrictSecureCookiesBrowserTest
    : public ContentSettingsTest {
 protected:
  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  }
};

// This test verifies that if strict secure cookies is enabled, the site
// settings accurately reflect that an attempt to create a secure cookie by an
// insecure origin fails.
IN_PROC_BROWSER_TEST_F(ContentSettingsStrictSecureCookiesBrowserTest, Cookies) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());

  GURL http_url = embedded_test_server()->GetURL("/setsecurecookie.html");
  GURL https_url = https_server.GetURL("/setsecurecookie.html");

  ui_test_utils::NavigateToURL(browser(), http_url);
  EXPECT_TRUE(GetSiteSettingsCookieContainer(browser())->cookies()->empty());

  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("/setsecurecookie.html"));
  EXPECT_FALSE(GetSiteSettingsCookieContainer(browser())->cookies()->empty());
};

IN_PROC_BROWSER_TEST_F(ContentSettingsTest, ContentSettingsBlockDataURLs) {
  GURL url("data:text/html,<title>Data URL</title><script>alert(1)</script>");

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(base::UTF8ToUTF16("Data URL"), web_contents->GetTitle());

  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)->
      IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
}

// Tests that if redirect across origins occurs, the new process still gets the
// content settings before the resource headers.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, RedirectCrossOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());

  net::HostPortPair host_port = embedded_test_server()->host_port_pair();
  DCHECK_EQ(host_port.host(), std::string("127.0.0.1"));

  std::string redirect(base::StringPrintf(
      "http://localhost:%d/redirect-cross-origin.html", host_port.port()));
  GURL test_url =
      embedded_test_server()->GetURL("/server-redirect?" + redirect);

  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), test_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)->
      IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

#if defined(ENABLE_PLUGINS)
class PepperContentSettingsSpecialCasesTest : public ContentSettingsTest {
 protected:
  static const char kExternalClearKeyMimeType[];

  // Registers any CDM plugins not registered by default.
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if defined(ENABLE_PEPPER_CDMS)
    // Platform-specific filename relative to the chrome executable.
#if defined(OS_WIN)
    const char kLibraryName[] = "clearkeycdmadapter.dll";
#else  // !defined(OS_WIN)
#if defined(OS_MACOSX)
    const char kLibraryName[] = "clearkeycdmadapter.plugin";
#elif defined(OS_POSIX)
    const char kLibraryName[] = "libclearkeycdmadapter.so";
#endif  // defined(OS_MACOSX)
#endif  // defined(OS_WIN)

    // Append the switch to register the External Clear Key CDM.
    base::FilePath::StringType pepper_plugins = BuildPepperPluginRegistration(
        kLibraryName, "Clear Key CDM", kExternalClearKeyMimeType);
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
    // The CDM must be registered when it is a component.
    pepper_plugins.append(FILE_PATH_LITERAL(","));
    pepper_plugins.append(
        BuildPepperPluginRegistration(kWidevineCdmAdapterFileName,
                                      kWidevineCdmDisplayName,
                                      kWidevineCdmPluginMimeType));
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
    command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                     pepper_plugins);
#endif  // defined(ENABLE_PEPPER_CDMS)

#if !defined(DISABLE_NACL)
    // Ensure NaCl can run.
    command_line->AppendSwitch(switches::kEnableNaCl);
#endif
  }

  void RunLoadPepperPluginTest(const char* mime_type, bool expect_loaded) {
    const char* expected_result = expect_loaded ? "Loaded" : "Not Loaded";
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    base::string16 expected_title(base::ASCIIToUTF16(expected_result));
    content::TitleWatcher title_watcher(web_contents, expected_title);

    // GetTestUrl assumes paths, so we must append query parameters to result.
    GURL file_url = ui_test_utils::GetTestUrl(
        base::FilePath(),
        base::FilePath().AppendASCII("load_pepper_plugin.html"));
    GURL url(file_url.spec() +
             base::StringPrintf("?mimetype=%s", mime_type));
    ui_test_utils::NavigateToURL(browser(), url);

    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    EXPECT_EQ(!expect_loaded,
              TabSpecificContentSettings::FromWebContents(web_contents)->
                  IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  }

  void RunJavaScriptBlockedTest(const char* html_file,
                                bool expect_is_javascript_content_blocked) {
    // Because JavaScript is blocked, <title> will be the only title set.
    // Checking for it ensures that the page loaded, though that is not always
    // sufficient - see below.
    const char* const kExpectedTitle = "Initial Title";
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TabSpecificContentSettings* tab_settings =
        TabSpecificContentSettings::FromWebContents(web_contents);
    base::string16 expected_title(base::ASCIIToUTF16(kExpectedTitle));
    content::TitleWatcher title_watcher(web_contents, expected_title);

    // Because JavaScript is blocked, we cannot rely on JavaScript to set a
    // title, telling us the test is complete.
    // As a result, it is possible to reach the IsContentBlocked() checks below
    // before the blocked content can be reported to the browser process.
    // See http://crbug.com/306702.
    // Therefore, when expecting blocked content, we must wait until it has been
    // reported by checking IsContentBlocked() when notified that
    // NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED. (It is not sufficient to wait
    // for just the notification because the same notification is reported for
    // other reasons and the notification contains no indication of what
    // caused it.)
    content::WindowedNotificationObserver javascript_content_blocked_observer(
              chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
              base::Bind(&TabSpecificContentSettings::IsContentBlocked,
                                   base::Unretained(tab_settings),
                                   CONTENT_SETTINGS_TYPE_JAVASCRIPT));

    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(), base::FilePath().AppendASCII(html_file));
    ui_test_utils::NavigateToURL(browser(), url);

    // Always wait for the page to load.
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

    if (expect_is_javascript_content_blocked) {
      javascript_content_blocked_observer.Wait();
    } else {
      // Since there is no notification that content is not blocked and no
      // content is blocked when |expect_is_javascript_content_blocked| is
      // false, javascript_content_blocked_observer would never succeed.
      // There is no way to ensure blocked content would not have been reported
      // after the check below. For coverage of this scenario, we must rely on
      // the TitleWatcher adding sufficient delay most of the time.
    }

    EXPECT_EQ(expect_is_javascript_content_blocked,
              tab_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
    EXPECT_FALSE(tab_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS));
  }

 private:
  // Builds the string to pass to kRegisterPepperPlugins for a single
  // plugin using the provided parameters and a dummy version.
  // Multiple results may be passed to kRegisterPepperPlugins, separated by ",".
  base::FilePath::StringType BuildPepperPluginRegistration(
      const char* library_name,
      const char* display_name,
      const char* mime_type) {
    base::FilePath plugin_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &plugin_dir));

    base::FilePath plugin_lib = plugin_dir.AppendASCII(library_name);
    EXPECT_TRUE(base::PathExists(plugin_lib));

    base::FilePath::StringType pepper_plugin = plugin_lib.value();
    std::string string_to_append = "#";
    string_to_append.append(display_name);
    string_to_append.append("#A CDM#0.1.0.0;");
    string_to_append.append(mime_type);

#if defined(OS_WIN)
    pepper_plugin.append(base::ASCIIToUTF16(string_to_append));
#else
    pepper_plugin.append(string_to_append);
#endif

    return pepper_plugin;
  }
};

const char
PepperContentSettingsSpecialCasesTest::kExternalClearKeyMimeType[] =
    "application/x-ppapi-clearkey-cdm";

class PepperContentSettingsSpecialCasesPluginsBlockedTest
    : public PepperContentSettingsSpecialCasesTest {
 public:
  void SetUpOnMainThread() override {
    PepperContentSettingsSpecialCasesTest::SetUpOnMainThread();
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                   CONTENT_SETTING_BLOCK);
  }
};

class PepperContentSettingsSpecialCasesJavaScriptBlockedTest
    : public PepperContentSettingsSpecialCasesTest {
 public:
  void SetUpOnMainThread() override {
    PepperContentSettingsSpecialCasesTest::SetUpOnMainThread();
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                   CONTENT_SETTING_ALLOW);
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                   CONTENT_SETTING_BLOCK);
  }
};

#if defined(ENABLE_PEPPER_CDMS)
// A sanity check to verify that the plugin that is used as a baseline below
// can be loaded.
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesTest, Baseline) {
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                 CONTENT_SETTING_ALLOW);

  RunLoadPepperPluginTest(kExternalClearKeyMimeType, true);
}
#endif  // defined(ENABLE_PEPPER_CDMS)

// The following tests verify that Pepper plugins that use JavaScript settings
// instead of Plugins settings still work when Plugins are blocked.

#if defined(ENABLE_PEPPER_CDMS)
// The plugin successfully loaded above is blocked.
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesPluginsBlockedTest,
                       Normal) {
  RunLoadPepperPluginTest(kExternalClearKeyMimeType, false);
}

#if defined(WIDEVINE_CDM_AVAILABLE) && !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesPluginsBlockedTest,
                       WidevineCdm) {
  RunLoadPepperPluginTest(kWidevineCdmPluginMimeType, true);
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && !defined(OS_CHROMEOS)
#endif  // defined(ENABLE_PEPPER_CDMS)

#if !defined(DISABLE_NACL)
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesPluginsBlockedTest,
                       NaCl) {
  RunLoadPepperPluginTest("application/x-nacl", true);
}
#endif  // !defined(DISABLE_NACL)

// The following tests verify that those same Pepper plugins do not work when
// JavaScript is blocked.

#if defined(ENABLE_PEPPER_CDMS)
// A plugin with no special behavior is not blocked when JavaScript is blocked.
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesJavaScriptBlockedTest,
                       Normal) {
  RunJavaScriptBlockedTest("load_clearkey_no_js.html", false);
}

#if defined(WIDEVINE_CDM_AVAILABLE)
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesJavaScriptBlockedTest,
                       WidevineCdm) {
  RunJavaScriptBlockedTest("load_widevine_no_js.html", true);
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
#endif  // defined(ENABLE_PEPPER_CDMS)

#if !defined(DISABLE_NACL)
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesJavaScriptBlockedTest,
                       NaCl) {
  RunJavaScriptBlockedTest("load_nacl_no_js.html", true);
}
#endif  // !defined(DISABLE_NACL)

#endif  // defined(ENABLE_PLUGINS)
