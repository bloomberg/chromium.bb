// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_details.h"

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/metrics/metrics_memory_details.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_process_policy.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/metrics_service.h"
#include "components/variations/metrics_util.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/value_builder.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using content::WebContents;
using extensions::DictionaryBuilder;
using extensions::Extension;
using extensions::ListBuilder;
using extensions::TestExtensionDir;
using testing::ContainerEq;
using testing::ElementsAre;

namespace {

class TestMemoryDetails : public MetricsMemoryDetails {
 public:
  TestMemoryDetails()
      : MetricsMemoryDetails(base::Bind(&base::DoNothing), nullptr) {}

  void StartFetchAndWait() {
    uma_.reset(new base::HistogramTester());
    StartFetch(FROM_CHROME_ONLY);
    content::RunMessageLoop();
  }

  // Returns a HistogramTester which observed the most recent call to
  // StartFetchAndWait().
  base::HistogramTester* uma() { return uma_.get(); }

  int GetOutOfProcessIframeCount() {
    std::vector<Bucket> buckets =
        uma_->GetAllSamples("SiteIsolation.OutOfProcessIframes");
    CHECK_EQ(1U, buckets.size());
    return buckets[0].min;
  }

  size_t CountPageTitles() {
    size_t count = 0;
    for (const ProcessMemoryInformation& process : ChromeBrowser()->processes) {
      if (process.process_type == content::PROCESS_TYPE_RENDERER) {
        count += process.titles.size();
      }
    }
    return count;
  }

 private:
  ~TestMemoryDetails() override {}

  void OnDetailsAvailable() override {
    MetricsMemoryDetails::OnDetailsAvailable();
    // Exit the loop initiated by StartFetchAndWait().
    base::MessageLoop::current()->QuitWhenIdle();
  }

  scoped_ptr<base::HistogramTester> uma_;

  DISALLOW_COPY_AND_ASSIGN(TestMemoryDetails);
};

}  // namespace

