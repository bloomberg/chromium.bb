// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/prerender/prerender_handle.h"
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
using prerender::test_utils::CreatePrefetchOnlyInterceptorOnIO;
using prerender::test_utils::DestructionWaiter;
using prerender::test_utils::RequestCounter;
using prerender::test_utils::TestPrerender;

namespace prerender {

// These URLs used for test resources must be relative with the exception of
// |kPrefetchLoaderPath|.
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

class NoStatePrefetchBrowserTest
    : public test_utils::PrerenderInProcessBrowserTest {
 public:
  NoStatePrefetchBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PrerenderInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kPrerenderMode, switches::kPrerenderModeSwitchValuePrefetch);
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

 protected:
  std::unique_ptr<TestPrerender> PrefetchFromURL(
      const GURL& target_url,
      FinalStatus expected_final_status) {
    GURL loader_url = ServeLoaderURL(
        kPrefetchLoaderPath, "REPLACE_WITH_PREFETCH_URL", target_url, "");
    std::vector<FinalStatus> expected_final_status_queue(1,
                                                         expected_final_status);
    std::vector<std::unique_ptr<TestPrerender>> prerenders =
        NavigateWithPrerenders(loader_url, expected_final_status_queue);
    prerenders[0]->WaitForStop();
    return std::move(prerenders[0]);
  }

  std::unique_ptr<TestPrerender> PrefetchFromFile(
      const std::string& html_file,
      FinalStatus expected_final_status) {
    return PrefetchFromURL(src_server()->GetURL(MakeAbsolute(html_file)),
                           expected_final_status);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoStatePrefetchBrowserTest);
};

// Checks that a page is correctly prefetched in the case of a
// <link rel=prerender> tag and the JavaScript on the page is not executed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchSimple) {
  RequestCounter main_counter;
  CountRequestFor(kPrefetchPage, &main_counter);
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);
  RequestCounter script2_counter;
  CountRequestFor(kPrefetchScript2, &script2_counter);

  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  main_counter.WaitForCount(1);
  script_counter.WaitForCount(1);
  script2_counter.WaitForCount(0);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
}

// Check that the LOAD_PREFETCH flag is set.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchLoadFlag) {
  RequestCounter main_counter;
  RequestCounter script_counter;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CreatePrefetchOnlyInterceptorOnIO,
                 src_server()->GetURL(MakeAbsolute(kPrefetchPage)),
                 main_counter.AsWeakPtr()));
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CreatePrefetchOnlyInterceptorOnIO,
                 src_server()->GetURL(MakeAbsolute(kPrefetchScript)),
                 script_counter.AsWeakPtr()));

  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  main_counter.WaitForCount(1);
  script_counter.WaitForCount(1);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
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
  PrefetchFromFile(main_page_path, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
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
  PrefetchFromURL(cross_domain_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
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
  PrefetchFromFile(kPrefetchResponseHeaderCSP,
                   FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // The second script is in the correct domain for CSP, but the first script is
  // not.
  main_page.WaitForCount(1);
  second_script.WaitForCount(1);
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
  PrefetchFromFile(kPrefetchMetaCSP, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // TODO(mattcary): See test comment above. If the meta CSP tag were parsed,
  // |second_script| would be loaded. Instead as the background scanner bails as
  // soon as the meta CSP tag is seen, only |main_page| is fetched.
  main_page.WaitForCount(1);
  second_script.WaitForCount(0);
  first_script.WaitForCount(0);
}

// Checks that the second prefetch request succeeds. This test waits for
// Prerender Stop before starting the second request.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchMultipleRequest) {
  RequestCounter first_main_counter;
  CountRequestFor(kPrefetchPage, &first_main_counter);
  RequestCounter second_main_counter;
  CountRequestFor(kPrefetchPage2, &second_main_counter);
  RequestCounter first_script_counter;
  CountRequestFor(kPrefetchScript, &first_script_counter);
  RequestCounter second_script_counter;
  CountRequestFor(kPrefetchScript2, &second_script_counter);

  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  PrefetchFromFile(kPrefetchPage2, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  first_main_counter.WaitForCount(1);
  second_main_counter.WaitForCount(1);
  first_script_counter.WaitForCount(1);
  second_script_counter.WaitForCount(1);
}

// Checks that a second prefetch request, started before the first stops,
// succeeds.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchSimultaneous) {
  RequestCounter second_main_counter;
  CountRequestFor(kPrefetchPage2, &second_main_counter);
  RequestCounter second_script_counter;
  CountRequestFor(kPrefetchScript2, &second_script_counter);

  GURL first_url = src_server()->GetURL(MakeAbsolute(kPrefetchPage));
  base::FilePath first_path = ui_test_utils::GetTestFilePath(
      base::FilePath(), base::FilePath().AppendASCII(kPrefetchPage));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&test_utils::CreateHangingFirstRequestInterceptorOnIO,
                 first_url, first_path, base::Closure()));

  // Start the first prefetch directly instead of via PrefetchFromFile for the
  // first prefetch to avoid the wait on prerender stop.
  GURL first_loader_url = ServeLoaderURL(
      kPrefetchLoaderPath, "REPLACE_WITH_PREFETCH_URL", first_url, "");
  std::vector<FinalStatus> first_expected_status_queue(1,
                                                       FINAL_STATUS_CANCELLED);
  NavigateWithPrerenders(first_loader_url, first_expected_status_queue);

  PrefetchFromFile(kPrefetchPage2, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  second_main_counter.WaitForCount(1);
  second_script_counter.WaitForCount(1);
}

// Checks a prefetch to a nonexisting page.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchNonexisting) {
  std::unique_ptr<TestPrerender> test_prerender = PrefetchFromFile(
      "nonexisting-page.html", FINAL_STATUS_UNSUPPORTED_SCHEME);
}

