// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/simple_test_tick_clock.h"
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
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/appcache_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using prerender::test_utils::DestructionWaiter;
using prerender::test_utils::RequestCounter;
using prerender::test_utils::TestPrerender;

namespace prerender {

// These URLs used for test resources must be relative with the exception of
// |kPrefetchLoaderPath|.
const char kPrefetchAppcache[] = "prerender/prefetch_appcache.html";
const char kPrefetchAppcacheManifest[] = "prerender/appcache.manifest";
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
const char kServiceWorkerLoader[] = "prerender/service_worker.html";
const char kPrefetchSubresourceRedirectPage[] =
    "prerender/prefetch_subresource_redirect.html";

class NoStatePrefetchBrowserTest
    : public test_utils::PrerenderInProcessBrowserTest {
 public:
  NoStatePrefetchBrowserTest() {}

  void SetUpOnMainThread() override {
    test_utils::PrerenderInProcessBrowserTest::SetUpOnMainThread();
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH);
    host_resolver()->AddRule("*", "127.0.0.1");
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
        base::BindOnce(&prerender::test_utils::CreateCountingInterceptorOnIO,
                       url, url_file, counter->AsWeakPtr()));
  }

  base::SimpleTestTickClock* OverridePrerenderManagerTimeTicks() {
    auto clock = base::MakeUnique<base::SimpleTestTickClock>();
    auto* clock_ptr = clock.get();
    // The default zero time causes the prerender manager to do strange things.
    clock->Advance(base::TimeDelta::FromSeconds(1));
    GetPrerenderManager()->SetTickClockForTesting(std::move(clock));
    return clock_ptr;
  }

  // Block until an AppCache exists for |manifest_url|.
  void WaitForAppcache(const GURL& manifest_url) {
    bool found_manifest = false;
    content::AppCacheService* appcache_service =
        content::BrowserContext::GetDefaultStoragePartition(
            current_browser()->profile())
            ->GetAppCacheService();
    do {
      base::RunLoop wait_loop;
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE,
          base::BindOnce(WaitForAppcacheOnIO, manifest_url, appcache_service,
                         wait_loop.QuitClosure(), &found_manifest));
      wait_loop.Run();
    } while (!found_manifest);
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
  // Schedule a task to retrieve AppCacheInfo from |appcache_service|. This sets
  // |found_manifest| if an appcache exists for |manifest_url|. |callback| will
  // be called on the UI thread after the info is retrieved, whether or not the
  // manifest exists.
  static void WaitForAppcacheOnIO(const GURL& manifest_url,
                                  content::AppCacheService* appcache_service,
                                  base::Closure callback,
                                  bool* found_manifest) {
    scoped_refptr<content::AppCacheInfoCollection> info_collection =
        new content::AppCacheInfoCollection();
    appcache_service->GetAllAppCacheInfo(
        info_collection.get(),
        base::Bind(ProcessAppCacheInfo, manifest_url, callback, found_manifest,
                   info_collection));
  }

  // Look through |info_collection| for an entry matching |target_manifest|,
  // setting |found_manifest| appropriately. Then |callback| will be invoked on
  // the UI thread.
  static void ProcessAppCacheInfo(
      const GURL& target_manifest,
      base::Closure callback,
      bool* found_manifest,
      scoped_refptr<content::AppCacheInfoCollection> info_collection,
      int status) {
    if (status == net::OK) {
      for (const auto& origin_pair : info_collection->infos_by_origin) {
        for (const auto& info : origin_pair.second) {
          if (info.manifest_url == target_manifest) {
            *found_manifest = true;
            break;
          }
        }
      }
    }
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     callback);
  }

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
  auto verify_prefetch_only = base::Bind([](net::URLRequest* request) {
    EXPECT_TRUE(request->load_flags() & net::LOAD_PREFETCH);
  });

  prerender::test_utils::InterceptRequestAndCount(
      src_server()->GetURL(MakeAbsolute(kPrefetchPage)), &main_counter,
      verify_prefetch_only);
  prerender::test_utils::InterceptRequestAndCount(
      src_server()->GetURL(MakeAbsolute(kPrefetchScript)), &script_counter,
      verify_prefetch_only);

  std::unique_ptr<TestPrerender> test_prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  main_counter.WaitForCount(1);
  script_counter.WaitForCount(1);

  // Verify that the page load did not happen.
  test_prerender->WaitForLoads(0);
}

// Check that a prefetch followed by a load produces the approriate
// histograms. Note that other histogram testing is done in
// browser/page_load_metrics, in particular, testing the combinations of
// Warm/Cold and Cacheable/NoCacheable.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchHistograms) {
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 0);

  test_utils::FirstContentfulPaintManagerWaiter* fcp_waiter =
      test_utils::FirstContentfulPaintManagerWaiter::Create(
          GetPrerenderManager());
  ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL(MakeAbsolute(kPrefetchPage)));
  fcp_waiter->Wait();

  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 1);
  histogram_tester().ExpectTotalCount(
      "Prerender.websame_NoStatePrefetchResponseTypes", 2);
  histogram_tester().ExpectTotalCount("Prerender.websame_PrefetchAge", 1);
}

