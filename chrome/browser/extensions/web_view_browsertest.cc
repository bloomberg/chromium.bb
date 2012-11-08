// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/compositor/compositor_setup.h"
#include "ui/gl/gl_switches.h"

class WebViewTest : public extensions::PlatformAppBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
#if !defined(OS_MACOSX)
    CHECK(test_launcher_utils::OverrideGLImplementation(
          command_line, gfx::kGLImplementationOSMesaName)) <<
          "kUseGL must not be set by test framework code!";
#endif
    ui::DisableTestCompositor();
  }

  // This method is responsible for initializing a packaged app, which contains
  // multiple webview tags. The tags have different partition identifiers and
  // their WebContent objects are returned as output. The method also verifies
  // the expected process allocation and storage partition assignment.
  // The |navigate_to_url| parameter is used to navigate the main browser
  // window.
  void NavigateAndOpenAppForIsolation(
      GURL navigate_to_url,
      content::WebContents** default_tag_contents1,
      content::WebContents** default_tag_contents2,
      content::WebContents** named_partition_contents1,
      content::WebContents** named_partition_contents2) {
    GURL::Replacements replace_host;
    std::string host_str("localhost");  // Must stay in scope with replace_host.
    replace_host.SetHostStr(host_str);

    navigate_to_url = navigate_to_url.ReplaceComponents(replace_host);

    GURL tag_url1 = test_server()->GetURL(
        "files/extensions/platform_apps/web_view_isolation/cookie.html");
    tag_url1 = tag_url1.ReplaceComponents(replace_host);
    GURL tag_url2 = test_server()->GetURL(
        "files/extensions/platform_apps/web_view_isolation/cookie2.html");
    tag_url2 = tag_url2.ReplaceComponents(replace_host);
    GURL tag_url3 = test_server()->GetURL(
        "files/extensions/platform_apps/web_view_isolation/storage1.html");
    tag_url3 = tag_url3.ReplaceComponents(replace_host);
    GURL tag_url4 = test_server()->GetURL(
        "files/extensions/platform_apps/web_view_isolation/storage2.html");
    tag_url4 = tag_url4.ReplaceComponents(replace_host);

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), navigate_to_url, CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

    ui_test_utils::UrlLoadObserver observer1(
        tag_url1, content::NotificationService::AllSources());
    ui_test_utils::UrlLoadObserver observer2(
        tag_url2, content::NotificationService::AllSources());
    ui_test_utils::UrlLoadObserver observer3(
        tag_url3, content::NotificationService::AllSources());
    ui_test_utils::UrlLoadObserver observer4(
        tag_url4, content::NotificationService::AllSources());
    LoadAndLaunchPlatformApp("web_view_isolation");
    observer1.Wait();
    observer2.Wait();
    observer3.Wait();
    observer4.Wait();

    content::Source<content::NavigationController> source1 = observer1.source();
    EXPECT_TRUE(source1->GetWebContents()->GetRenderProcessHost()->IsGuest());
    content::Source<content::NavigationController> source2 = observer2.source();
    EXPECT_TRUE(source2->GetWebContents()->GetRenderProcessHost()->IsGuest());
    content::Source<content::NavigationController> source3 = observer3.source();
    EXPECT_TRUE(source3->GetWebContents()->GetRenderProcessHost()->IsGuest());
    content::Source<content::NavigationController> source4 = observer4.source();
    EXPECT_TRUE(source4->GetWebContents()->GetRenderProcessHost()->IsGuest());

    // Tags with the same storage partition are not yet combined in the same
    // process. Check this until http://crbug.com/138296 is fixed.
    EXPECT_NE(source1->GetWebContents()->GetRenderProcessHost()->GetID(),
              source2->GetWebContents()->GetRenderProcessHost()->GetID());

    // Check that the storage partitions of the first two tags match and are
    // different than the other two.
    EXPECT_EQ(
        source1->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source2->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());
    EXPECT_EQ(
        source3->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source4->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());
    EXPECT_NE(
        source1->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source3->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());

    *default_tag_contents1 = source1->GetWebContents();
    *default_tag_contents2 = source2->GetWebContents();
    *named_partition_contents1 = source3->GetWebContents();
    *named_partition_contents2 = source4->GetWebContents();
  }

  void ExecuteScriptWaitForTitle(content::WebContents* web_contents,
                                 const char* script,
                                 const char* title) {
    std::wstring js_script = ASCIIToWide(script);
    string16 expected_title(ASCIIToUTF16(title));
    string16 error_title(ASCIIToUTF16("error"));

    content::TitleWatcher title_watcher(web_contents, expected_title);
    title_watcher.AlsoWaitForTitle(error_title);
    EXPECT_TRUE(content::ExecuteJavaScript(web_contents->GetRenderViewHost(),
                                           std::wstring(), js_script));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }
};

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ShimSrcAttribute) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view_src_attribute"))
      << message_;
}

