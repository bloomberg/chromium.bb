// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
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
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/url_request/url_request_mock_http_job.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

using content::BrowserThread;
using net::URLRequestMockHTTPJob;

class ContentSettingsTest : public InProcessBrowserTest {
 public:
  ContentSettingsTest()
      : https_server_(net::SpawnedTestServer::TYPE_HTTPS,
                      net::SpawnedTestServer::SSLOptions(
                          net::SpawnedTestServer::SSLOptions::CERT_OK),
                      base::FilePath(FILE_PATH_LITERAL("chrome/test/data"))) {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
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
    content::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::Source<Browser>(incognito));

    chrome::CloseWindow(incognito);

#if defined(OS_MACOSX)
    // BrowserWindowController depends on the auto release pool being recycled
    // in the message loop to delete itself, which frees the Browser object
    // which fires this event.
    AutoreleasePool()->Recycle();
#endif

    signal.Wait();

    incognito = CreateIncognitoBrowser();
    ASSERT_TRUE(content::GetCookies(incognito->profile(), url).empty());
    chrome::CloseWindow(incognito);
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

  net::SpawnedTestServer https_server_;
};

// Sanity check on cookies before we do other tests. While these can be written
// in content_browsertests, we want to verify Chrome's cookie storage and how it
// handles incognito windows.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, PRE_BasicCookies) {
  ASSERT_TRUE(test_server()->Start());
  GURL http_url = test_server()->GetURL("files/setcookie.html");
  PreBasic(http_url);
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest, BasicCookies) {
  ASSERT_TRUE(test_server()->Start());
  GURL http_url = test_server()->GetURL("files/setcookie.html");
  Basic(http_url);
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest, PRE_BasicCookiesHttps) {
  ASSERT_TRUE(https_server_.Start());
  GURL https_url = https_server_.GetURL("files/setcookie.html");
  PreBasic(https_url);
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest, BasicCookiesHttps) {
  ASSERT_TRUE(https_server_.Start());
  GURL https_url = https_server_.GetURL("files/setcookie.html");
  Basic(https_url);
}

// Verify that cookies are being blocked.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, PRE_BlockCookies) {
  ASSERT_TRUE(test_server()->Start());
  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  GURL url = test_server()->GetURL("files/setcookie.html");
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());
  CookieCheckIncognitoWindow(url, false);
}

// Ensure that the setting persists.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, BlockCookies) {
  ASSERT_EQ(
      CONTENT_SETTING_BLOCK,
      CookieSettings::Factory::GetForProfile(browser()->profile())->
          GetDefaultCookieSetting(NULL));
}

// Verify that cookies can be allowed and set using exceptions for particular
// website(s) when all others are blocked.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, AllowCookiesUsingExceptions) {
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL("files/setcookie.html");
  CookieSettings* settings =
      CookieSettings::Factory::GetForProfile(browser()->profile()).get();
  settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());

  settings->SetCookieSetting(
      ContentSettingsPattern::FromURL(url),
      ContentSettingsPattern::Wildcard(), CONTENT_SETTING_ALLOW);

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_FALSE(GetCookies(browser()->profile(), url).empty());
}

// Verify that cookies can be blocked for a specific website using exceptions.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, BlockCookiesUsingExceptions) {
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL("files/setcookie.html");
  CookieSettings* settings =
      CookieSettings::Factory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(ContentSettingsPattern::FromURL(url),
                             ContentSettingsPattern::Wildcard(),
                             CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());

  ASSERT_TRUE(https_server_.Start());
  GURL unblocked_url = https_server_.GetURL("files/cookie1.html");

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
  GURL url = URLRequestMockHTTPJob::GetMockUrl(
      base::FilePath(FILE_PATH_LITERAL("setcookie.html")));
  CookieSettings* settings =
      CookieSettings::Factory::GetForProfile(browser()->profile()).get();
  settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());

  settings->SetCookieSetting(
      ContentSettingsPattern::FromURL(url),
      ContentSettingsPattern::Wildcard(), CONTENT_SETTING_SESSION_ONLY);
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_FALSE(GetCookies(browser()->profile(), url).empty());
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest,
                       AllowCookiesForASessionUsingExceptions) {
  GURL url = URLRequestMockHTTPJob::GetMockUrl(
      base::FilePath(FILE_PATH_LITERAL("setcookie.html")));
  ASSERT_TRUE(GetCookies(browser()->profile(), url).empty());
}

#endif // !CHROME_OS