class SiteDetailsBrowserTest : public ExtensionBrowserTest {
 public:
  SiteDetailsBrowserTest() {}
  ~SiteDetailsBrowserTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data so we can use cross_site_iframe_factory.html
    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("content/test/data/"));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Create and install an extension that has a couple of web-accessible
  // resources and, optionally, a background process.
  const Extension* CreateExtension(const std::string& name,
                                   bool has_background_process) {
    scoped_ptr<TestExtensionDir> dir(new TestExtensionDir);

    DictionaryBuilder manifest;
    manifest.Set("name", name)
        .Set("version", "1.0")
        .Set("manifest_version", 2)
        .Set("web_accessible_resources", ListBuilder()
                                             .Append("blank_iframe.html")
                                             .Append("http_iframe.html")
                                             .Append("two_http_iframes.html"));

    if (has_background_process) {
      manifest.Set("background",
                   DictionaryBuilder().Set("scripts",
                                           ListBuilder().Append("script.js")));
      dir->WriteFile(FILE_PATH_LITERAL("script.js"),
                     "console.log('" + name + " running');");
    }

    dir->WriteFile(FILE_PATH_LITERAL("blank_iframe.html"),
                   base::StringPrintf("<html><body>%s, blank iframe:"
                                      "  <iframe width=80 height=80></iframe>"
                                      "</body></html>",
                                      name.c_str()));
    std::string iframe_url =
        embedded_test_server()
            ->GetURL("w.com", "/cross_site_iframe_factory.html?w")
            .spec();
    std::string iframe_url2 =
        embedded_test_server()
            ->GetURL("x.com", "/cross_site_iframe_factory.html?x")
            .spec();
    dir->WriteFile(
        FILE_PATH_LITERAL("http_iframe.html"),
        base::StringPrintf("<html><body>%s, http:// iframe:"
                           "  <iframe width=80 height=80 src='%s'></iframe>"
                           "</body></html>",
                           name.c_str(), iframe_url.c_str()));
    dir->WriteFile(FILE_PATH_LITERAL("two_http_iframes.html"),
                   base::StringPrintf(
                       "<html><body>%s, two http:// iframes:"
                       "  <iframe width=80 height=80 src='%s'></iframe>"
                       "  <iframe width=80 height=80 src='%s'></iframe>"
                       "</body></html>",
                       name.c_str(), iframe_url.c_str(), iframe_url2.c_str()));
    dir->WriteManifest(manifest.ToJSON());

    const Extension* extension = LoadExtension(dir->unpacked_path());
    EXPECT_TRUE(extension);
    temp_dirs_.push_back(dir.release());
    return extension;
  }

  const Extension* CreateHostedApp(const std::string& name,
                                   const GURL& app_url) {
    scoped_ptr<TestExtensionDir> dir(new TestExtensionDir);

    DictionaryBuilder manifest;
    manifest.Set("name", name)
        .Set("version", "1.0")
        .Set("manifest_version", 2)
        .Set("app", DictionaryBuilder()
                        .Set("urls", ListBuilder().Append(app_url.spec()))
                        .Set("launch", DictionaryBuilder().Set(
                                           "web_url", app_url.spec())));
    dir->WriteManifest(manifest.ToJSON());

    const Extension* extension = LoadExtension(dir->unpacked_path());
    EXPECT_TRUE(extension);
    temp_dirs_.push_back(dir.release());
    return extension;
  }

  int GetRenderProcessCount() {
    int count = 0;
    for (content::RenderProcessHost::iterator it(
             content::RenderProcessHost::AllHostsIterator());
         !it.IsAtEnd(); it.Advance()) {
      count++;
    }
    return count;
  }

  // Checks whether the test run is part of a field trial with |trial_name|.
  bool IsInTrial(const std::string& trial_name) {
    uint32_t trial = metrics::HashName(trial_name);

    std::vector<variations::ActiveGroupId> synthetic_trials;
    g_browser_process->metrics_service()
        ->GetCurrentSyntheticFieldTrialsForTesting(&synthetic_trials);

    for (const auto& entry : synthetic_trials) {
      if (trial == entry.name)
        return true;
    }

    return false;
  }

  // Similar to IsInTrial but checks that the correct group is present as well.
  bool IsInTrialGroup(const std::string& trial_name,
                      const std::string& group_name) {
    uint32_t trial = metrics::HashName(trial_name);
    uint32_t group = metrics::HashName(group_name);

    std::vector<variations::ActiveGroupId> synthetic_trials;
    g_browser_process->metrics_service()
        ->GetCurrentSyntheticFieldTrialsForTesting(&synthetic_trials);

    for (const auto& entry : synthetic_trials) {
      if (trial == entry.name && group == entry.group)
        return true;
    }

    return false;
  }

 private:
  ScopedVector<TestExtensionDir> temp_dirs_;
  DISALLOW_COPY_AND_ASSIGN(SiteDetailsBrowserTest);
};

MATCHER_P(EqualsIfExtensionsIsolated, expected, "") {
  if (content::AreAllSitesIsolatedForTesting())
    return arg >= expected;
  if (extensions::IsIsolateExtensionsEnabled())
    return arg == expected;
  return true;
}

MATCHER_P(EqualsIfSitePerProcess, expected, "") {
  return !content::AreAllSitesIsolatedForTesting() || expected == arg;
}

