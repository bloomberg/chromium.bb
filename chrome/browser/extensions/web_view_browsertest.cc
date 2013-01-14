// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
  }

  // This method is responsible for initializing a packaged app, which contains
  // multiple webview tags. The tags have different partition identifiers and
  // their WebContent objects are returned as output. The method also verifies
  // the expected process allocation and storage partition assignment.
  // The |navigate_to_url| parameter is used to navigate the main browser
  // window.
  //
  // TODO(ajwong): This function is getting to be too large. Either refactor it
  // so the test can specify a configuration of WebView tags that we will
  // dynamically inject JS to generate, or move this test wholesale into
  // something that RunPlatformAppTest() can execute purely in Javascript. This
  // won't let us do a white-box examination of the StoragePartition equivalence
  // directly, but we will be able to view the black box effects which is good
  // enough.  http://crbug.com/160361
  void NavigateAndOpenAppForIsolation(
      GURL navigate_to_url,
      content::WebContents** default_tag_contents1,
      content::WebContents** default_tag_contents2,
      content::WebContents** named_partition_contents1,
      content::WebContents** named_partition_contents2,
      content::WebContents** persistent_partition_contents1,
      content::WebContents** persistent_partition_contents2,
      content::WebContents** persistent_partition_contents3) {
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
    GURL tag_url5 = test_server()->GetURL(
        "files/extensions/platform_apps/web_view_isolation/storage1.html#p1");
    tag_url5 = tag_url5.ReplaceComponents(replace_host);
    GURL tag_url6 = test_server()->GetURL(
        "files/extensions/platform_apps/web_view_isolation/storage1.html#p2");
    tag_url6 = tag_url6.ReplaceComponents(replace_host);
    GURL tag_url7 = test_server()->GetURL(
        "files/extensions/platform_apps/web_view_isolation/storage1.html#p3");
    tag_url7 = tag_url7.ReplaceComponents(replace_host);

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
    ui_test_utils::UrlLoadObserver observer5(
        tag_url5, content::NotificationService::AllSources());
    ui_test_utils::UrlLoadObserver observer6(
        tag_url6, content::NotificationService::AllSources());
    ui_test_utils::UrlLoadObserver observer7(
        tag_url7, content::NotificationService::AllSources());
    LoadAndLaunchPlatformApp("web_view_isolation");
    observer1.Wait();
    observer2.Wait();
    observer3.Wait();
    observer4.Wait();
    observer5.Wait();
    observer6.Wait();
    observer7.Wait();

    content::Source<content::NavigationController> source1 = observer1.source();
    EXPECT_TRUE(source1->GetWebContents()->GetRenderProcessHost()->IsGuest());
    content::Source<content::NavigationController> source2 = observer2.source();
    EXPECT_TRUE(source2->GetWebContents()->GetRenderProcessHost()->IsGuest());
    content::Source<content::NavigationController> source3 = observer3.source();
    EXPECT_TRUE(source3->GetWebContents()->GetRenderProcessHost()->IsGuest());
    content::Source<content::NavigationController> source4 = observer4.source();
    EXPECT_TRUE(source4->GetWebContents()->GetRenderProcessHost()->IsGuest());
    content::Source<content::NavigationController> source5 = observer5.source();
    EXPECT_TRUE(source5->GetWebContents()->GetRenderProcessHost()->IsGuest());
    content::Source<content::NavigationController> source6 = observer6.source();
    EXPECT_TRUE(source6->GetWebContents()->GetRenderProcessHost()->IsGuest());
    content::Source<content::NavigationController> source7 = observer7.source();
    EXPECT_TRUE(source7->GetWebContents()->GetRenderProcessHost()->IsGuest());

    // Check that the first two tags use the same process and it is different
    // than the process used by the other two.
    EXPECT_EQ(source1->GetWebContents()->GetRenderProcessHost()->GetID(),
              source2->GetWebContents()->GetRenderProcessHost()->GetID());
    EXPECT_EQ(source3->GetWebContents()->GetRenderProcessHost()->GetID(),
              source4->GetWebContents()->GetRenderProcessHost()->GetID());
    EXPECT_NE(source1->GetWebContents()->GetRenderProcessHost()->GetID(),
              source3->GetWebContents()->GetRenderProcessHost()->GetID());

    // The two sets of tags should also be isolated from the main browser.
    EXPECT_NE(source1->GetWebContents()->GetRenderProcessHost()->GetID(),
              browser()->tab_strip_model()->GetWebContentsAt(0)->
                  GetRenderProcessHost()->GetID());
    EXPECT_NE(source3->GetWebContents()->GetRenderProcessHost()->GetID(),
              browser()->tab_strip_model()->GetWebContentsAt(0)->
                  GetRenderProcessHost()->GetID());

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

    // Ensure the persistent storage partitions are different.
    EXPECT_EQ(
        source5->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source6->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());
    EXPECT_NE(
        source5->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source7->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());
    EXPECT_NE(
        source1->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source5->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());
    EXPECT_NE(
        source1->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source7->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());

    *default_tag_contents1 = source1->GetWebContents();
    *default_tag_contents2 = source2->GetWebContents();
    *named_partition_contents1 = source3->GetWebContents();
    *named_partition_contents2 = source4->GetWebContents();
    if (persistent_partition_contents1) {
      *persistent_partition_contents1 = source5->GetWebContents();
    }
    if (persistent_partition_contents2) {
      *persistent_partition_contents2 = source6->GetWebContents();
    }
    if (persistent_partition_contents3) {
      *persistent_partition_contents3 = source7->GetWebContents();
    }
  }

  void ExecuteScriptWaitForTitle(content::WebContents* web_contents,
                                 const char* script,
                                 const char* title) {
    string16 expected_title(ASCIIToUTF16(title));
    string16 error_title(ASCIIToUTF16("error"));

    content::TitleWatcher title_watcher(web_contents, expected_title);
    title_watcher.AlsoWaitForTitle(error_title);
    EXPECT_TRUE(content::ExecuteScript(web_contents, script));
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
  const std::string kExpire =
      "var expire = new Date(Date.now() + 24 * 60 * 60 * 1000);";
  std::string cookie_script1(kExpire);
  cookie_script1.append(
      "document.cookie = 'guest1=true; path=/; expires=' + expire + ';';");
  std::string cookie_script2(kExpire);
  cookie_script2.append(
      "document.cookie = 'guest2=true; path=/; expires=' + expire + ';';");

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
                                 &named_partition_contents2, NULL, NULL, NULL);

  EXPECT_TRUE(content::ExecuteScript(cookie_contents1, cookie_script1));
  EXPECT_TRUE(content::ExecuteScript(cookie_contents2, cookie_script2));

  int cookie_size;
  std::string cookie_value;

  // Test the regular browser context to ensure we have only one cookie.
  automation_util::GetCookies(GURL("http://localhost"),
                              browser()->tab_strip_model()->GetWebContentsAt(0),
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

// This tests that in-memory storage partitions are reset on browser restart,
// but persistent ones maintain state for cookies and HTML5 storage.
IN_PROC_BROWSER_TEST_F(WebViewTest, PRE_StoragePersistence) {
  ASSERT_TRUE(StartTestServer());
  const std::string kExpire =
      "var expire = new Date(Date.now() + 24 * 60 * 60 * 1000);";
  std::string cookie_script1(kExpire);
  cookie_script1.append(
      "document.cookie = 'inmemory=true; path=/; expires=' + expire + ';';");
  std::string cookie_script2(kExpire);
  cookie_script2.append(
      "document.cookie = 'persist1=true; path=/; expires=' + expire + ';';");
  std::string cookie_script3(kExpire);
  cookie_script3.append(
      "document.cookie = 'persist2=true; path=/; expires=' + expire + ';';");

  // We don't care where the main browser is on this test.
  GURL blank_url("about:blank");

  // The first two partitions will be used to set cookies and ensure they are
  // shared. The named partition is used to ensure that cookies are isolated
  // between partitions within the same app.
  content::WebContents* cookie_contents1;
  content::WebContents* cookie_contents2;
  content::WebContents* named_partition_contents1;
  content::WebContents* named_partition_contents2;
  content::WebContents* persistent_partition_contents1;
  content::WebContents* persistent_partition_contents2;
  content::WebContents* persistent_partition_contents3;
  NavigateAndOpenAppForIsolation(blank_url, &cookie_contents1,
                                 &cookie_contents2, &named_partition_contents1,
                                 &named_partition_contents2,
                                 &persistent_partition_contents1,
                                 &persistent_partition_contents2,
                                 &persistent_partition_contents3);

  // Set the inmemory=true cookie for tags with inmemory partitions.
  EXPECT_TRUE(content::ExecuteScript(cookie_contents1, cookie_script1));
  EXPECT_TRUE(content::ExecuteScript(named_partition_contents1,
                                     cookie_script1));

  // For the two different persistent storage partitions, set the
  // two different cookies so we can check that they aren't comingled below.
  EXPECT_TRUE(content::ExecuteScript(persistent_partition_contents1,
                                     cookie_script2));

  EXPECT_TRUE(content::ExecuteScript(persistent_partition_contents3,
                                     cookie_script3));

  int cookie_size;
  std::string cookie_value;

  // Check that all in-memory partitions have a cookie set.
  automation_util::GetCookies(GURL("http://localhost"),
                              cookie_contents1,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("inmemory=true", cookie_value);
  automation_util::GetCookies(GURL("http://localhost"),
                              cookie_contents2,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("inmemory=true", cookie_value);
  automation_util::GetCookies(GURL("http://localhost"),
                              named_partition_contents1,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("inmemory=true", cookie_value);
  automation_util::GetCookies(GURL("http://localhost"),
                              named_partition_contents2,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("inmemory=true", cookie_value);

  // Check that all persistent partitions kept their state.
  automation_util::GetCookies(GURL("http://localhost"),
                              persistent_partition_contents1,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("persist1=true", cookie_value);
  automation_util::GetCookies(GURL("http://localhost"),
                              persistent_partition_contents2,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("persist1=true", cookie_value);
  automation_util::GetCookies(GURL("http://localhost"),
                              persistent_partition_contents3,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("persist2=true", cookie_value);
}

// This is the post-reset portion of the StoragePersistence test.  See
// PRE_StoragePersistence for main comment.
IN_PROC_BROWSER_TEST_F(WebViewTest, StoragePersistence) {
  ASSERT_TRUE(StartTestServer());

  // We don't care where the main browser is on this test.
  GURL blank_url("about:blank");

  // The first two partitions will be used to set cookies and ensure they are
  // shared. The named partition is used to ensure that cookies are isolated
  // between partitions within the same app.
  content::WebContents* cookie_contents1;
  content::WebContents* cookie_contents2;
  content::WebContents* named_partition_contents1;
  content::WebContents* named_partition_contents2;
  content::WebContents* persistent_partition_contents1;
  content::WebContents* persistent_partition_contents2;
  content::WebContents* persistent_partition_contents3;
  NavigateAndOpenAppForIsolation(blank_url, &cookie_contents1,
                                 &cookie_contents2, &named_partition_contents1,
                                 &named_partition_contents2,
                                 &persistent_partition_contents1,
                                 &persistent_partition_contents2,
                                 &persistent_partition_contents3);

  int cookie_size;
  std::string cookie_value;

  // Check that all in-memory partitions lost their state.
  automation_util::GetCookies(GURL("http://localhost"),
                              cookie_contents1,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("", cookie_value);
  automation_util::GetCookies(GURL("http://localhost"),
                              cookie_contents2,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("", cookie_value);
  automation_util::GetCookies(GURL("http://localhost"),
                              named_partition_contents1,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("", cookie_value);
  automation_util::GetCookies(GURL("http://localhost"),
                              named_partition_contents2,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("", cookie_value);

  // Check that all persistent partitions kept their state.
  automation_util::GetCookies(GURL("http://localhost"),
                              persistent_partition_contents1,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("persist1=true", cookie_value);
  automation_util::GetCookies(GURL("http://localhost"),
                              persistent_partition_contents2,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("persist1=true", cookie_value);
  automation_util::GetCookies(GURL("http://localhost"),
                              persistent_partition_contents3,
                              &cookie_size, &cookie_value);
  EXPECT_EQ("persist2=true", cookie_value);
}

// This tests DOM storage isolation for packaged apps with webview tags. It
// loads an app with multiple webview tags and each tag sets DOM storage
// entries, which the test checks to ensure proper storage isolation is
// enforced.
IN_PROC_BROWSER_TEST_F(WebViewTest, DOMStorageIsolation) {
  ASSERT_TRUE(StartTestServer());
  GURL regular_url = test_server()->GetURL("files/title1.html");

  std::string output;
  std::string get_local_storage("window.domAutomationController.send("
      "window.localStorage.getItem('foo') || 'badval')");
  std::string get_session_storage("window.domAutomationController.send("
      "window.sessionStorage.getItem('bar') || 'badval')");

  content::WebContents* default_tag_contents1;
  content::WebContents* default_tag_contents2;
  content::WebContents* storage_contents1;
  content::WebContents* storage_contents2;

  NavigateAndOpenAppForIsolation(regular_url, &default_tag_contents1,
                                 &default_tag_contents2, &storage_contents1,
                                 &storage_contents2, NULL, NULL, NULL);

  // Initialize the storage for the first of the two tags that share a storage
  // partition.
  EXPECT_TRUE(content::ExecuteScript(storage_contents1,
                                     "initDomStorage('page1')"));

  // Let's test that the expected values are present in the first tag, as they
  // will be overwritten once we call the initDomStorage on the second tag.
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents1,
                                            get_local_storage.c_str(),
                                            &output));
  EXPECT_STREQ("local-page1", output.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents1,
                                            get_session_storage.c_str(),
                                            &output));
  EXPECT_STREQ("session-page1", output.c_str());

  // Now, init the storage in the second tag in the same storage partition,
  // which will overwrite the shared localStorage.
  EXPECT_TRUE(content::ExecuteScript(storage_contents2,
                                     "initDomStorage('page2')"));

  // The localStorage value now should reflect the one written through the
  // second tag.
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents1,
                                            get_local_storage.c_str(),
                                            &output));
  EXPECT_STREQ("local-page2", output.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents2,
                                            get_local_storage.c_str(),
                                            &output));
  EXPECT_STREQ("local-page2", output.c_str());

  // Session storage is not shared though, as each webview tag has separate
  // instance, even if they are in the same storage partition.
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents1,
                                            get_session_storage.c_str(),
                                            &output));
  EXPECT_STREQ("session-page1", output.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents2,
                                            get_session_storage.c_str(),
                                            &output));
  EXPECT_STREQ("session-page2", output.c_str());

  // Also, let's check that the main browser and another tag that doesn't share
  // the same partition don't have those values stored.
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      get_local_storage.c_str(),
      &output));
  EXPECT_STREQ("badval", output.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      get_session_storage.c_str(),
      &output));
  EXPECT_STREQ("badval", output.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractString(default_tag_contents1,
                                            get_local_storage.c_str(),
                                            &output));
  EXPECT_STREQ("badval", output.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractString(default_tag_contents1,
                                            get_session_storage.c_str(),
                                            &output));
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
                                 &storage_contents2, NULL, NULL, NULL);

  // Initialize the storage for the first of the two tags that share a storage
  // partition.
  ExecuteScriptWaitForTitle(storage_contents1, "initIDB()", "idb created");
  ExecuteScriptWaitForTitle(storage_contents1, "addItemIDB(7, 'page1')",
                            "addItemIDB complete");
  ExecuteScriptWaitForTitle(storage_contents1, "readItemIDB(7)",
                            "readItemIDB complete");

  std::string output;
  std::string get_value(
      "window.domAutomationController.send(getValueIDB() || 'badval')");

  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents1,
                                            get_value.c_str(), &output));
  EXPECT_STREQ("page1", output.c_str());

  // Initialize the db in the second tag.
  ExecuteScriptWaitForTitle(storage_contents2, "initIDB()", "idb open");

  // Since we share a partition, reading the value should return the existing
  // one.
  ExecuteScriptWaitForTitle(storage_contents2, "readItemIDB(7)",
                            "readItemIDB complete");
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents2,
                                            get_value.c_str(), &output));
  EXPECT_STREQ("page1", output.c_str());

  // Now write through the second tag and read it back.
  ExecuteScriptWaitForTitle(storage_contents2, "addItemIDB(7, 'page2')",
                            "addItemIDB complete");
  ExecuteScriptWaitForTitle(storage_contents2, "readItemIDB(7)",
                            "readItemIDB complete");
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents2,
                                            get_value.c_str(), &output));
  EXPECT_STREQ("page2", output.c_str());

  // Reset the document title, otherwise the next call will not see a change and
  // will hang waiting for it.
  EXPECT_TRUE(content::ExecuteScript(storage_contents1,
                                     "document.title = 'foo'"));

  // Read through the first tag to ensure we have the second value.
  ExecuteScriptWaitForTitle(storage_contents1, "readItemIDB(7)",
                            "readItemIDB complete");
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents1,
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
  ExecuteScriptWaitForTitle(browser()->tab_strip_model()->GetWebContentsAt(0),
                            script, "db not found");
  ExecuteScriptWaitForTitle(default_tag_contents1, script, "db not found");
}
