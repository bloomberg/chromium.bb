// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/autoupdate_interceptor.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif

const std::string kSubscribePage = "/subscribe.html";
const std::string kFeedPage = "files/feeds/feed.html";
const std::string kFeedPageMultiRel = "files/feeds/feed_multi_rel.html";
const std::string kNoFeedPage = "files/feeds/no_feed.html";
const std::string kValidFeed0 = "files/feeds/feed_script.xml";
const std::string kValidFeed1 = "files/feeds/feed1.xml";
const std::string kValidFeed2 = "files/feeds/feed2.xml";
const std::string kValidFeed3 = "files/feeds/feed3.xml";
const std::string kValidFeed4 = "files/feeds/feed4.xml";
const std::string kValidFeed5 = "files/feeds/feed5.xml";
const std::string kValidFeed6 = "files/feeds/feed6.xml";
const std::string kValidFeedNoLinks = "files/feeds/feed_nolinks.xml";
const std::string kInvalidFeed1 = "files/feeds/feed_invalid1.xml";
const std::string kInvalidFeed2 = "files/feeds/feed_invalid2.xml";
const std::string kLocalization =
    "files/extensions/browsertest/title_localized_pa/simple.html";
// We need a triple encoded string to prove that we are not decoding twice in
// subscribe.js because one layer is also stripped off when subscribe.js passes
// it to the XMLHttpRequest object.
const std::string kFeedTripleEncoded = "files/feeds/url%25255Fdecoding.html";
const std::string kHashPageA =
    "files/extensions/api_test/page_action/hash_change/test_page_A.html";
const std::string kHashPageAHash = kHashPageA + "#asdf";
const std::string kHashPageB =
    "files/extensions/api_test/page_action/hash_change/test_page_B.html";

// Looks for an ExtensionHost whose URL has the given path component (including
// leading slash).  Also verifies that the expected number of hosts are loaded.
static ExtensionHost* FindHostWithPath(ExtensionProcessManager* manager,
                                       const std::string& path,
                                       int expected_hosts) {
  ExtensionHost* host = NULL;
  int num_hosts = 0;
  for (ExtensionProcessManager::const_iterator iter = manager->begin();
       iter != manager->end(); ++iter) {
    if ((*iter)->GetURL().path() == path) {
      EXPECT_FALSE(host);
      host = *iter;
    }
    num_hosts++;
  }
  EXPECT_EQ(expected_hosts, num_hosts);
  return host;
}

// Tests that extension resources can be loaded from origins which the
// extension specifies in permissions but not from others.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, OriginPrivileges) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(LoadExtension(test_data_dir_
    .AppendASCII("origin_privileges").AppendASCII("extension")));

  GURL origin_privileges_index(
      test_server()->GetURL("files/extensions/origin_privileges/index.html"));

  std::string host_a("a.com");
  GURL::Replacements make_host_a_com;
  make_host_a_com.SetHostStr(host_a);

  std::string host_b("b.com");
  GURL::Replacements make_host_b_com;
  make_host_b_com.SetHostStr(host_b);

  // A web host that has permission.
  ui_test_utils::NavigateToURL(
      browser(), origin_privileges_index.ReplaceComponents(make_host_a_com));
  std::string result;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
    browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(document.title)",
    &result));
  EXPECT_EQ(result, "Loaded");

  // A web host that does not have permission.
  ui_test_utils::NavigateToURL(
      browser(), origin_privileges_index.ReplaceComponents(make_host_b_com));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Image failed to load");

  // A data URL. Data URLs should always be able to load chrome-extension://
  // resources.
  std::string file_source;
  ASSERT_TRUE(file_util::ReadFileToString(
      test_data_dir_.AppendASCII("origin_privileges")
                    .AppendASCII("index.html"), &file_source));
  ui_test_utils::NavigateToURL(browser(),
      GURL(std::string("data:text/html;charset=utf-8,") + file_source));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Loaded");

  // A different extension. Extensions should always be able to load each
  // other's resources.
  ASSERT_TRUE(LoadExtension(test_data_dir_
    .AppendASCII("origin_privileges").AppendASCII("extension2")));
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://pbkkcbgdkliohhfaeefcijaghglkahja/index.html"));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Loaded");
}