// Test the accuracy of SiteDetails process estimation, in the presence of
// multiple iframes, navigation, multiple BrowsingInstances, and multiple tabs
// in the same BrowsingInstance.
IN_PROC_BROWSER_TEST_F(SiteDetailsBrowserTest, ManyIframes) {
  // Page with 14 nested oopifs across 9 sites (a.com through i.com).
  // None of these are https.
  GURL abcdefghi_url = embedded_test_server()->GetURL(
      "a.com",
      "/cross_site_iframe_factory.html?a(b(a(b,c,d,e,f,g,h)),c,d,e,i(f))");
  ui_test_utils::NavigateToURL(browser(), abcdefghi_url);

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(1U, details->CountPageTitles());
  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(9, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(9, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(9, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(1));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfSitePerProcess(9));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(0));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfSitePerProcess(14));

  // Navigate to a different, disjoint set of 7 sites.
  GURL pqrstuv_url = embedded_test_server()->GetURL(
      "p.com",
      "/cross_site_iframe_factory.html?p(q(r),r(s),s(t),t(q),u(u),v(p))");
  ui_test_utils::NavigateToURL(browser(), pqrstuv_url);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(1U, details->CountPageTitles());
  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(7, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(7, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(7, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(1));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfSitePerProcess(7));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(0));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfSitePerProcess(11));

  // Open a second tab (different BrowsingInstance) with 4 sites (a through d).
  GURL abcd_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c(d())))");
  AddTabAtIndex(1, abcd_url, ui::PAGE_TRANSITION_TYPED);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(2U, details->CountPageTitles());
  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(11, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(11, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(11, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));  // TODO(nick): This should be 2.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(2));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfSitePerProcess(11));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(0));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfSitePerProcess(14));

  // Open a third tab (different BrowsingInstance) with the same 4 sites.
  AddTabAtIndex(2, abcd_url, ui::PAGE_TRANSITION_TYPED);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  // Could be 11 if subframe processes were reused across BrowsingInstances.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(15, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(11, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(15, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));  // TODO(nick): This should be 3.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(3));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfSitePerProcess(15));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(0));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfSitePerProcess(17));

  // From the third tab, window.open() a fourth tab in the same
  // BrowsingInstance, to a page using the same four sites "a-d" as third tab,
  // plus an additional site "e". The estimated process counts should increase
  // by one (not five) from the previous scenario, as the new tab can reuse the
  // four processes already in the BrowsingInstance.
  GURL dcbae_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?d(c(b(a(e))))");
  ui_test_utils::UrlLoadObserver load_complete(
      dcbae_url, content::NotificationService::AllSources());
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.open('" + dcbae_url.spec() + "');"));
  ASSERT_EQ(4, browser()->tab_strip_model()->count());
  load_complete.Wait();

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  // Could be 11 if subframe processes were reused across BrowsingInstances.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(16, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(12, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(16, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));  // TODO(nick): This should be 3.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(3));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfSitePerProcess(16));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(0));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfSitePerProcess(21));

  // This test doesn't navigate to any extensions URLs, so it should not be
  // in any of the field trial groups.
  EXPECT_FALSE(IsInTrial("SiteIsolationExtensionsActive"));
}

IN_PROC_BROWSER_TEST_F(SiteDetailsBrowserTest, IsolateExtensions) {
  // We start on "about:blank", which should be credited with a process in this
  // case.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(GetRenderProcessCount(), 1);
  EXPECT_EQ(0, details->GetOutOfProcessIframeCount());

  // Install one script-injecting extension with background page, and an
  // extension with web accessible resources.
  const Extension* extension1 = CreateExtension("Extension One", true);
  const Extension* extension2 = CreateExtension("Extension Two", false);

  // Open two a.com tabs (with cross site http iframes). IsolateExtensions mode
  // should have no effect so far, since there are no frames straddling the
  // extension/web boundary.
  GURL tab1_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)");
  ui_test_utils::NavigateToURL(browser(), tab1_url);
  WebContents* tab1 = browser()->tab_strip_model()->GetWebContentsAt(0);
  GURL tab2_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(d,e)");
  AddTabAtIndex(1, tab2_url, ui::PAGE_TRANSITION_TYPED);
  WebContents* tab2 = browser()->tab_strip_model()->GetWebContentsAt(1);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(3));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(0));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(4));

  // Test that "one process per extension" applies even when web content has an
  // extension iframe.

  // Tab1 navigates its first iframe to a resource of extension1. This shouldn't
  // result in a new extension process (it should share with extension1's
  // background page).
  content::NavigateIframeToURL(
      tab1, "child-0", extension1->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(3));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(1));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(4));

  // Tab2 navigates its first iframe to a resource of extension1. This also
  // shouldn't result in a new extension process (it should share with the
  // background page and the other iframe).
  content::NavigateIframeToURL(
      tab2, "child-0", extension1->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(3));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(2));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(4));

  // Tab1 navigates its second iframe to a resource of extension2. This SHOULD
  // result in a new process since extension2 had no existing process.
  content::NavigateIframeToURL(
      tab1, "child-1", extension2->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(4));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(3));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(4));

  // Tab2 navigates its second iframe to a resource of extension2. This should
  // share the existing extension2 process.
  content::NavigateIframeToURL(
      tab2, "child-1", extension2->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(4));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(4));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(4));

  // Install extension3 (identical config to extension2)
  const Extension* extension3 = CreateExtension("Extension Three", false);

  // Navigate Tab2 to a top-level page from extension3. There are four processes
  // now: one for tab1's main frame, and one for each of the extensions:
  // extension1 has a process because it has a background page; extension2 is
  // used as an iframe in tab1, and extension3 is the top-level frame in tab2.
  ui_test_utils::NavigateToURL(browser(),
                               extension3->GetResourceURL("blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(4));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(2));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(2));

  // Navigate tab2 to a different extension3 page containing a web iframe. The
  // iframe should get its own process. The lower bound number indicates that,
  // in theory, the iframe could share a process with tab1's main frame.
  ui_test_utils::NavigateToURL(browser(),
                               extension3->GetResourceURL("http_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(5, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(5, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(5));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(3));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(3));

  // Navigate tab1 to an extension3 page with an extension3 iframe. There should
  // be three processes estimated by IsolateExtensions: one for extension3, one
  // for extension1's background page, and one for the web iframe in tab2.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ui_test_utils::NavigateToURL(browser(),
                               extension3->GetResourceURL("blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(3));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(1));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(1));

  // Now navigate tab1 to an extension3 page with a web iframe. This could share
  // a process with tab2's iframe (the LowerBound number), or it could get its
  // own process (the Estimate number).
  ui_test_utils::NavigateToURL(browser(),
                               extension3->GetResourceURL("http_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(4, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(4));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(2));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(2));

  EXPECT_TRUE(IsInTrial("SiteIsolationExtensionsActive"));
}

// Exercises accounting in the case where an extension has two different-site
// web iframes.
IN_PROC_BROWSER_TEST_F(SiteDetailsBrowserTest, ExtensionWithTwoWebIframes) {
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  // Install one script-injecting extension with background page, and an
  // extension with web accessible resources.
  const Extension* extension = CreateExtension("Test Extension", false);

  ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("/two_http_iframes.html"));

  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(2, 1)));
  // TODO(nick): https://crbug.com/512560 Make the number below agree with the
  // estimates above, which assume consolidation of subframe processes.
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(3));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(2));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(2));

  EXPECT_TRUE(IsInTrial("SiteIsolationExtensionsActive"));
}

