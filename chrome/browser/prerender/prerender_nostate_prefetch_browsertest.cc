// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using prerender::test_utils::CreateCountingInterceptorOnIO;
using prerender::test_utils::DestructionWaiter;
using prerender::test_utils::RequestCounter;
using prerender::test_utils::TestPrerender;
using prerender::test_utils::TestPrerenderContents;
using task_manager::browsertest_util::WaitForTaskManagerRows;

namespace {
// Fetches a boolean value from javascript. Returns whether the fetch
// succeeded; the value of the variable is returned in value. If
// javascript_variable does not exist, this returns false and value is
// unchanged. The function checks that script execution works.
bool GetJavascriptBoolean(const std::string& javascript_variable,
                          content::WebContents* web_contents,
                          bool* value) {
  // In order to detect unknown variables a three-valued return is needed.
  int result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents,
      "try { if (" + javascript_variable + ") { " +
          "window.domAutomationController.send(1) } else { " +
          "window.domAutomationController.send(0); } } catch(err) {" +
          "window.domAutomationController.send(2) }",
      &result));
  if (result == 2) {
    // This means an exception was caught, usually because of a missing
    // variable.
    return false;
  }
  *value = (result == 1);
  return true;
}

// As above, but just checks for a missing variable.
bool JavascriptVariableMissing(const std::string& javascript_variable,
                               content::WebContents* web_contents) {
  bool unused;
  return !GetJavascriptBoolean(javascript_variable, web_contents, &unused);
}

}  // namespace

namespace prerender {

// These URLs used for test resources must be relative with the exception of
// |PrefetchLoaderPath|, which is only used in |PrerenderTestURLImpl()|.
const char kPrefetchImagePage[] = "prerender/prefetch_image.html";
const char kPrefetchJpeg[] = "prerender/image.jpeg";
const char kPrefetchLoaderPath[] = "/prerender/prefetch_loader.html";
const char kPrefetchLoopPage[] = "prerender/prefetch_loop.html";
const char kPrefetchMetaCSP[] = "prerender/prefetch_meta_csp.html";
const char kPrefetchPage[] = "prerender/prefetch_page.html";
const char kPrefetchPage2[] = "prerender/prefetch_page2.html";
const char kPrefetchPng[] = "prerender/image.png";
const char kPrefetchResponseHeaderCSP[] =
    "prerender/prefetch_response_csp.html";
const char kPrefetchScript[] = "prerender/prefetch.js";
const char kPrefetchScript2[] = "prerender/prefetch2.js";
const char kPrefetchSubresourceRedirectPage[] =
    "prerender/prefetch_subresource_redirect.html";

const char kPageBool[] = "pageBool";
const char kScriptBool[] = "scriptBool";

class NoStatePrefetchBrowserTest
    : public test_utils::PrerenderInProcessBrowserTest {
 public:
  class BrowserTestTime : public PrerenderManager::TimeOverride {
   public:
    BrowserTestTime() {}

    base::Time GetCurrentTime() const override {
      if (delta_.is_zero()) {
        return base::Time::Now();
      }
      return time_ + delta_;
    }

    base::TimeTicks GetCurrentTimeTicks() const override {
      if (delta_.is_zero()) {
        return base::TimeTicks::Now();
      }
      return time_ticks_ + delta_;
    }

    void AdvanceTime(base::TimeDelta delta) {
      if (delta_.is_zero()) {
        time_ = base::Time::Now();
        time_ticks_ = base::TimeTicks::Now();
        delta_ = delta;
      } else {
        delta_ += delta;
      }
    }

   private:
    base::Time time_;
    base::TimeTicks time_ticks_;
    base::TimeDelta delta_;
  };

  NoStatePrefetchBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PrerenderInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kPrerenderMode, switches::kPrerenderModeSwitchValuePrefetch);
  }

  void SetUpOnMainThread() override {
    PrerenderInProcessBrowserTest::SetUpOnMainThread();
    std::unique_ptr<BrowserTestTime> test_time =
        base::MakeUnique<BrowserTestTime>();
    browser_test_time_ = test_time.get();
    GetPrerenderManager()->SetTimeOverride(std::move(test_time));
  }

  // Set up a request counter for |path|, which is also the location of the data
  // served by the request.
  void CountRequestFor(const std::string& path_str, RequestCounter* counter) {
    url::StringPieceReplacements<base::FilePath::StringType> replacement;
    base::FilePath file_path = base::FilePath::FromUTF8Unsafe(path_str);
    replacement.SetPathStr(file_path.value());
    const GURL url = src_server()->base_url().ReplaceComponents(replacement);
    CountRequestForUrl(url, path_str, counter);
  }

  // As above, but specify the data path and URL separately.
  void CountRequestForUrl(const GURL& url,
                          const std::string& path_str,
                          RequestCounter* counter) {
    base::FilePath url_file = ui_test_utils::GetTestFilePath(
        base::FilePath(), base::FilePath::FromUTF8Unsafe(path_str));
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&CreateCountingInterceptorOnIO, url, url_file,
                   counter->AsWeakPtr()));
  }

  BrowserTestTime* GetTimeOverride() const { return browser_test_time_; }

 private:
  ScopedVector<TestPrerender> PrerenderTestURLImpl(
      const GURL& prerender_url,
      const std::vector<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads) override {
    base::StringPairs replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_PREFETCH_URL", prerender_url.spec()));
    std::string replacement_path;
    net::test_server::GetFilePathWithReplacements(
        kPrefetchLoaderPath, replacement_text, &replacement_path);
    GURL loader_url = src_server()->GetURL(replacement_path);

    ScopedVector<TestPrerender> prerenders = NavigateWithPrerenders(
        loader_url, expected_final_status_queue, expected_number_of_loads);

    TestPrerenderContents* prerender_contents = prerenders[0]->contents();
    if (expected_number_of_loads > 0) {
      CHECK(prerender_contents);
      // Checks that the prerender contents final status is unchanged from its
      // default value, meaning that the contents has not been destroyed.
      EXPECT_EQ(FINAL_STATUS_MAX, prerender_contents->final_status());
    }
    EXPECT_EQ(expected_number_of_loads, prerenders[0]->number_of_loads());

    return prerenders;
  }

  BrowserTestTime* browser_test_time_;
};