// Tests that we can load extension pages into the tab area and they can call
// extension APIs.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, TabContents) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html"));

  bool result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"testTabsAPI()", &result));
  EXPECT_TRUE(result);

  // There was a bug where we would crash if we navigated to a page in the same
  // extension because no new render view was getting created, so we would not
  // do some setup.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html"));
  result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"testTabsAPI()", &result));
  EXPECT_TRUE(result);
}

// Tests that GPU-related WebKit preferences are set for extension background
// pages. See http://crbug.com/64512.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WebKitPrefsBackgroundPage) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  ExtensionProcessManager* manager =
        browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host = FindHostWithPath(manager, "/backgroundpage.html", 1);
  WebPreferences prefs = host->GetWebkitPrefs();
  ASSERT_FALSE(prefs.experimental_webgl_enabled);
  ASSERT_FALSE(prefs.accelerated_compositing_enabled);
  ASSERT_FALSE(prefs.accelerated_2d_canvas_enabled);
}

// Tests that we can load page actions in the Omnibox.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, PageAction) {
  ASSERT_TRUE(test_server()->Start());

  // This page action will not show an icon, since it doesn't specify one but
  // is included here to test for a crash (http://crbug.com/25562).
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("browsertest")
                    .AppendASCII("crash_25562")));

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(0));

  // Navigate to the feed page.
  GURL feed_url = test_server()->GetURL(kFeedPage);
  ui_test_utils::NavigateToURL(browser(), feed_url);
  // We should now have one page action ready to go in the LocationBar.
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));

  // Navigate to a page with no feed.
  GURL no_feed = test_server()->GetURL(kNoFeedPage);
  ui_test_utils::NavigateToURL(browser(), no_feed);
  // Make sure the page action goes away.
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(0));
}

// Tests that we don't lose the page action icon on in-page navigations.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, PageActionInPageNavigation) {
  ASSERT_TRUE(test_server()->Start());

  FilePath extension_path(test_data_dir_.AppendASCII("api_test")
                                        .AppendASCII("page_action")
                                        .AppendASCII("hash_change"));
  ASSERT_TRUE(LoadExtension(extension_path));

  // Page action should become visible when we navigate here.
  GURL feed_url = test_server()->GetURL(kHashPageA);
  ui_test_utils::NavigateToURL(browser(), feed_url);
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));

  // In-page navigation, page action should remain.
  feed_url = test_server()->GetURL(kHashPageAHash);
  ui_test_utils::NavigateToURL(browser(), feed_url);
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));

  // Not an in-page navigation, page action should go away.
  feed_url = test_server()->GetURL(kHashPageB);
  ui_test_utils::NavigateToURL(browser(), feed_url);
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(0));
}

// Tests that the location bar forgets about unloaded page actions.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, UnloadPageAction) {
  ASSERT_TRUE(test_server()->Start());

  FilePath extension_path(test_data_dir_.AppendASCII("subscribe_page_action"));
  ASSERT_TRUE(LoadExtension(extension_path));

  // Navigation prompts the location bar to load page actions.
  GURL feed_url = test_server()->GetURL(kFeedPage);
  ui_test_utils::NavigateToURL(browser(), feed_url);
  ASSERT_TRUE(WaitForPageActionCountChangeTo(1));

  UnloadExtension(last_loaded_extension_id_);

  // Make sure the page action goes away when it's unloaded.
  ASSERT_TRUE(WaitForPageActionCountChangeTo(0));
}