// Verifies that --isolate-extensions doesn't isolate hosted apps.
IN_PROC_BROWSER_TEST_F(SiteDetailsBrowserTest, IsolateExtensionsHostedApps) {
  GURL app_with_web_iframe_url = embedded_test_server()->GetURL(
      "app.org", "/cross_site_iframe_factory.html?app.org(b.com)");
  GURL app_in_web_iframe_url = embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b.com(app.org)");

  // No hosted app is installed: app.org just behaves like a normal domain.
  ui_test_utils::NavigateToURL(browser(), app_with_web_iframe_url);
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(1));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfSitePerProcess(2));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(0));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(1));

  ui_test_utils::NavigateToURL(browser(), app_in_web_iframe_url);
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(1));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfSitePerProcess(2));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(0));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(1));

  // Now install app.org as a hosted app.
  CreateHostedApp("App", GURL("http://app.org"));

  // Reload the same two pages, and verify that the hosted app still is not
  // isolated by --isolate-extensions, but is isolated by --site-per-process.
  ui_test_utils::NavigateToURL(browser(), app_with_web_iframe_url);
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(1));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfSitePerProcess(2));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(0));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(1));

  ui_test_utils::NavigateToURL(browser(), app_in_web_iframe_url);
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(GetRenderProcessCount(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateNothingProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateExtensionsProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfExtensionsIsolated(1));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(GetRenderProcessCount(), EqualsIfSitePerProcess(2));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              EqualsIfExtensionsIsolated(0));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(), EqualsIfSitePerProcess(1));

  // Since hosted apps are excluded from isolation, this test should not be
  // in any of the field trial groups.
  EXPECT_FALSE(IsInTrial("SiteIsolationExtensionsActive"));
}

// Verifies that the client is put in the appropriate field trial group.
IN_PROC_BROWSER_TEST_F(SiteDetailsBrowserTest, VerifyFieldTrialGroup) {
  const Extension* extension = CreateExtension("Extension", false);
  GURL tab1_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)");
  ui_test_utils::NavigateToURL(browser(), tab1_url);
  WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(0);

  // Tab navigates its second iframe to a page of the extension.
  content::NavigateIframeToURL(tab, "child-1",
                               extension->GetResourceURL("/blank_iframe.html"));

  std::string group;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    group = "SitePerProcessFlag";
  } else if (extensions::IsIsolateExtensionsEnabled()) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kIsolateExtensions)) {
      group = "IsolateExtensionsFlag";
    } else {
      group = "FieldTrial";
    }
  } else {
    group = "Default";
  }

  EXPECT_TRUE(IsInTrialGroup("SiteIsolationExtensionsActive", group));
}