// This tests cookie isolation for packaged apps with webview tags. It navigates
// the main browser window to a page that sets a cookie and loads an app with
// multiple webview tags. Each tag sets a cookie and the test checks the proper
// storage isolation is enforced.
IN_PROC_BROWSER_TEST_F(WebViewTest, CookieIsolation) {
  ASSERT_TRUE(StartTestServer());
  const std::wstring kExpire =
      L"var expire = new Date(Date.now() + 24 * 60 * 60 * 1000);";
  std::wstring cookie_script1(kExpire);
  cookie_script1.append(
      L"document.cookie = 'guest1=true; path=/; expires=' + expire + ';';");
  std::wstring cookie_script2(kExpire);
  cookie_script2.append(
      L"document.cookie = 'guest2=true; path=/; expires=' + expire + ';';");

  GURL::Replacements replace_host;
  std::string host_str("localhost");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);

  GURL set_cookie_url = test_server()->GetURL(
      "files/extensions/platform_apps/isolation/set_cookie.html");
  set_cookie_url = set_cookie_url.ReplaceComponents(replace_host);

  // The first two partitions will be used to set cookies and ensure they are
  // shared. The named partition is used to ensure that cookies are isolated
  // between partitions within the same app.
  content::WebContents* cookie_contents1;
  content::WebContents* cookie_contents2;
  content::WebContents* named_partition_contents1;
  content::WebContents* named_partition_contents2;

  NavigateAndOpenAppForIsolation(set_cookie_url, &cookie_contents1,
                                 &cookie_contents2, &named_partition_contents1,
                                 &named_partition_contents2);

  EXPECT_TRUE(content::ExecuteJavaScript(
      cookie_contents1->GetRenderViewHost(), std::wstring(), cookie_script1));
  EXPECT_TRUE(content::ExecuteJavaScript(
      cookie_contents2->GetRenderViewHost(), std::wstring(), cookie_script2));

  int cookie_size;
  std::string cookie_value;

  // Test the regular browser context to ensure we have only one cookie.
  automation_util::GetCookies(GURL("http://localhost"),
                              chrome::GetWebContentsAt(browser(), 0),
                              &cookie_size, &cookie_value);
  EXPECT_EQ("testCookie=1", cookie_value);

  // The default behavior is to combine webview tags with no explicit partition
  // declaration into the same in-memory partition. Test the webview tags to
  // ensure we have properly set the cookies and we have both cookies in both
  // tags.
  automation_util::GetCookies(GURL("http://localhost"),
                              cookie_contents1,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("guest1=true; guest2=true", cookie_value);

  automation_util::GetCookies(GURL("http://localhost"),
                              cookie_contents2,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("guest1=true; guest2=true", cookie_value);

  // The third tag should not have any cookies as it is in a separate partition.
  automation_util::GetCookies(GURL("http://localhost"),
                              named_partition_contents1,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("", cookie_value);
}

// This tests DOM storage isolation for packaged apps with webview tags. It
// loads an app with multiple webview tags and each tag sets DOM storage
// entries, which the test checks to ensure proper storage isolation is
// enforced.
IN_PROC_BROWSER_TEST_F(WebViewTest, DOMStorageIsolation) {
  ASSERT_TRUE(StartTestServer());
  GURL regular_url = test_server()->GetURL("files/title1.html");

  std::string output;
  std::wstring get_local_storage(L"window.domAutomationController.send("
      L"window.localStorage.getItem('foo') || 'badval')");
  std::wstring get_session_storage(L"window.domAutomationController.send("
      L"window.sessionStorage.getItem('bar') || 'badval')");

  content::WebContents* default_tag_contents1;
  content::WebContents* default_tag_contents2;
  content::WebContents* storage_contents1;
  content::WebContents* storage_contents2;

  NavigateAndOpenAppForIsolation(regular_url, &default_tag_contents1,
                                 &default_tag_contents2, &storage_contents1,
                                 &storage_contents2);

  // Initialize the storage for the first of the two tags that share a storage
  // partition.
  EXPECT_TRUE(content::ExecuteJavaScript(
      storage_contents1->GetRenderViewHost(), std::wstring(),
      L"initDomStorage('page1')"));

  // Let's test that the expected values are present in the first tag, as they
  // will be overwritten once we call the initDomStorage on the second tag.
  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      storage_contents1->GetRenderViewHost(), std::wstring(),
      get_local_storage.c_str(), &output));
  EXPECT_STREQ("local-page1", output.c_str());
  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      storage_contents1->GetRenderViewHost(), std::wstring(),
      get_session_storage.c_str(), &output));
  EXPECT_STREQ("session-page1", output.c_str());

  // Now, init the storage in the second tag in the same storage partition,
  // which will overwrite the shared localStorage.
  EXPECT_TRUE(content::ExecuteJavaScript(
      storage_contents2->GetRenderViewHost(), std::wstring(),
      L"initDomStorage('page2')"));

  // The localStorage value now should reflect the one written through the
  // second tag.
  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      storage_contents1->GetRenderViewHost(), std::wstring(),
      get_local_storage.c_str(), &output));
  EXPECT_STREQ("local-page2", output.c_str());
  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      storage_contents2->GetRenderViewHost(), std::wstring(),
      get_local_storage.c_str(), &output));
  EXPECT_STREQ("local-page2", output.c_str());

  // Session storage is not shared though, as each webview tag has separate
  // instance, even if they are in the same storage partition.
  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      storage_contents1->GetRenderViewHost(), std::wstring(),
      get_session_storage.c_str(), &output));
  EXPECT_STREQ("session-page1", output.c_str());
  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      storage_contents2->GetRenderViewHost(), std::wstring(),
      get_session_storage.c_str(), &output));
  EXPECT_STREQ("session-page2", output.c_str());

  // Also, let's check that the main browser and another tag that doesn't share
  // the same partition don't have those values stored.
  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      chrome::GetWebContentsAt(browser(), 0)->GetRenderViewHost(),
      std::wstring(), get_local_storage.c_str(), &output));
  EXPECT_STREQ("badval", output.c_str());
  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      chrome::GetWebContentsAt(browser(), 0)->GetRenderViewHost(),
      std::wstring(), get_session_storage.c_str(), &output));
  EXPECT_STREQ("badval", output.c_str());
  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      default_tag_contents1->GetRenderViewHost(), std::wstring(),
      get_local_storage.c_str(), &output));
  EXPECT_STREQ("badval", output.c_str());
  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      default_tag_contents1->GetRenderViewHost(), std::wstring(),
      get_session_storage.c_str(), &output));
  EXPECT_STREQ("badval", output.c_str());
}