// Checks that a 301 redirect is followed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Prefetch301Redirect) {
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);
  PrefetchFromFile(
      "/server-redirect/?" +
          net::EscapeQueryParamValue(MakeAbsolute(kPrefetchPage), false),
      FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  script_counter.WaitForCount(1);
}

// Checks that the load flags are set correctly for all resources in a 301
// redirect chain.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Prefetch301LoadFlags) {
  std::string redirect_path =
      "/server-redirect/?" +
      net::EscapeQueryParamValue(MakeAbsolute(kPrefetchPage), false);
  GURL redirect_url = src_server()->GetURL(redirect_path);
  GURL page_url = src_server()->GetURL(MakeAbsolute(kPrefetchPage));
  RequestCounter redirect_counter;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CreatePrefetchOnlyInterceptorOnIO, redirect_url,
                 redirect_counter.AsWeakPtr()));
  RequestCounter page_counter;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CreatePrefetchOnlyInterceptorOnIO, page_url,
                 page_counter.AsWeakPtr()));
  PrefetchFromFile(redirect_path, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  redirect_counter.WaitForCount(1);
  page_counter.WaitForCount(1);
}

// Checks that a subresource 301 redirect is followed.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Prefetch301Subresource) {
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);
  PrefetchFromFile(kPrefetchSubresourceRedirectPage,
                   FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
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
  PrefetchFromFile(
      "/client-redirect/?" +
          net::EscapeQueryParamValue(MakeAbsolute(kPrefetchPage), false),
      FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
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
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
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
  std::unique_ptr<TestPrerender> prerender = PrefetchFromURL(
      https_server.GetURL(MakeAbsolute(kPrefetchPage)), FINAL_STATUS_SSL_ERROR);
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
      PrefetchFromFile(main_page_path, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
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

  std::unique_ptr<TestPrerender> test_prerender = PrefetchFromFile(
      kPrefetchLoopPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  main_counter.WaitForCount(1);
  script_counter.WaitForCount(1);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, RendererCrash) {
  // Navigate to about:blank to get the session storage namespace.
  ui_test_utils::NavigateToURL(current_browser(), GURL(url::kAboutBlankURL));
  content::SessionStorageNamespace* storage_namespace =
      GetActiveWebContents()
          ->GetController()
          .GetDefaultSessionStorageNamespace();

  // Navigate to about:crash without an intermediate loader because chrome://
  // URLs are ignored in renderers, and the test server has no support for them.
  const gfx::Size kSize(640, 480);
  std::unique_ptr<TestPrerender> test_prerender =
      prerender_contents_factory()->ExpectPrerenderContents(
          FINAL_STATUS_RENDERER_CRASHED);
  std::unique_ptr<PrerenderHandle> prerender_handle(
      GetPrerenderManager()->AddPrerenderFromExternalRequest(
          GURL(content::kChromeUICrashURL), content::Referrer(),
          storage_namespace, gfx::Rect(kSize)));
  ASSERT_EQ(prerender_handle->contents(), test_prerender->contents());
  test_prerender->WaitForStop();
}

// Checks that the prefetch of png correctly loads the png.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Png) {
  RequestCounter counter;
  CountRequestFor(kPrefetchPng, &counter);
  PrefetchFromFile(kPrefetchPng, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  counter.WaitForCount(1);
}

// Checks that the prefetch of png correctly loads the jpeg.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, Jpeg) {
  RequestCounter counter;
  CountRequestFor(kPrefetchJpeg, &counter);
  PrefetchFromFile(kPrefetchJpeg, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
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
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_SAFE_BROWSING);
}

// Checks that prefetching a page does not add it to browsing history.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, HistoryUntouchedByPrefetch) {
  // Initialize.
  Profile* profile = current_browser()->profile();
  ASSERT_TRUE(profile);
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS));

  // Prefetch a page.
  GURL prefetched_url = src_server()->GetURL(MakeAbsolute(kPrefetchPage));
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  WaitForHistoryBackendToRun(profile);

  // Navigate to another page.
  GURL navigated_url = src_server()->GetURL(MakeAbsolute(kPrefetchPage2));
  ui_test_utils::NavigateToURL(current_browser(), navigated_url);
  WaitForHistoryBackendToRun(profile);

  // Check that the URL that was explicitly navigated to is already in history.
  ui_test_utils::HistoryEnumerator enumerator(profile);
  std::vector<GURL>& urls = enumerator.urls();
  EXPECT_TRUE(std::find(urls.begin(), urls.end(), navigated_url) != urls.end());

  // Check that the URL that was prefetched is not in history.
  EXPECT_TRUE(std::find(urls.begin(), urls.end(), prefetched_url) ==
              urls.end());

  // The loader URL is the remaining entry.
  EXPECT_EQ(2U, urls.size());
}

}  // namespace prerender