// Flaky crash on Mac debug. http://crbug.com/45079
#if defined(OS_MACOSX)
#define PageActionRefreshCrash PageActionRefreshCrash
#endif
// Tests that we can load page actions in the Omnibox.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, PageActionRefreshCrash) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  ExtensionService* service = browser()->profile()->GetExtensionService();

  size_t size_before = service->extensions()->size();

  FilePath base_path = test_data_dir_.AppendASCII("browsertest")
                                     .AppendASCII("crash_44415");
  // Load extension A.
  ASSERT_TRUE(LoadExtension(base_path.AppendASCII("ExtA")));
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));
  ASSERT_EQ(size_before + 1, service->extensions()->size());
  const Extension* extensionA = service->extensions()->at(size_before);

  LOG(INFO) << "Load extension A done  : "
            << (base::TimeTicks::Now() - start_time).InMilliseconds()
            << " ms" << std::flush;

  // Load extension B.
  ASSERT_TRUE(LoadExtension(base_path.AppendASCII("ExtB")));
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(2));
  ASSERT_EQ(size_before + 2, service->extensions()->size());
  const Extension* extensionB = service->extensions()->at(size_before + 1);

  LOG(INFO) << "Load extension B done  : "
            << (base::TimeTicks::Now() - start_time).InMilliseconds()
            << " ms" << std::flush;

  ReloadExtension(extensionA->id());
  // ExtensionA has changed, so refetch it.
  ASSERT_EQ(size_before + 2, service->extensions()->size());
  extensionA = service->extensions()->at(size_before + 1);

  LOG(INFO) << "Reload extension A done: "
            << (base::TimeTicks::Now() - start_time).InMilliseconds()
            << " ms" << std::flush;

  ReloadExtension(extensionB->id());

  LOG(INFO) << "Reload extension B done: "
            << (base::TimeTicks::Now() - start_time).InMilliseconds()
            << " ms" << std::flush;

  // This is where it would crash, before http://crbug.com/44415 was fixed.
  ReloadExtension(extensionA->id());

  LOG(INFO) << "Test completed         : "
            << (base::TimeTicks::Now() - start_time).InMilliseconds()
            << " ms" << std::flush;
}

// Makes sure that the RSS detects RSS feed links, even when rel tag contains
// more than just "alternate".
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, RSSMultiRelLink) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
    test_data_dir_.AppendASCII("subscribe_page_action")));

  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(0));

  // Navigate to the feed page.
  GURL feed_url = test_server()->GetURL(kFeedPageMultiRel);
  ui_test_utils::NavigateToURL(browser(), feed_url);
  // We should now have one page action ready to go in the LocationBar.
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));
}

// Tests that tooltips of a browser action icon can be specified using UTF8.
// See http://crbug.com/25349.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, TitleLocalizationBrowserAction) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  const size_t size_before = service->extensions()->size();
  FilePath extension_path(test_data_dir_.AppendASCII("browsertest")
                                        .AppendASCII("title_localized"));
  ASSERT_TRUE(LoadExtension(extension_path));

  ASSERT_EQ(size_before + 1, service->extensions()->size());
  const Extension* extension = service->extensions()->at(size_before);

  EXPECT_STREQ(WideToUTF8(L"Hreggvi\u00F0ur: l10n browser action").c_str(),
               extension->description().c_str());
  EXPECT_STREQ(WideToUTF8(L"Hreggvi\u00F0ur is my name").c_str(),
               extension->name().c_str());
  int tab_id = ExtensionTabUtil::GetTabId(browser()->GetSelectedTabContents());
  EXPECT_STREQ(WideToUTF8(L"Hreggvi\u00F0ur").c_str(),
               extension->browser_action()->GetTitle(tab_id).c_str());
}

// Tests that tooltips of a page action icon can be specified using UTF8.
// See http://crbug.com/25349.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, TitleLocalizationPageAction) {
  ASSERT_TRUE(test_server()->Start());

  ExtensionService* service = browser()->profile()->GetExtensionService();
  const size_t size_before = service->extensions()->size();

  FilePath extension_path(test_data_dir_.AppendASCII("browsertest")
                                        .AppendASCII("title_localized_pa"));
  ASSERT_TRUE(LoadExtension(extension_path));

  // Any navigation prompts the location bar to load the page action.
  GURL url = test_server()->GetURL(kLocalization);
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));

  ASSERT_EQ(size_before + 1, service->extensions()->size());
  const Extension* extension = service->extensions()->at(size_before);

  EXPECT_STREQ(WideToUTF8(L"Hreggvi\u00F0ur: l10n page action").c_str(),
               extension->description().c_str());
  EXPECT_STREQ(WideToUTF8(L"Hreggvi\u00F0ur is my name").c_str(),
               extension->name().c_str());
  int tab_id = ExtensionTabUtil::GetTabId(browser()->GetSelectedTabContents());
  EXPECT_STREQ(WideToUTF8(L"Hreggvi\u00F0ur").c_str(),
               extension->page_action()->GetTitle(tab_id).c_str());
}

