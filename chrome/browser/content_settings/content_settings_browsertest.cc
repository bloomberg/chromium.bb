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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "net/test/test_server.h"

// Regression test for http://crbug.com/63649.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, RedirectLoopCookies) {
  ASSERT_TRUE(test_server()->Start());

  GURL test_url = test_server()->GetURL("files/redirect-loop.html");

  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), test_url);

  TabContentsWrapper* tab_contents = browser()->GetSelectedTabContentsWrapper();
  ASSERT_EQ(UTF8ToUTF16(test_url.spec() + " failed to load"),
            tab_contents->web_contents()->GetTitle());

  EXPECT_TRUE(tab_contents->content_settings()->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_COOKIES));
}

IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, ContentSettingsBlockDataURLs) {
  GURL url("data:text/html,<title>Data URL</title><script>alert(1)</script>");

  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), url);

  TabContentsWrapper* tab_contents = browser()->GetSelectedTabContentsWrapper();
  ASSERT_EQ(UTF8ToUTF16("Data URL"), tab_contents->web_contents()->GetTitle());

  EXPECT_TRUE(tab_contents->content_settings()->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT));
}

// Tests that if redirect across origins occurs, the new process still gets the
// content settings before the resource headers.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, RedirectCrossOrigin) {
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

  TabContentsWrapper* tab_contents = browser()->GetSelectedTabContentsWrapper();

  EXPECT_TRUE(tab_contents->content_settings()->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_COOKIES));
}

#if !defined(USE_AURA)  // No NPAPI plugins with Aura.

class ClickToPlayPluginTest : public InProcessBrowserTest {
 public:
  ClickToPlayPluginTest() {
    EnableDOMAutomation();
  }

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
      FilePath().AppendASCII("npapi"),
      FilePath().AppendASCII("clicktoplay.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  string16 expected_title(ASCIIToUTF16("OK"));
  ui_test_utils::TitleWatcher title_watcher(
      browser()->GetSelectedWebContents(), expected_title);

  content::RenderViewHost* host =
      browser()->GetSelectedWebContents()->GetRenderViewHost();
  host->Send(new ChromeViewMsg_LoadBlockedPlugins(
      host->GetRoutingID(), std::string()));

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ClickToPlayPluginTest, LoadAllBlockedPlugins) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);

  GURL url = ui_test_utils::GetTestUrl(
      FilePath().AppendASCII("npapi"),
      FilePath().AppendASCII("load_all_blocked_plugins.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  string16 expected_title1(ASCIIToUTF16("1"));
  ui_test_utils::TitleWatcher title_watcher1(
      browser()->GetSelectedWebContents(), expected_title1);

  content::RenderViewHost* host =
      browser()->GetSelectedWebContents()->GetRenderViewHost();
  host->Send(new ChromeViewMsg_LoadBlockedPlugins(
      host->GetRoutingID(), std::string()));
  EXPECT_EQ(expected_title1, title_watcher1.WaitAndGetTitle());

  string16 expected_title2(ASCIIToUTF16("2"));
  ui_test_utils::TitleWatcher title_watcher2(
      browser()->GetSelectedWebContents(), expected_title2);

  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      browser()->GetSelectedWebContents()->GetRenderViewHost(),
      L"", L"window.inject()"));

  EXPECT_EQ(expected_title2, title_watcher2.WaitAndGetTitle());
}

// If this flakes, use http://crbug.com/113057.
IN_PROC_BROWSER_TEST_F(ClickToPlayPluginTest, NoCallbackAtLoad) {
  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_BLOCK);

  GURL url("data:application/vnd.npapi-test,CallOnStartup();");
  ui_test_utils::NavigateToURL(browser(), url);

  // Inject the callback function into the HTML page generated by the browser.
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      browser()->GetSelectedWebContents()->GetRenderViewHost(),
      L"", L"CallOnStartup = function() { document.title = \"OK\"; }"));

  string16 expected_title(ASCIIToUTF16("OK"));
  ui_test_utils::TitleWatcher title_watcher(
      browser()->GetSelectedWebContents(), expected_title);

  content::RenderViewHost* host =
      browser()->GetSelectedWebContents()->GetRenderViewHost();
  host->Send(new ChromeViewMsg_LoadBlockedPlugins(
      host->GetRoutingID(), std::string()));

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

#endif  // !defined(USE_AURA)