// Regression test for http://crbug.com/63649.
IN_PROC_BROWSER_TEST_F(ContentSettingsTest, RedirectLoopCookies) {
  ASSERT_TRUE(test_server()->Start());

  GURL test_url = test_server()->GetURL("files/redirect-loop.html");

  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), test_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(base::UTF8ToUTF16(test_url.spec() + " failed to load"),
            web_contents->GetTitle());

  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)->
      IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest, ContentSettingsBlockDataURLs) {
  GURL url("data:text/html,<title>Data URL</title><script>alert(1)</script>");

  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);

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
  ASSERT_TRUE(test_server()->Start());

  net::HostPortPair host_port = test_server()->host_port_pair();
  DCHECK_EQ(host_port.host(), std::string("127.0.0.1"));

  std::string redirect(base::StringPrintf(
      "http://localhost:%d/files/redirect-cross-origin.html",
      host_port.port()));
  GURL test_url = test_server()->GetURL("server-redirect?" + redirect);

  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), test_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)->
      IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

// On Aura NPAPI only works on Windows.
#if !defined(USE_AURA) || defined(OS_WIN)

class ClickToPlayPluginTest : public ContentSettingsTest {
 public:
  ClickToPlayPluginTest() {}

#if defined(OS_MACOSX)
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    base::FilePath plugin_dir;
    PathService::Get(base::DIR_MODULE, &plugin_dir);
    plugin_dir = plugin_dir.AppendASCII("plugins");
    // The plugins directory isn't read by default on the Mac, so it needs to be
    // explicitly registered.
    command_line->AppendSwitchPath(switches::kExtraPluginDir, plugin_dir);
  }
#endif
};

IN_PROC_BROWSER_TEST_F(ClickToPlayPluginTest, Basic) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath().AppendASCII("clicktoplay.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  base::string16 expected_title(base::ASCIIToUTF16("OK"));
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ChromePluginServiceFilter* filter = ChromePluginServiceFilter::GetInstance();
  int process_id = web_contents->GetMainFrame()->GetProcess()->GetID();
  base::FilePath path(FILE_PATH_LITERAL("blah"));
  EXPECT_FALSE(filter->CanLoadPlugin(process_id, path));
  filter->AuthorizeAllPlugins(web_contents, true, std::string());
  EXPECT_TRUE(filter->CanLoadPlugin(process_id, path));

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Verify that plugins can be allowed on a domain by adding an exception
IN_PROC_BROWSER_TEST_F(ClickToPlayPluginTest, AllowException) {
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath().AppendASCII("clicktoplay.html"));

  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);
  browser()->profile()->GetHostContentSettingsMap()
      ->SetContentSetting(ContentSettingsPattern::FromURL(url),
                          ContentSettingsPattern::Wildcard(),
                          CONTENT_SETTINGS_TYPE_PLUGINS,
                          std::string(),
                          CONTENT_SETTING_ALLOW);

  base::string16 expected_title(base::ASCIIToUTF16("OK"));
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Verify that plugins can be blocked on a domain by adding an exception.
IN_PROC_BROWSER_TEST_F(ClickToPlayPluginTest, BlockException) {
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath().AppendASCII("clicktoplay.html"));

  browser()->profile()->GetHostContentSettingsMap()
      ->SetContentSetting(ContentSettingsPattern::FromURL(url),
                          ContentSettingsPattern::Wildcard(),
                          CONTENT_SETTINGS_TYPE_PLUGINS,
                          std::string(),
                          CONTENT_SETTING_BLOCK);

  base::string16 expected_title(base::ASCIIToUTF16("Click To Play"));
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Crashes on Mac Asan.  http://crbug.com/239169
#if defined(OS_MACOSX)
#define MAYBE_LoadAllBlockedPlugins DISABLED_LoadAllBlockedPlugins
// TODO(jschuh): Flaky plugin tests. crbug.com/244653
#elif defined(OS_WIN) && defined(ARCH_CPU_X86_64)
#define MAYBE_LoadAllBlockedPlugins DISABLED_LoadAllBlockedPlugins
#else
#define MAYBE_LoadAllBlockedPlugins LoadAllBlockedPlugins
#endif
IN_PROC_BROWSER_TEST_F(ClickToPlayPluginTest, MAYBE_LoadAllBlockedPlugins) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath().AppendASCII("load_all_blocked_plugins.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  base::string16 expected_title1(base::ASCIIToUTF16("1"));
  content::TitleWatcher title_watcher1(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title1);

  ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
      browser()->tab_strip_model()->GetActiveWebContents(), true,
      std::string());
  EXPECT_EQ(expected_title1, title_watcher1.WaitAndGetTitle());

  base::string16 expected_title2(base::ASCIIToUTF16("2"));
  content::TitleWatcher title_watcher2(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title2);

  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(), "window.inject()"));

  EXPECT_EQ(expected_title2, title_watcher2.WaitAndGetTitle());
}