GURL GetFeedUrl(net::TestServer* server, const std::string& feed_page,
                bool direct_url, std::string extension_id) {
  GURL feed_url = server->GetURL(feed_page);
  if (direct_url) {
    // We navigate directly to the subscribe page for feeds where the feed
    // sniffing won't work, in other words, as is the case for malformed feeds.
    return GURL(std::string(chrome::kExtensionScheme) +
        chrome::kStandardSchemeSeparator +
        extension_id + std::string(kSubscribePage) + std::string("?") +
        feed_url.spec() + std::string("&synchronous"));
  } else {
    // Navigate to the feed content (which will cause the extension to try to
    // sniff the type and display the subscribe page in another tab.
    return GURL(feed_url.spec());
  }
}

static const wchar_t* jscript_feed_title =
    L"window.domAutomationController.send("
    L"  document.getElementById('title') ? "
    L"    document.getElementById('title').textContent : "
    L"    \"element 'title' not found\""
    L");";
static const wchar_t* jscript_anchor =
    L"window.domAutomationController.send("
    L"  document.getElementById('anchor_0') ? "
    L"    document.getElementById('anchor_0').textContent : "
    L"    \"element 'anchor_0' not found\""
    L");";
static const wchar_t* jscript_desc =
    L"window.domAutomationController.send("
    L"  document.getElementById('desc_0') ? "
    L"    document.getElementById('desc_0').textContent : "
    L"    \"element 'desc_0' not found\""
    L");";
static const wchar_t* jscript_error =
    L"window.domAutomationController.send("
    L"  document.getElementById('error') ? "
    L"    document.getElementById('error').textContent : "
    L"    \"No error\""
    L");";

bool ValidatePageElement(TabContents* tab,
                         const std::wstring& frame,
                         const std::wstring& javascript,
                         const std::string& expected_value) {
  std::string returned_value;
  std::string error;

  if (!ui_test_utils::ExecuteJavaScriptAndExtractString(
          tab->render_view_host(),
          frame,
          javascript, &returned_value))
    return false;

  EXPECT_STREQ(expected_value.c_str(), returned_value.c_str());
  return expected_value == returned_value;
}