// Performs a full load of the target page and check that javascript values are
// set as expected. This confirms that our test system is working correctly, so
// that when the target page is prefetched it can be confirmed that javascript
// is not executed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, CheckJavascript) {
  ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL(MakeAbsolute(kPrefetchPage)));
  content::WebContents* web_contents =
      current_browser()->tab_strip_model()->GetActiveWebContents();

  // Confirms that true and false values can appear.
  bool value = false;
  EXPECT_TRUE(GetJavascriptBoolean(kPageBool, web_contents, &value));
  EXPECT_TRUE(value);
  value = true;
  EXPECT_TRUE(GetJavascriptBoolean("pageAntiBool", web_contents, &value));
  EXPECT_FALSE(value);

  // Confirm a value from the script is plumbed through.
  value = false;
  EXPECT_TRUE(GetJavascriptBoolean(kScriptBool, web_contents, &value));
  EXPECT_TRUE(value);

  // Confirm that the expected happens when a value doesn't exist.
  EXPECT_TRUE(JavascriptVariableMissing("iDontExist", web_contents));
}

// Checks that a page is correctly prefetched in the case of a
// <link rel=prerender> tag and then loaded into a tab in response to a
// navigation, when NoState Prefetch is enabled, but that the page is not loaded
// (which confirmed by checking that javascript is not executed).
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchSimple) {
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);
  RequestCounter main_counter;
  CountRequestFor(kPrefetchPage, &main_counter);

  std::unique_ptr<TestPrerender> test_prerender =
      PrerenderTestURL(kPrefetchPage, FINAL_STATUS_APP_TERMINATING, 1);
  main_counter.WaitForCount(1);
  script_counter.WaitForCount(1);

  content::WebContents* contents =
      test_prerender->contents()->prerender_contents();
  content::WebContents* active_contents =
      current_browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(JavascriptVariableMissing(kPageBool, contents));
  EXPECT_TRUE(JavascriptVariableMissing(kScriptBool, contents));
  EXPECT_TRUE(JavascriptVariableMissing(kPageBool, active_contents));
  EXPECT_TRUE(JavascriptVariableMissing(kScriptBool, active_contents));
}