// If this flakes, use http://crbug.com/113057.
// TODO(jschuh): Hanging plugin tests. crbug.com/244653
#if !defined(OS_WIN) && !defined(ARCH_CPU_X86_64)
IN_PROC_BROWSER_TEST_F(ClickToPlayPluginTest, NoCallbackAtLoad) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);

  GURL url("data:application/vnd.npapi-test,CallOnStartup();");
  ui_test_utils::NavigateToURL(browser(), url);

  std::string script("CallOnStartup = function() { ");
  script.append("document.documentElement.appendChild");
  script.append("(document.createElement(\"head\")); ");
  script.append("document.title = \"OK\"; }");

  // Inject the callback function into the HTML page generated by the browser.
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(), script));

  base::string16 expected_title(base::ASCIIToUTF16("OK"));
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
      browser()->tab_strip_model()->GetActiveWebContents(), true,
      std::string());

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}
#endif

IN_PROC_BROWSER_TEST_F(ClickToPlayPluginTest, DeleteSelfAtLoad) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath().AppendASCII("plugin_delete_self_at_load.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  base::string16 expected_title(base::ASCIIToUTF16("OK"));
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
      browser()->tab_strip_model()->GetActiveWebContents(), true,
      std::string());

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

#endif  // !defined(USE_AURA) || defined(OS_WIN)

#if defined(ENABLE_PLUGINS)
class PepperContentSettingsSpecialCasesTest : public ContentSettingsTest {
 protected:
  static const char* const kExternalClearKeyMimeType;

  // Registers any CDM plugins not registered by default.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
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
    pepper_plugin.append(FILE_PATH_LITERAL("#"));
#if defined(OS_WIN)
    pepper_plugin.append(base::ASCIIToWide(display_name));
#else
    pepper_plugin.append(display_name);
#endif
    pepper_plugin.append(FILE_PATH_LITERAL("#A CDM#0.1.0.0;"));
#if defined(OS_WIN)
    pepper_plugin.append(base::ASCIIToWide(mime_type));
#else
    pepper_plugin.append(mime_type);
#endif

    return pepper_plugin;
  }
};

const char* const
PepperContentSettingsSpecialCasesTest::kExternalClearKeyMimeType =
    "application/x-ppapi-clearkey-cdm";

class PepperContentSettingsSpecialCasesPluginsBlockedTest
    : public PepperContentSettingsSpecialCasesTest {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    PepperContentSettingsSpecialCasesTest::SetUpOnMainThread();
    browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
        CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);
  }
};

class PepperContentSettingsSpecialCasesJavaScriptBlockedTest
    : public PepperContentSettingsSpecialCasesTest {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    PepperContentSettingsSpecialCasesTest::SetUpOnMainThread();
    browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
        CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_ALLOW);
    browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
        CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);
  }
};

#if defined(ENABLE_PEPPER_CDMS)
// A sanity check to verify that the plugin that is used as a baseline below
// can be loaded.
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesTest, Baseline) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_ALLOW);

  RunLoadPepperPluginTest(kExternalClearKeyMimeType, true);
}
#endif  // defined(ENABLE_PEPPER_CDMS)

// The following tests verify that Pepper plugins that use JavaScript settings
// instead of Plug-ins settings still work when Plug-ins are blocked.

#if defined(ENABLE_PEPPER_CDMS)
// The plugin successfully loaded above is blocked.
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesPluginsBlockedTest,
                       Normal) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif
  RunLoadPepperPluginTest(kExternalClearKeyMimeType, false);
}

#if defined(WIDEVINE_CDM_AVAILABLE)
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesPluginsBlockedTest,
                       WidevineCdm) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif
  RunLoadPepperPluginTest(kWidevineCdmPluginMimeType, true);
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
#endif  // defined(ENABLE_PEPPER_CDMS)

#if !defined(DISABLE_NACL)
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesPluginsBlockedTest,
                       NaCl) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif
  RunLoadPepperPluginTest("application/x-nacl", true);
}
#endif  // !defined(DISABLE_NACL)

// The following tests verify that those same Pepper plugins do not work when
// JavaScript is blocked.

#if defined(ENABLE_PEPPER_CDMS)
// A plugin with no special behavior is not blocked when JavaScript is blocked.
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesJavaScriptBlockedTest,
                       Normal) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif
  RunJavaScriptBlockedTest("load_clearkey_no_js.html", false);
}

#if defined(WIDEVINE_CDM_AVAILABLE)
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesJavaScriptBlockedTest,
                       WidevineCdm) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif
  RunJavaScriptBlockedTest("load_widevine_no_js.html", true);
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
#endif  // defined(ENABLE_PEPPER_CDMS)

#if !defined(DISABLE_NACL)
IN_PROC_BROWSER_TEST_F(PepperContentSettingsSpecialCasesJavaScriptBlockedTest,
                       NaCl) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif
  RunJavaScriptBlockedTest("load_nacl_no_js.html", true);
}
#endif  // !defined(DISABLE_NACL)

#endif  // defined(ENABLE_PLUGINS)