// Navigates to a feed page and, if |sniff_xml_type| is set, wait for the
// extension to kick in, detect the feed and redirect to a feed preview page.
// |sniff_xml_type| is generally set to true if the feed is sniffable and false
// for invalid feeds.
void NavigateToFeedAndValidate(net::TestServer* server,
                               const std::string& url,
                               Browser* browser,
                               bool sniff_xml_type,
                               const std::string& expected_feed_title,
                               const std::string& expected_item_title,
                               const std::string& expected_item_desc,
                               const std::string& expected_error) {
  if (sniff_xml_type) {
    // TODO(finnur): Implement this is a non-flaky way.
  }

  ExtensionService* service = browser->profile()->GetExtensionService();
  const Extension* extension = service->extensions()->back();
  std::string id = extension->id();

  // Navigate to the subscribe page directly.
  ui_test_utils::NavigateToURL(browser, GetFeedUrl(server, url, true, id));

  TabContents* tab = browser->GetSelectedTabContents();
  ASSERT_TRUE(ValidatePageElement(tab,
                                  L"",
                                  jscript_feed_title,
                                  expected_feed_title));
  ASSERT_TRUE(ValidatePageElement(tab,
                                  L"//html/body/div/iframe[1]",
                                  jscript_anchor,
                                  expected_item_title));
  ASSERT_TRUE(ValidatePageElement(tab,
                                  L"//html/body/div/iframe[1]",
                                  jscript_desc,
                                  expected_item_desc));
  ASSERT_TRUE(ValidatePageElement(tab,
                                  L"//html/body/div/iframe[1]",
                                  jscript_error,
                                  expected_error));
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ParseFeedValidFeed1) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  NavigateToFeedAndValidate(test_server(), kValidFeed1, browser(), true,
                            "Feed for MyFeedTitle",
                            "Title 1",
                            "Desc",
                            "No error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ParseFeedValidFeed2) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  NavigateToFeedAndValidate(test_server(), kValidFeed2, browser(), true,
                            "Feed for MyFeed2",
                            "My item title1",
                            "This is a summary.",
                            "No error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ParseFeedValidFeed3) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  NavigateToFeedAndValidate(test_server(), kValidFeed3, browser(), true,
                            "Feed for Google Code buglist rss feed",
                            "My dear title",
                            "My dear content",
                            "No error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ParseFeedValidFeed4) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  NavigateToFeedAndValidate(test_server(), kValidFeed4, browser(), true,
                            "Feed for Title chars <script> %23 stop",
                            "Title chars  %23 stop",
                            "My dear content %23 stop",
                            "No error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ParseFeedValidFeed0) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  // Try a feed with a link with an onclick handler (before r27440 this would
  // trigger a NOTREACHED).
  NavigateToFeedAndValidate(test_server(), kValidFeed0, browser(), true,
                            "Feed for MyFeedTitle",
                            "Title 1",
                            "Desc VIDEO",
                            "No error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ParseFeedValidFeed5) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  // Feed with valid but mostly empty xml.
  NavigateToFeedAndValidate(test_server(), kValidFeed5, browser(), true,
                            "Feed for Unknown feed name",
                            "element 'anchor_0' not found",
                            "element 'desc_0' not found",
                            "This feed contains no entries.");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ParseFeedValidFeed6) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  // Feed that is technically invalid but still parseable.
  NavigateToFeedAndValidate(test_server(), kValidFeed6, browser(), true,
                            "Feed for MyFeedTitle",
                            "Title 1",
                            "Desc",
                            "No error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ParseFeedInvalidFeed1) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  // Try an empty feed.
  NavigateToFeedAndValidate(test_server(), kInvalidFeed1, browser(), false,
                            "Feed for Unknown feed name",
                            "element 'anchor_0' not found",
                            "element 'desc_0' not found",
                            "This feed contains no entries.");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ParseFeedInvalidFeed2) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  // Try a garbage feed.
  NavigateToFeedAndValidate(test_server(), kInvalidFeed2, browser(), false,
                            "Feed for Unknown feed name",
                            "element 'anchor_0' not found",
                            "element 'desc_0' not found",
                            "This feed contains no entries.");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ParseFeedInvalidFeed3) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  // Try a feed that doesn't exist.
  NavigateToFeedAndValidate(test_server(), "foo.xml", browser(), false,
                            "Feed for Unknown feed name",
                            "element 'anchor_0' not found",
                            "element 'desc_0' not found",
                            "This feed contains no entries.");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ParseFeedInvalidFeed4) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  // subscribe.js shouldn't double-decode the URL passed in. Otherwise feed
  // links such as http://search.twitter.com/search.atom?lang=en&q=%23chrome
  // will result in no feed being downloaded because %23 gets decoded to # and
  // therefore #chrome is not treated as part of the Twitter query. This test
  // uses an underscore instead of a hash, but the principle is the same. If
  // we start erroneously double decoding again, the path (and the feed) will
  // become valid resulting in a failure for this test.
  NavigateToFeedAndValidate(test_server(), kFeedTripleEncoded, browser(), true,
                            "Feed for Unknown feed name",
                            "element 'anchor_0' not found",
                            "element 'desc_0' not found",
                            "This feed contains no entries.");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, ParseFeedValidFeedNoLinks) {
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("subscribe_page_action")));

  // Valid feed but containing no links.
  NavigateToFeedAndValidate(test_server(), kValidFeedNoLinks, browser(), true,
                            "Feed for MyFeedTitle",
                            "Title with no link",
                            "Desc",
                            "No error");
}

// Tests that an error raised during an async function still fires
// the callback, but sets chrome.extension.lastError.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, LastError) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("browsertest").AppendASCII("last_error")));

  // Get the ExtensionHost that is hosting our toolstrip page.
  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host = FindHostWithPath(manager, "/bg.html", 1);

  bool result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testLastError()", &result));
  EXPECT_TRUE(result);
}