// This tests IndexedDB isolation for packaged apps with webview tags. It loads
// an app with multiple webview tags and each tag creates an IndexedDB record,
// which the test checks to ensure proper storage isolation is enforced.
IN_PROC_BROWSER_TEST_F(WebViewTest, IndexedDBIsolation) {
  ASSERT_TRUE(StartTestServer());
  GURL regular_url = test_server()->GetURL("files/title1.html");

  content::WebContents* default_tag_contents1;
  content::WebContents* default_tag_contents2;
  content::WebContents* storage_contents1;
  content::WebContents* storage_contents2;

  NavigateAndOpenAppForIsolation(regular_url, &default_tag_contents1,
                                 &default_tag_contents2, &storage_contents1,
                                 &storage_contents2);

  // Initialize the storage for the first of the two tags that share a storage
  // partition.
  ExecuteScriptWaitForTitle(storage_contents1, "initIDB()", "idb created");
  ExecuteScriptWaitForTitle(storage_contents1, "addItemIDB(7, 'page1')",
                            "addItemIDB complete");
  ExecuteScriptWaitForTitle(storage_contents1, "readItemIDB(7)",
                            "readItemIDB complete");

  std::string output;
  std::wstring get_value(
      L"window.domAutomationController.send(getValueIDB() || 'badval')");

  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      storage_contents1->GetRenderViewHost(), std::wstring(),
      get_value.c_str(), &output));
  EXPECT_STREQ("page1", output.c_str());

  // Initialize the db in the second tag.
  ExecuteScriptWaitForTitle(storage_contents2, "initIDB()", "idb open");

  // Since we share a partition, reading the value should return the existing
  // one.
  ExecuteScriptWaitForTitle(storage_contents2, "readItemIDB(7)",
                            "readItemIDB complete");
  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      storage_contents2->GetRenderViewHost(), std::wstring(),
      get_value.c_str(), &output));
  EXPECT_STREQ("page1", output.c_str());

  // Now write through the second tag and read it back.
  ExecuteScriptWaitForTitle(storage_contents2, "addItemIDB(7, 'page2')",
                            "addItemIDB complete");
  ExecuteScriptWaitForTitle(storage_contents2, "readItemIDB(7)",
                            "readItemIDB complete");
  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      storage_contents2->GetRenderViewHost(), std::wstring(),
      get_value.c_str(), &output));
  EXPECT_STREQ("page2", output.c_str());

  // Reset the document title, otherwise the next call will not see a change and
  // will hang waiting for it.
  EXPECT_TRUE(content::ExecuteJavaScript(
      storage_contents1->GetRenderViewHost(), std::wstring(),
      L"document.title = 'foo'"));

  // Read through the first tag to ensure we have the second value.
  ExecuteScriptWaitForTitle(storage_contents1, "readItemIDB(7)",
                            "readItemIDB complete");
  EXPECT_TRUE(ExecuteJavaScriptAndExtractString(
      storage_contents1->GetRenderViewHost(), std::wstring(),
      get_value.c_str(), &output));
  EXPECT_STREQ("page2", output.c_str());

  // Now, let's confirm there is no database in the main browser and another
  // tag that doesn't share the same partition. Due to the IndexedDB API design,
  // open will succeed, but the version will be 1, since it creates the database
  // if it is not found. The two tags use database version 3, so we avoid
  // ambiguity.
  const char* script =
    "indexedDB.open('isolation').onsuccess = function(e) {"
    "  if (e.target.result.version == 1)"
    "    document.title = 'db not found';"
    "  else "
    "    document.title = 'error';"
    "}";
  ExecuteScriptWaitForTitle(chrome::GetWebContentsAt(browser(), 0),
                            script, "db not found");
  ExecuteScriptWaitForTitle(default_tag_contents1, script, "db not found");
}