// Checks the prefetch of an img tag.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchImage) {
  RequestCounter image_counter;
  CountRequestFor(kPrefetchJpeg, &image_counter);
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", MakeAbsolute(kPrefetchJpeg)));
  std::string main_page_path;
  net::test_server::GetFilePathWithReplacements(
      kPrefetchImagePage, replacement_text, &main_page_path);
  // Note CountRequestFor cannot be used on the main page as the test server
  // must handling the image url replacement.
  PrerenderTestURL(main_page_path, FINAL_STATUS_APP_TERMINATING, 1);
  image_counter.WaitForCount(1);
}

// Checks that a cross-domain prefetching works correctly.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchCrossDomain) {
  static const std::string secondary_domain = "www.foo.com";
  host_resolver()->AddRule(secondary_domain, "127.0.0.1");
  GURL cross_domain_url(base::StringPrintf(
      "http://%s:%d/%s", secondary_domain.c_str(),
      embedded_test_server()->host_port_pair().port(), kPrefetchPage));
  RequestCounter cross_domain_counter;
  CountRequestForUrl(cross_domain_url, kPrefetchPage, &cross_domain_counter);
  PrerenderTestURL(cross_domain_url, FINAL_STATUS_APP_TERMINATING, 1);
  cross_domain_counter.WaitForCount(1);
}

// Checks that response header CSP is respected.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, ResponseHeaderCSP) {
  static const std::string secondary_domain = "foo.bar";
  host_resolver()->AddRule(secondary_domain, "127.0.0.1");
  RequestCounter main_page;
  CountRequestFor(kPrefetchResponseHeaderCSP, &main_page);
  RequestCounter first_script;
  CountRequestFor(kPrefetchScript, &first_script);
  RequestCounter second_script;
  GURL second_script_url(std::string("http://foo.bar/") + kPrefetchScript2);
  CountRequestForUrl(second_script_url, kPrefetchScript2, &second_script);
  PrerenderTestURL(kPrefetchResponseHeaderCSP, FINAL_STATUS_APP_TERMINATING, 1);
  // The second script is in the correct domain for CSP, but the first script is
  // not.
  main_page.WaitForCount(1);
  second_script.WaitForCount(1);
  // TODO(pasko): wait for prefetch to be finished before checking the counts.
  first_script.WaitForCount(0);
}

// Checks that CSP in the meta tag cancels the prefetch.
// TODO(mattcary): probably this behavior should be consistent with
// response-header CSP. See crbug/656581.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, MetaTagCSP) {
  static const std::string secondary_domain = "foo.bar";
  host_resolver()->AddRule(secondary_domain, "127.0.0.1");
  RequestCounter main_page;
  CountRequestFor(kPrefetchMetaCSP, &main_page);
  RequestCounter first_script;
  CountRequestFor(kPrefetchScript, &first_script);
  RequestCounter second_script;
  GURL second_script_url(std::string("http://foo.bar/") + kPrefetchScript2);
  CountRequestForUrl(second_script_url, kPrefetchScript2, &second_script);
  PrerenderTestURL(kPrefetchMetaCSP, FINAL_STATUS_APP_TERMINATING, 1);
  // TODO(mattcary): See test comment above. If the meta CSP tag were parsed,
  // |second_script| would be loaded. Instead as the background scanner bails as
  // soon as the meta CSP tag is seen, only |main_page| is fetched.
  main_page.WaitForCount(1);
  // TODO(pasko): wait for prefetch to be finished before checking the counts.
  second_script.WaitForCount(0);
  first_script.WaitForCount(0);
}