// Checks the prefetch of an img tag.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchImage) {
  RequestCounter image_counter;
  CountRequestFor(kPrefetchJpeg, &image_counter);
  GURL main_page_url =
      GetURLWithReplacement(kPrefetchImagePage, "REPLACE_WITH_IMAGE_URL",
                            MakeAbsolute(kPrefetchJpeg));
  PrefetchFromURL(main_page_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  image_counter.WaitForCount(1);
}

// Checks that a cross-domain prefetching works correctly.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, PrefetchCrossDomain) {
  static const std::string secondary_domain = "www.foo.com";
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

  test_utils::CreateHangingFirstRequestInterceptor(
      first_url, first_path, base::Callback<void(net::URLRequest*)>());

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
  auto verify_prefetch_only = base::Bind([](net::URLRequest* request) {
    EXPECT_TRUE(request->load_flags() & net::LOAD_PREFETCH);
  });
  prerender::test_utils::InterceptRequestAndCount(
      redirect_url, &redirect_counter, verify_prefetch_only);
  RequestCounter page_counter;
  prerender::test_utils::InterceptRequestAndCount(page_url, &page_counter,
                                                  verify_prefetch_only);
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
  GURL main_page_url = GetURLWithReplacement(
      kPrefetchImagePage, "REPLACE_WITH_IMAGE_URL", https_url.spec());
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);

  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromURL(main_page_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
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

// If the main resource is unsafe, the whole prefetch is cancelled.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrerenderSafeBrowsingTopLevel) {
  GURL url = src_server()->GetURL(MakeAbsolute(kPrefetchPage));
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, safe_browsing::SB_THREAT_TYPE_URL_MALWARE);

  RequestCounter main_counter;
  CountRequestFor(kPrefetchPage, &main_counter);
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);

  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_SAFE_BROWSING);

  main_counter.WaitForCount(0);
  script_counter.WaitForCount(0);

  // Verify that the page load did not happen.
  prerender->WaitForLoads(0);
}

// If a subresource is unsafe, the corresponding request is cancelled.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       PrerenderSafeBrowsingSubresource) {
  GURL url = src_server()->GetURL(MakeAbsolute(kPrefetchScript));
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, safe_browsing::SB_THREAT_TYPE_URL_MALWARE);

  RequestCounter main_counter;
  CountRequestFor(kPrefetchPage, &main_counter);
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);

  std::unique_ptr<TestPrerender> prerender =
      PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  main_counter.WaitForCount(1);
  script_counter.WaitForCount(0);
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
  EXPECT_TRUE(base::ContainsValue(urls, navigated_url));

  // Check that the URL that was prefetched is not in history.
  EXPECT_FALSE(base::ContainsValue(urls, prefetched_url));

  // The loader URL is the remaining entry.
  EXPECT_EQ(2U, urls.size());
}

// Checks that prefetch requests have net::IDLE priority.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, IssuesIdlePriorityRequests) {
  GURL script_url = src_server()->GetURL(MakeAbsolute(kPrefetchScript));
  RequestCounter script_counter;
  prerender::test_utils::InterceptRequestAndCount(
      script_url, &script_counter, base::Bind([](net::URLRequest* request) {
#if defined(OS_ANDROID)
        // On Android requests from prerenders do not get downgraded priority.
        // See: https://crbug.com/652746.
        constexpr net::RequestPriority kExpectedPriority = net::HIGHEST;
#else
        constexpr net::RequestPriority kExpectedPriority = net::IDLE;
#endif
        EXPECT_EQ(kExpectedPriority, request->priority());
      }));
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  script_counter.WaitForCount(1);
}