// Helper function for common code shared by the 3 WindowOpen tests below.
static void WindowOpenHelper(Browser* browser, const GURL& start_url,
                             const std::string& newtab_url,
                             TabContents** newtab_result) {
  ui_test_utils::NavigateToURL(browser, start_url);

  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      browser->GetSelectedTabContents()->render_view_host(), L"",
      L"window.open('" + UTF8ToWide(newtab_url) + L"');"));

  // Now the active tab in last active window should be the new tab.
  Browser* last_active_browser = BrowserList::GetLastActive();
  EXPECT_TRUE(last_active_browser);
  TabContents* newtab = last_active_browser->GetSelectedTabContents();
  EXPECT_TRUE(newtab);
  GURL expected_url = start_url.Resolve(newtab_url);
  if (!newtab->controller().GetLastCommittedEntry() ||
      newtab->controller().GetLastCommittedEntry()->url() != expected_url)
    ui_test_utils::WaitForNavigation(&newtab->controller());
  EXPECT_EQ(expected_url,
            newtab->controller().GetLastCommittedEntry()->url());
  if (newtab_result)
    *newtab_result = newtab;
}

// Tests that an extension page can call window.open to an extension URL and
// the new window has extension privileges.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WindowOpenExtension) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open")));

  TabContents* newtab;
  ASSERT_NO_FATAL_FAILURE(WindowOpenHelper(
      browser(),
      GURL(std::string("chrome-extension://") + last_loaded_extension_id_ +
           "/test.html"),
      "newtab.html", &newtab));

  bool result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      newtab->render_view_host(), L"", L"testExtensionApi()", &result));
  EXPECT_TRUE(result);
}

// Tests that if an extension page calls window.open to an invalid extension
// URL, the browser doesn't crash.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WindowOpenInvalidExtension) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open")));

  ASSERT_NO_FATAL_FAILURE(WindowOpenHelper(
      browser(),
      GURL(std::string("chrome-extension://") + last_loaded_extension_id_ +
           "/test.html"),
      "chrome-extension://thisissurelynotavalidextensionid/newtab.html", NULL));

  // If we got to this point, we didn't crash, so we're good.
}

// Tests that calling window.open from the newtab page to an extension URL
// gives the new window extension privileges - even though the opening page
// does not have extension privileges, we break the script connection, so
// there is no privilege leak.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WindowOpenNoPrivileges) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open")));

  TabContents* newtab;
  ASSERT_NO_FATAL_FAILURE(WindowOpenHelper(
      browser(),
      GURL("about:blank"),
      std::string("chrome-extension://") + last_loaded_extension_id_ +
          "/newtab.html",
      &newtab));

  // Extension API should succeed.
  bool result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      newtab->render_view_host(), L"", L"testExtensionApi()", &result));
  EXPECT_TRUE(result);
}

#if defined(OS_WIN)
#define MAYBE_PluginLoadUnload PluginLoadUnload
#elif defined(OS_LINUX)
// http://crbug.com/47598
#define MAYBE_PluginLoadUnload DISABLED_PluginLoadUnload
#else
// TODO(mpcomplete): http://crbug.com/29900 need cross platform plugin support.
#define MAYBE_PluginLoadUnload DISABLED_PluginLoadUnload
#endif

// Tests that a renderer's plugin list is properly updated when we load and
// unload an extension that contains a plugin.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, MAYBE_PluginLoadUnload) {
  FilePath extension_dir =
      test_data_dir_.AppendASCII("uitest").AppendASCII("plugins");

  ui_test_utils::NavigateToURL(browser(),
      net::FilePathToFileURL(extension_dir.AppendASCII("test.html")));
  TabContents* tab = browser()->GetSelectedTabContents();

  // With no extensions, the plugin should not be loaded.
  bool result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"", L"testPluginWorks()", &result));
  EXPECT_FALSE(result);

  ExtensionService* service = browser()->profile()->GetExtensionService();
  const size_t size_before = service->extensions()->size();
  ASSERT_TRUE(LoadExtension(extension_dir));
  EXPECT_EQ(size_before + 1, service->extensions()->size());
  // Now the plugin should be in the cache, but we have to reload the page for
  // it to work.
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"", L"testPluginWorks()", &result));
  EXPECT_FALSE(result);
  browser()->Reload(CURRENT_TAB);
  ui_test_utils::WaitForNavigationInCurrentTab(browser());
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"", L"testPluginWorks()", &result));
  EXPECT_TRUE(result);

  EXPECT_EQ(size_before + 1, service->extensions()->size());
  UnloadExtension(service->extensions()->at(size_before)->id());
  EXPECT_EQ(size_before, service->extensions()->size());

  // Now the plugin should be unloaded, and the page should be broken.

  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"", L"testPluginWorks()", &result));
  EXPECT_FALSE(result);

  // If we reload the extension and page, it should work again.

  ASSERT_TRUE(LoadExtension(extension_dir));
  EXPECT_EQ(size_before + 1, service->extensions()->size());
  browser()->Reload(CURRENT_TAB);
  ui_test_utils::WaitForNavigationInCurrentTab(browser());
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      tab->render_view_host(), L"", L"testPluginWorks()", &result));
  EXPECT_TRUE(result);
}