// Checks simultaneous prefetch.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchSimultaneous) {
  RequestCounter first_main_counter;
  CountRequestFor(kPrefetchPage, &first_main_counter);
  RequestCounter second_main_counter;
  CountRequestFor(kPrefetchPage2, &second_main_counter);
  RequestCounter first_script_counter;
  CountRequestFor(kPrefetchScript, &first_script_counter);
  RequestCounter second_script_counter;
  CountRequestFor(kPrefetchScript2, &second_script_counter);

  // The first prerender is marked as canceled as when the second starts, it
  // sees that the first has been abandoned (presumably because it is detached
  // immediately and so dies quickly).
  PrerenderTestURL(kPrefetchPage, FINAL_STATUS_CANCELLED, 1);
  PrerenderTestURL(kPrefetchPage2, FINAL_STATUS_APP_TERMINATING, 1);
  first_main_counter.WaitForCount(1);
  second_main_counter.WaitForCount(1);
  first_script_counter.WaitForCount(1);
  second_script_counter.WaitForCount(1);
}

// Checks a prefetch to a nonexisting page.
// TODO(mattcary): disabled as prefetch process teardown is racey with prerender
// contents destruction, can fix when prefetch prerenderers are destroyed
// deterministically.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       DISABLED_PrefetchNonexisting) {
  PrerenderTestURL("nonexisting-page.html", FINAL_STATUS_APP_TERMINATING, 0);
  // TODO(mattcary): we fire up a prerenderer before we discover that the main
  // page doesn't exist, we still count this as a prerender. Also we don't fail
  // the renderer (presumably because we've detached the resource, etc). Is this
  // what we want? At any rate, we can't positively check any of that now due to
  // histogram race conditions, and only test that we don't crash on a
  // nonexisting page.
}

// Checks that a 301 redirect is followed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Prefetch301Redirect) {
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);
  PrerenderTestURL(
      "/server-redirect/?" +
          net::EscapeQueryParamValue(MakeAbsolute(kPrefetchPage), false),
      FINAL_STATUS_APP_TERMINATING, 1);
  script_counter.WaitForCount(1);
}

// Checks that a subresource 301 redirect is followed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Prefetch301Subresource) {
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);
  PrerenderTestURL(kPrefetchSubresourceRedirectPage,
                   FINAL_STATUS_APP_TERMINATING, 1);
  script_counter.WaitForCount(1);
}

// Checks a client redirect is not followed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchClientRedirect) {
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);
  // A complete load of kPrefetchPage2 is used as a sentinal. Otherwise the test
  // ends before script_counter would reliably see the load of kPrefetchScript,
  // were it to happen.
  RequestCounter sentinel_counter;
  CountRequestFor(kPrefetchScript2, &sentinel_counter);
  PrerenderTestURL(
      "/client-redirect/?" +
          net::EscapeQueryParamValue(MakeAbsolute(kPrefetchPage), false),
      FINAL_STATUS_APP_TERMINATING, 1);
  ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL(MakeAbsolute(kPrefetchPage2)));
  sentinel_counter.WaitForCount(1);
  script_counter.WaitForCount(0);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchHttps) {
  UseHttpsSrcServer();
  RequestCounter main_counter;
  CountRequestFor(kPrefetchPage, &main_counter);
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);
  PrerenderTestURL(kPrefetchPage, FINAL_STATUS_APP_TERMINATING, 1);
  main_counter.WaitForCount(1);
  script_counter.WaitForCount(1);
}

// Checks that an SSL error prevents prefetch.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, SSLError) {
  // Only send the loaded page, not the loader, through SSL.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  std::unique_ptr<TestPrerender> prerender =
      PrerenderTestURL(https_server.GetURL(MakeAbsolute(kPrefetchPage)),
                       FINAL_STATUS_SSL_ERROR, 0);
  DestructionWaiter waiter(prerender->contents(), FINAL_STATUS_SSL_ERROR);
  EXPECT_TRUE(waiter.WaitForDestroy());
}