// Checks that a registered ServiceWorker (SW) that is not currently running
// will intercepts a prefetch request.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, ServiceWorkerIntercept) {
  // Register and launch a SW.
  base::string16 expected_title = base::ASCIIToUTF16("SW READY");
  content::TitleWatcher title_watcher(GetActiveWebContents(), expected_title);
  ui_test_utils::NavigateToURL(
      current_browser(),
      src_server()->GetURL(MakeAbsolute(kServiceWorkerLoader)));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Stop any SW, killing the render process in order to test that the
  // lightweight renderer created for NoState prefetch does not interfere with
  // SW startup.
  int host_count = 0;
  for (content::RenderProcessHost::iterator iter(
           content::RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd(); iter.Advance()) {
    ++host_count;
    iter.GetCurrentValue()->Shutdown(content::RESULT_CODE_KILLED,
                                     true /* wait */);
  }
  // There should be at most one render_process_host, that created for the SW.
  EXPECT_EQ(1, host_count);

  // Open a new tab to replace the one closed with all the RenderProcessHosts.
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // The SW intercepts kPrefetchPage and replaces it with a body that contains
  // an <img> tage for kPrefetchPng. This verifies that the SW ran correctly by
  // observing the fetch of the image.
  RequestCounter image_counter;
  CountRequestFor(kPrefetchPng, &image_counter);
  PrefetchFromFile(kPrefetchPage, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  image_counter.WaitForCount(1);
}

// Checks that prefetching happens if an appcache is mentioned in the html tag
// but is uninitialized.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, AppCacheHtmlUninitialized) {
  RequestCounter image_counter;
  CountRequestFor(kPrefetchPng, &image_counter);
  PrefetchFromFile(kPrefetchAppcache, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  image_counter.WaitForCount(1);
}

// Checks that prefetching does not if an initialized appcache is mentioned in
// the html tag.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, AppCacheHtmlInitialized) {
  base::TimeTicks current_time = GetPrerenderManager()->GetCurrentTimeTicks();
  auto* clock = OverridePrerenderManagerTimeTicks();
  // Some navigations have already occurred in test setup. In order to track
  // duplicate prefetches correctly the test clock needs to be beyond those
  // navigations.
  clock->SetNowTicks(current_time);
  clock->Advance(base::TimeDelta::FromSeconds(600));

  // Fill manifest with the image url. The main resource will be cached
  // implicitly.
  GURL image_url = src_server()->GetURL(MakeAbsolute(kPrefetchPng));
  GURL manifest_url = GetURLWithReplacement(
      kPrefetchAppcacheManifest, "REPLACE_WITH_URL", image_url.spec());
  GURL appcache_page_url = GetURLWithReplacement(
      kPrefetchAppcache, "REPLACE_WITH_MANIFEST", manifest_url.spec());

  // Load the page into the appcache.
  ui_test_utils::NavigateToURL(current_browser(), appcache_page_url);

  WaitForAppcache(manifest_url);

  // If a page is prefetch shortly after being loading, the prefetch is
  // canceled. Advancing the clock prevents the cancelation.
  clock->Advance(base::TimeDelta::FromSeconds(6000));

  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);
  // While the prefetch stops when it sees the AppCache manifest, from the point
  // of view of the prerender manager the prefetch stops normally.
  PrefetchFromURL(appcache_page_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

  // The prefetch should have been canceled before the script in
  // kPrefetchAppcache is loaded (note the script is not mentioned in the
  // manifest).
  script_counter.WaitForCount(0);
}

// If a page has been cached by another AppCache, the prefetch should be
// canceled.
// Flaky on mac crbug.com/733504
#if defined(OS_MACOSX)
#define MAYBE_AppCacheRegistered DISABLED_AppCacheRegistered
#else
#define MAYBE_AppCacheRegistered AppCacheRegistered
#endif
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, MAYBE_AppCacheRegistered) {
  base::TimeTicks current_time = GetPrerenderManager()->GetCurrentTimeTicks();
  auto* clock = OverridePrerenderManagerTimeTicks();
  // Some navigations have already occurred in test setup. In order to track
  // duplicate prefetches correctly the test clock needs to be beyond those
  // navigations.
  clock->SetNowTicks(current_time);
  clock->Advance(base::TimeDelta::FromSeconds(600));

  // Fill manifest with kPrefetchPage so that it is cached without explicitly
  // listing a manifest.
  GURL prefetch_page_url = src_server()->GetURL(MakeAbsolute(kPrefetchPage));
  GURL manifest_url = GetURLWithReplacement(
      kPrefetchAppcacheManifest, "REPLACE_WITH_URL", prefetch_page_url.spec());

  GURL appcache_page_url = GetURLWithReplacement(
      kPrefetchAppcache, "REPLACE_WITH_MANIFEST", manifest_url.spec());

  // Load the page into the appcache.
  ui_test_utils::NavigateToURL(current_browser(), appcache_page_url);
  // Load the prefetch page so it can be cached.
  ui_test_utils::NavigateToURL(current_browser(), prefetch_page_url);

  WaitForAppcache(manifest_url);

  // If a page is prefetch shortly after being loading, the prefetch is
  // canceled. Advancing the clock prevents the cancelation.
  clock->Advance(base::TimeDelta::FromSeconds(6000));

  RequestCounter page_counter;
  CountRequestFor(kPrefetchPage, &page_counter);
  RequestCounter script_counter;
  CountRequestFor(kPrefetchScript, &script_counter);
  PrefetchFromURL(prefetch_page_url, FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
  // Neither the page nor the script should be prefetched.
  script_counter.WaitForCount(0);
  page_counter.WaitForCount(0);
}

}  // namespace prerender
