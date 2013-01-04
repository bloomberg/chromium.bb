// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "net/test/test_server.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

using content::BrowserThread;
using content::URLRequestMockHTTPJob;

class ContentSettingsTest : public InProcessBrowserTest {
 public:
  ContentSettingsTest()
      : https_server_(
            net::TestServer::TYPE_HTTPS,
            net::TestServer::SSLOptions(net::TestServer::SSLOptions::CERT_OK),
            FilePath(FILE_PATH_LITERAL("chrome/test/data"))) {
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

  net::TestServer https_server_;
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
      CookieSettings::Factory::GetForProfile(browser()->profile());
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
      CookieSettings::Factory::GetForProfile(browser()->profile());
  settings->SetCookieSetting(
      ContentSettingsPattern::FromURL(url),
      ContentSettingsPattern::Wildcard(), CONTENT_SETTING_BLOCK);

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
      FilePath(FILE_PATH_LITERAL("setcookie.html")));
  CookieSettings* settings =
      CookieSettings::Factory::GetForProfile(browser()->profile());
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
      FilePath(FILE_PATH_LITERAL("setcookie.html")));
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

  content::WebContents* web_contents = chrome::GetActiveWebContents(browser());
  ASSERT_EQ(UTF8ToUTF16(test_url.spec() + " failed to load"),
            web_contents->GetTitle());

  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)->
      IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

IN_PROC_BROWSER_TEST_F(ContentSettingsTest, ContentSettingsBlockDataURLs) {
  GURL url("data:text/html,<title>Data URL</title><script>alert(1)</script>");

  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* web_contents = chrome::GetActiveWebContents(browser());
  ASSERT_EQ(UTF8ToUTF16("Data URL"), web_contents->GetTitle());

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

  content::WebContents* web_contents = chrome::GetActiveWebContents(browser());

  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)->
      IsContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
}

#if !defined(USE_AURA)  // No NPAPI plugins with Aura.

class ClickToPlayPluginTest : public ContentSettingsTest {
 public:
  ClickToPlayPluginTest() {}

#if defined(OS_MACOSX)
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    FilePath plugin_dir;
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
      FilePath(), FilePath().AppendASCII("clicktoplay.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  string16 expected_title(ASCIIToUTF16("OK"));
  content::TitleWatcher title_watcher(
      chrome::GetActiveWebContents(browser()), expected_title);

  content::RenderViewHost* host =
      chrome::GetActiveWebContents(browser())->GetRenderViewHost();
  host->Send(new ChromeViewMsg_LoadBlockedPlugins(
      host->GetRoutingID(), std::string()));

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Verify that plugins can be allowed on a domain by adding an exception
IN_PROC_BROWSER_TEST_F(ClickToPlayPluginTest, AllowException) {
  GURL url = ui_test_utils::GetTestUrl(
      FilePath(), FilePath().AppendASCII("clicktoplay.html"));

  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);
  browser()->profile()->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURL(url),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "",
      CONTENT_SETTING_ALLOW);

  string16 expected_title(ASCIIToUTF16("OK"));
  content::TitleWatcher title_watcher(
      chrome::GetActiveWebContents(browser()), expected_title);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Verify that plugins can be blocked on a domain by adding an exception.
IN_PROC_BROWSER_TEST_F(ClickToPlayPluginTest, BlockException) {
  GURL url = ui_test_utils::GetTestUrl(
      FilePath(), FilePath().AppendASCII("clicktoplay.html"));

  browser()->profile()->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURL(url),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_PLUGINS,
      "",
      CONTENT_SETTING_BLOCK);

  string16 expected_title(ASCIIToUTF16("Click To Play"));
  content::TitleWatcher title_watcher(
      chrome::GetActiveWebContents(browser()), expected_title);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ClickToPlayPluginTest, LoadAllBlockedPlugins) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);

  GURL url = ui_test_utils::GetTestUrl(
      FilePath(), FilePath().AppendASCII("load_all_blocked_plugins.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  string16 expected_title1(ASCIIToUTF16("1"));
  content::TitleWatcher title_watcher1(
      chrome::GetActiveWebContents(browser()), expected_title1);

  content::RenderViewHost* host =
      chrome::GetActiveWebContents(browser())->GetRenderViewHost();
  host->Send(new ChromeViewMsg_LoadBlockedPlugins(
      host->GetRoutingID(), std::string()));
  EXPECT_EQ(expected_title1, title_watcher1.WaitAndGetTitle());

  string16 expected_title2(ASCIIToUTF16("2"));
  content::TitleWatcher title_watcher2(
      chrome::GetActiveWebContents(browser()), expected_title2);

  ASSERT_TRUE(content::ExecuteScript(
      chrome::GetActiveWebContents(browser()), "window.inject()"));

  EXPECT_EQ(expected_title2, title_watcher2.WaitAndGetTitle());
}

// If this flakes, use http://crbug.com/113057.
IN_PROC_BROWSER_TEST_F(ClickToPlayPluginTest, NoCallbackAtLoad) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);

  GURL url("data:application/vnd.npapi-test,CallOnStartup();");
  ui_test_utils::NavigateToURL(browser(), url);

  // Inject the callback function into the HTML page generated by the browser.
  ASSERT_TRUE(content::ExecuteScript(
      chrome::GetActiveWebContents(browser()),
      "CallOnStartup = function() { document.title = \"OK\"; }"));

  string16 expected_title(ASCIIToUTF16("OK"));
  content::TitleWatcher title_watcher(
      chrome::GetActiveWebContents(browser()), expected_title);

  content::RenderViewHost* host =
      chrome::GetActiveWebContents(browser())->GetRenderViewHost();
  host->Send(new ChromeViewMsg_LoadBlockedPlugins(
      host->GetRoutingID(), std::string()));

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ClickToPlayPluginTest, DeleteSelfAtLoad) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);

  GURL url = ui_test_utils::GetTestUrl(
      FilePath(), FilePath().AppendASCII("plugin_delete_self_at_load.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  string16 expected_title(ASCIIToUTF16("OK"));
  content::TitleWatcher title_watcher(
      chrome::GetActiveWebContents(browser()), expected_title);

  content::RenderViewHost* host =
      chrome::GetActiveWebContents(browser())->GetRenderViewHost();
  host->Send(new ChromeViewMsg_LoadBlockedPlugins(
      host->GetRoutingID(), std::string()));

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

#endif  // !defined(USE_AURA)