// Checks that a subresource failing SSL does not prevent prefetch on the rest
// of the page.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, SSLSubresourceError) {
  // First confirm that the image loads as expected.

  // A separate HTTPS server is started for the subresource; src_server() is
  // non-HTTPS.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("/prerender/image.jpeg");
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", https_url.spec()));
  std::string main_page_path;
  net::test_server::GetFilePathWithReplacements(
      kPrefetchImagePage, replacement_text, &main_page_path);
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);

  std::unique_ptr<TestPrerender> prerender =
      PrerenderTestURL(main_page_path, FINAL_STATUS_APP_TERMINATING, 1);
  // Checks that the presumed failure of the image load didn't affect the script
  // fetch. This assumes waiting for the script load is enough to see any error
  // from the image load.
  script_counter.WaitForCount(1);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Loop) {
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);
  RequestCounter main_counter;
  CountRequestFor(kPrefetchLoopPage, &main_counter);

  std::unique_ptr<TestPrerender> test_prerender =
      PrerenderTestURL(kPrefetchLoopPage, FINAL_STATUS_APP_TERMINATING, 1);
  main_counter.WaitForCount(1);
  script_counter.WaitForCount(1);
}

#if defined(ENABLE_TASK_MANAGER)

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       OpenTaskManagerBeforePrefetch) {
  const base::string16 any_prerender = MatchTaskManagerPrerender("*");
  const base::string16 any_tab = MatchTaskManagerTab("*");
  const base::string16 original = MatchTaskManagerTab("Prefetch Loader");
  // The page title is not visible in the task manager, presumably because the
  // page has not been fully parsed.
  const base::string16 prerender =
      MatchTaskManagerPrerender("*prefetch_page.html*");

  // Show the task manager. This populates the model.
  chrome::OpenTaskManager(current_browser());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, any_prerender));

  // Prerender a page in addition to the original tab.
  PrerenderTestURL(kPrefetchPage, FINAL_STATUS_APP_TERMINATING, 1);

  // A TaskManager entry should appear like "Prerender: Prerender Page"
  // alongside the original tab entry. There should be just these two entries.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));
}

#endif  // defined(ENABLE_TASK_MANAGER)

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, RendererCrash) {
  std::unique_ptr<TestPrerender> prerender =
      PrerenderTestURL(kPrefetchPage, FINAL_STATUS_RENDERER_CRASHED, 1);
  prerender->contents()->prerender_contents()->GetController().LoadURL(
      GURL(content::kChromeUICrashURL), content::Referrer(),
      ui::PAGE_TRANSITION_TYPED, std::string());
  prerender->WaitForStop();
}

// Checks that the prefetch of png correctly loads the png.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Png) {
  RequestCounter counter;
  CountRequestFor(kPrefetchPng, &counter);
  PrerenderTestURL(kPrefetchPng, FINAL_STATUS_APP_TERMINATING, 1);
  counter.WaitForCount(1);
}

// Checks that the prefetch of png correctly loads the jpeg.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Jpeg) {
  RequestCounter counter;
  CountRequestFor(kPrefetchJpeg, &counter);
  PrerenderTestURL(kPrefetchJpeg, FINAL_STATUS_APP_TERMINATING, 1);
  counter.WaitForCount(1);
}

// Checks that nothing is prefetched from malware sites.
// TODO(mattcary): disabled as prefetch process teardown is racey with prerender
// contents destruction, can fix when prefetch prerenderers are destroyed
// deterministically.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       DISABLED_PrerenderSafeBrowsingTopLevel) {
  GURL url = src_server()->GetURL(MakeAbsolute(kPrefetchPage));
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, safe_browsing::SB_THREAT_TYPE_URL_MALWARE);
  // Prefetch resources are blocked, but the prerender is not killed in any
  // special way.
  // TODO(mattcary): since the prerender will count itself as loaded even if the
  // fetch of the main resource fails, the test doesn't actually confirm what we
  // want it to confirm. This may be fixed by planned changes to the prerender
  // lifecycle.
  std::unique_ptr<TestPrerender> prerender =
      PrerenderTestURL(kPrefetchPage, FINAL_STATUS_SAFE_BROWSING, 1);
}

}  // namespace prerender