// Used to simulate a click on the first button named 'Options'.
static const wchar_t* jscript_click_option_button =
    L"(function() { "
    L"  var button = document.evaluate(\"//button[text()='Options']\","
    L"      document, null, XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE,"
    L"      null).snapshotItem(0);"
    L"  button.click();"
    L"})();";

// Test that an extension with an options page makes an 'Options' button appear
// on chrome://extensions, and that clicking the button opens a new tab with the
// extension's options page.
// Disabled.  See http://crbug.com/26948 for details.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, DISABLED_OptionsPage) {
  // Install an extension with an options page.
  ASSERT_TRUE(InstallExtension(test_data_dir_.AppendASCII("options.crx"), 1));
  ExtensionService* service = browser()->profile()->GetExtensionService();
  const ExtensionList* extensions = service->extensions();
  ASSERT_EQ(1u, extensions->size());
  const Extension* extension = extensions->at(0);

  // Go to the chrome://extensions page and click the Options button.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIExtensionsURL));
  TabStripModel* tab_strip = browser()->tabstrip_model();
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      jscript_click_option_button));

  // If the options page hasn't already come up, wait for it.
  if (tab_strip->count() == 1) {
    ui_test_utils::WaitForNewTab(browser());
  }
  ASSERT_EQ(2, tab_strip->count());

  EXPECT_EQ(extension->GetResourceURL("options.html"),
            tab_strip->GetTabContentsAt(1)->tab_contents()->GetURL());
}

// Test window.chrome.app.isInstalled .
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, PropertyAppIsInstalled) {
  std::string app_host("app.com");
  std::string nonapp_host("nonapp.com");

  host_resolver()->AddRule(app_host, "127.0.0.1");
  host_resolver()->AddRule(nonapp_host, "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL test_file_url(test_server()->GetURL("extensions/test_file.html"));
  GURL::Replacements replace_host;

  replace_host.SetHostStr(app_host);
  GURL app_url(test_file_url.ReplaceComponents(replace_host));

  replace_host.SetHostStr(nonapp_host);
  GURL non_app_url(test_file_url.ReplaceComponents(replace_host));


  // Load an app which includes app.com in its extent.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("app_dot_com_app")));


  // Test that a non-app page has chrome.app.isInstalled = false.
  ui_test_utils::NavigateToURL(browser(), non_app_url);
  std::wstring get_app_is_installed =
      L"window.domAutomationController.send("
      L"    JSON.stringify(window.chrome.app.isInstalled));";
  std::string result;
  ASSERT_TRUE(
      ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetSelectedTabContents()->render_view_host(),
          L"",
          get_app_is_installed.c_str(),
          &result));
  EXPECT_EQ("false", result);


  // Check that an app page has chrome.app.isInstalled = true.
  ui_test_utils::NavigateToURL(browser(), app_url);
  ASSERT_TRUE(
      ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetSelectedTabContents()->render_view_host(),
          L"",
          get_app_is_installed.c_str(),
          &result));
  EXPECT_EQ("true", result);


  // Test that trying to set window.chrome.app.isInstalled throws
  // an exception.
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          browser()->GetSelectedTabContents()->render_view_host(),
          L"",
          L"window.domAutomationController.send("
          L"    function() {"
          L"      try {"
          L"        window.chrome.app.isInstalled = false;"
          L"        return 'BAD: Should have thrown by now...';"
          L"      } catch (e) {"
          L"        return 'GOOD: Saw expected error.';"
          L"      }"
          L"    }()"
          L");",
          &result));
  EXPECT_EQ("GOOD: Saw expected error.", result);
}
