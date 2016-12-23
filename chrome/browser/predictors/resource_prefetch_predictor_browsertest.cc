// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_factory.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace predictors {

namespace {

const char kFooHost[] = "foo.com";
const char kBarHost[] = "bar.com";

const char kImageMime[] = "image/png";
const char kStyleMime[] = "text/css";
const char kJavascriptMime[] = "application/javascript";

// Paths to resources handled by a custom request handler. They return empty
// responses with controllable response headers.
const char kImagePath[] = "/handled-by-test/image.png";
const char kImagePath2[] = "/handled-by-test/image2.png";
const char kStylePath[] = "/handled-by-test/style.css";
const char kStylePath2[] = "/handled-by-test/style2.css";
const char kScriptPath[] = "/handled-by-test/script.js";
const char kScriptPath2[] = "/handled-by-test/script2.js";
const char kFontPath[] = "/handled-by-test/font.ttf";
const char kRedirectPath[] = "/handled-by-test/redirect.html";
const char kRedirectPath2[] = "/handled-by-test/redirect2.html";
const char kRedirectPath3[] = "/handled-by-test/redirect3.html";

// These are loaded from a file by the test server.
const char kHtmlSubresourcesPath[] = "/predictors/html_subresources.html";
const char kHtmlDocumentWritePath[] = "/predictors/document_write.html";
const char kScriptDocumentWritePath[] = "/predictors/document_write.js";
const char kHtmlAppendChildPath[] = "/predictors/append_child.html";
const char kScriptAppendChildPath[] = "/predictors/append_child.js";
const char kHtmlInnerHtmlPath[] = "/predictors/inner_html.html";
const char kScriptInnerHtmlPath[] = "/predictors/inner_html.js";
const char kHtmlXHRPath[] = "/predictors/xhr.html";
const char kScriptXHRPath[] = "/predictors/xhr.js";
const char kHtmlIframePath[] = "/predictors/html_iframe.html";

struct ResourceSummary {
  ResourceSummary()
      : is_no_store(false),
        version(0),
        is_external(false),
        should_be_recorded(true) {}

  ResourcePrefetchPredictor::URLRequestSummary request;
  std::string content;
  bool is_no_store;
  size_t version;
  bool is_external;
  bool should_be_recorded;
};

struct RedirectEdge {
  // This response code should be returned by previous url in the chain.
  net::HttpStatusCode code;
  GURL url;
};

// Helper class to track and allow waiting for ResourcePrefetchPredictor
// initialization. WARNING: OnPredictorInitialized event will not be fired if
// ResourcePrefetchPredictor is initialized before the observer creation.
class InitializationObserver : public TestObserver {
 public:
  explicit InitializationObserver(ResourcePrefetchPredictor* predictor)
      : TestObserver(predictor) {}

  void OnPredictorInitialized() override { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(InitializationObserver);
};

class BrowsingDataRemoverObserver : public BrowsingDataRemover::Observer {
 public:
  explicit BrowsingDataRemoverObserver(BrowsingDataRemover* remover)
      : remover_(remover) {
    remover_->AddObserver(this);
  }
  ~BrowsingDataRemoverObserver() override { remover_->RemoveObserver(this); }

  void OnBrowsingDataRemoverDone() override { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

 private:
  BrowsingDataRemover* remover_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverObserver);
};

using PageRequestSummary = ResourcePrefetchPredictor::PageRequestSummary;
using URLRequestSummary = ResourcePrefetchPredictor::URLRequestSummary;

void RemoveDuplicateSubresources(std::vector<URLRequestSummary>* subresources) {
  std::stable_sort(subresources->begin(), subresources->end(),
                   [](const URLRequestSummary& x, const URLRequestSummary& y) {
                     return x.resource_url < y.resource_url;
                   });
  subresources->erase(
      std::unique(subresources->begin(), subresources->end(),
                  [](const URLRequestSummary& x, const URLRequestSummary& y) {
                    return x.resource_url == y.resource_url;
                  }),
      subresources->end());
}

// Fill a NavigationID with "empty" data that does not trigger
// the is_valid DCHECK(). Allows comparing.
void SetValidNavigationID(NavigationID* navigation_id) {
  navigation_id->tab_id = 0;
  navigation_id->main_frame_url = GURL("http://127.0.0.1");
}

void ModifySubresourceForComparison(URLRequestSummary* subresource,
                                    bool match_navigation_id) {
  if (!match_navigation_id)
    SetValidNavigationID(&subresource->navigation_id);
  if (subresource->resource_type == content::RESOURCE_TYPE_IMAGE &&
      subresource->priority == net::LOWEST) {
    // Fuzzy comparison for images because an image priority can be
    // boosted during layout via
    // ResourceFetcher::updateAllImageResourcePriorities().
    subresource->priority = net::MEDIUM;
  }
}

// Does a custom comparison of subresources of URLRequestSummary
// and fail the test if the expectation is not met.
void CompareSubresources(std::vector<URLRequestSummary> actual_subresources,
                         std::vector<URLRequestSummary> expected_subresources,
                         bool match_navigation_id) {
  // Duplicate resources can be observed in a single navigation but
  // ResourcePrefetchPredictor only cares about the first occurrence of each.
  RemoveDuplicateSubresources(&actual_subresources);

  for (auto& subresource : actual_subresources)
    ModifySubresourceForComparison(&subresource, match_navigation_id);
  for (auto& subresource : expected_subresources)
    ModifySubresourceForComparison(&subresource, match_navigation_id);

  EXPECT_THAT(actual_subresources,
              testing::UnorderedElementsAreArray(expected_subresources));
}

}  // namespace

// Helper class to track and allow waiting for a single OnNavigationLearned
// event. The information provided by this event is also used to verify that
// ResourcePrefetchPredictor works as expected.
class LearningObserver : public TestObserver {
 public:
  using PageRequestSummary = ResourcePrefetchPredictor::PageRequestSummary;

  LearningObserver(ResourcePrefetchPredictor* predictor,
                   const size_t expected_url_visit_count,
                   const PageRequestSummary& expected_summary,
                   bool match_navigation_id)
      : TestObserver(predictor),
        url_visit_count_(expected_url_visit_count),
        summary_(expected_summary),
        match_navigation_id_(match_navigation_id) {}

  // TestObserver:
  void OnNavigationLearned(size_t url_visit_count,
                           const PageRequestSummary& summary) override {
    EXPECT_EQ(url_visit_count, url_visit_count_);
    EXPECT_EQ(summary.main_frame_url, summary_.main_frame_url);
    EXPECT_EQ(summary.initial_url, summary_.initial_url);
    for (const auto& resource : summary.subresource_requests)
      current_navigation_ids_.insert(resource.navigation_id);
    CompareSubresources(summary.subresource_requests,
                        summary_.subresource_requests, match_navigation_id_);
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

  std::set<NavigationID>& current_navigation_ids() {
    return current_navigation_ids_;
  }

 private:
  base::RunLoop run_loop_;
  size_t url_visit_count_;
  PageRequestSummary summary_;
  bool match_navigation_id_;
  std::set<NavigationID> current_navigation_ids_;

  DISALLOW_COPY_AND_ASSIGN(LearningObserver);
};

// Helper class to track and allow waiting for a single OnPrefetchingFinished
// event. No learning events should be fired while this observer is active.
class PrefetchingObserver : public TestObserver {
 public:
  PrefetchingObserver(ResourcePrefetchPredictor* predictor,
                      const GURL& expected_main_frame_url)
      : TestObserver(predictor), main_frame_url_(expected_main_frame_url) {}

  // TestObserver:
  void OnNavigationLearned(size_t url_visit_count,
                           const PageRequestSummary& summary) override {
    ADD_FAILURE() << "Prefetching shouldn't activate learning";
  }

  void OnPrefetchingFinished(const GURL& main_frame_url) override {
    EXPECT_EQ(main_frame_url_, main_frame_url);
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  GURL main_frame_url_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchingObserver);
};

class ResourcePrefetchPredictorBrowserTest : public InProcessBrowserTest {
 protected:
  using URLRequestSummary = ResourcePrefetchPredictor::URLRequestSummary;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kSpeculativeResourcePrefetching,
        switches::kSpeculativeResourcePrefetchingEnabledExternal);
  }

  void SetUpOnMainThread() override {
    // Resolving all hosts to local allows us to have
    // cross domains navigations (matching url_visit_count_, etc).
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&ResourcePrefetchPredictorBrowserTest::HandleRedirectRequest,
                   base::Unretained(this)));
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&ResourcePrefetchPredictorBrowserTest::HandleResourceRequest,
                   base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
    predictor_ =
        ResourcePrefetchPredictorFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(predictor_);
    EnsurePredictorInitialized();
  }

  void TestLearningAndPrefetching(const GURL& main_frame_url) {
    // Navigate to |main_frame_url| and check all the expectations.
    NavigateToURLAndCheckSubresources(main_frame_url);
    ClearCache();
    // It is needed to have at least two resource hits to trigger prefetch.
    NavigateToURLAndCheckSubresources(main_frame_url);
    ClearCache();
    // Prefetch all needed resources and change expectations so that all
    // cacheable resources should be served from cache next navigation.
    PrefetchURL(main_frame_url);
    NavigateToURLAndCheckSubresources(main_frame_url);
  }

  void NavigateToURLAndCheckSubresources(
      const GURL& main_frame_url,
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB) {
    GURL endpoint_url = GetRedirectEndpoint(main_frame_url);
    std::vector<URLRequestSummary> url_request_summaries;
    for (const auto& kv : resources_) {
      if (kv.second.is_no_store || !kv.second.should_be_recorded)
        continue;
      url_request_summaries.push_back(
          GetURLRequestSummaryForResource(endpoint_url, kv.second));
    }

    bool match_navigation_id =
        disposition == WindowOpenDisposition::CURRENT_TAB;

    LearningObserver observer(
        predictor_, UpdateAndGetVisitCount(main_frame_url),
        CreatePageRequestSummary(endpoint_url.spec(), main_frame_url.spec(),
                                 url_request_summaries),
        match_navigation_id);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), main_frame_url, disposition,
        ui_test_utils::BROWSER_TEST_NONE);
    observer.Wait();
    for (auto& kv : resources_) {
      if (!kv.second.is_no_store && kv.second.should_be_recorded)
        kv.second.request.was_cached = true;
    }
    for (const auto& nav : observer.current_navigation_ids())
      navigation_id_history_.insert(nav);
  }

  void PrefetchURL(const GURL& main_frame_url) {
    PrefetchingObserver observer(predictor_, main_frame_url);
    predictor_->StartPrefetching(main_frame_url, PrefetchOrigin::EXTERNAL);
    observer.Wait();
    for (auto& kv : resources_) {
      if (!kv.second.is_no_store && kv.second.should_be_recorded)
        kv.second.request.was_cached = true;
    }
  }

  ResourceSummary* AddResource(const GURL& resource_url,
                               content::ResourceType resource_type,
                               net::RequestPriority priority) {
    auto pair_and_whether_inserted =
        resources_.insert(std::make_pair(resource_url, ResourceSummary()));
    EXPECT_TRUE(pair_and_whether_inserted.second) << resource_url
                                                  << " was inserted twice";
    ResourceSummary* resource = &pair_and_whether_inserted.first->second;
    resource->request.resource_url = resource_url;
    resource->request.resource_type = resource_type;
    resource->request.priority = priority;
    resource->request.has_validators = true;
    return resource;
  }

  ResourceSummary* AddExternalResource(const GURL& resource_url,
                                       content::ResourceType resource_type,
                                       net::RequestPriority priority) {
    auto resource = AddResource(resource_url, resource_type, priority);
    resource->is_external = true;
    return resource;
  }

  void AddUnrecordedResources(const std::vector<GURL>& resource_urls) {
    for (const GURL& resource_url : resource_urls) {
      auto resource =
          AddResource(resource_url, content::RESOURCE_TYPE_SUB_RESOURCE,
                      net::DEFAULT_PRIORITY);
      resource->should_be_recorded = false;
    }
  }

  void AddRedirectChain(const GURL& initial_url,
                        const std::vector<RedirectEdge>& redirect_chain) {
    ASSERT_FALSE(redirect_chain.empty());
    GURL current = initial_url;
    for (const auto& edge : redirect_chain) {
      auto result = redirects_.insert(std::make_pair(current, edge));
      EXPECT_TRUE(result.second) << current << " already has a redirect.";
      current = edge.url;
    }
  }

  void ClearResources() { resources_.clear(); }

  void ClearCache() {
    BrowsingDataRemover* remover =
        BrowsingDataRemoverFactory::GetForBrowserContext(browser()->profile());
    BrowsingDataRemoverObserver observer(remover);
    remover->RemoveAndReply(BrowsingDataRemover::Unbounded(),
                            BrowsingDataRemover::REMOVE_CACHE,
                            BrowsingDataHelper::UNPROTECTED_WEB, &observer);
    observer.Wait();

    for (auto& kv : resources_)
      kv.second.request.was_cached = false;
  }

  // Shortcut for convenience.
  GURL GetURL(const std::string& path) const {
    return embedded_test_server()->GetURL(path);
  }

  void EnableHttpsServer() {
    ASSERT_FALSE(https_server_);
    https_server_ = base::MakeUnique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server()->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
    https_server()->RegisterRequestHandler(
        base::Bind(&ResourcePrefetchPredictorBrowserTest::HandleRedirectRequest,
                   base::Unretained(this)));
    https_server()->RegisterRequestHandler(
        base::Bind(&ResourcePrefetchPredictorBrowserTest::HandleResourceRequest,
                   base::Unretained(this)));
    ASSERT_TRUE(https_server()->Start());
  }

  // Returns the embedded test server working over HTTPS. Must be enabled by
  // calling EnableHttpsServer() before use.
  const net::EmbeddedTestServer* https_server() const {
    return https_server_.get();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  size_t navigation_ids_history_size() const {
    return navigation_id_history_.size();
  }

 private:
  // ResourcePrefetchPredictor needs to be initialized before the navigation
  // happens otherwise this navigation will be ignored by predictor.
  void EnsurePredictorInitialized() {
    if (predictor_->initialization_state_ ==
        ResourcePrefetchPredictor::INITIALIZED) {
      return;
    }

    InitializationObserver observer(predictor_);
    if (predictor_->initialization_state_ ==
        ResourcePrefetchPredictor::NOT_INITIALIZED) {
      predictor_->StartInitialization();
    }
    observer.Wait();
  }

  URLRequestSummary GetURLRequestSummaryForResource(
      const GURL& main_frame_url,
      const ResourceSummary& resource_summary) const {
    URLRequestSummary summary(resource_summary.request);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    summary.navigation_id =
        NavigationID(web_contents, main_frame_url, base::TimeTicks::Now());
    return summary;
  }

  GURL GetRedirectEndpoint(const GURL& initial_url) const {
    GURL current = initial_url;
    while (true) {
      auto it = redirects_.find(current);
      if (it == redirects_.end())
        break;
      current = it->second.url;
    }
    return current;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleResourceRequest(
      const net::test_server::HttpRequest& request) const {
    GURL resource_url = request.GetURL();
    // Retrieve the host that was used in the request because
    // resource_url contains a resolved host (e.g. 127.0.0.1).
    if (request.headers.find("Host") != request.headers.end()) {
      auto host_port_pair =
          net::HostPortPair::FromString(request.headers.at("Host"));
      GURL::Replacements replace_host;
      replace_host.SetHostStr(host_port_pair.host());
      resource_url = resource_url.ReplaceComponents(replace_host);
    } else {
      ADD_FAILURE() << "Host header was not found in a HttpRequest to url: "
                    << resource_url.spec();
    }

    auto resource_it = resources_.find(resource_url);
    if (resource_it == resources_.end())
      return nullptr;

    const ResourceSummary& summary = resource_it->second;
    if (summary.is_external)
      return nullptr;

    auto http_response =
        base::MakeUnique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    if (!summary.request.mime_type.empty())
      http_response->set_content_type(summary.request.mime_type);
    if (!summary.content.empty())
      http_response->set_content(summary.content);
    if (summary.is_no_store)
      http_response->AddCustomHeader("Cache-Control", "no-store");
    if (summary.request.has_validators) {
      http_response->AddCustomHeader(
          "ETag", base::StringPrintf("'%zu%s'", summary.version,
                                     request.relative_url.c_str()));
    }
    if (summary.request.always_revalidate)
      http_response->AddCustomHeader("Cache-Control", "no-cache");
    else
      http_response->AddCustomHeader("Cache-Control", "max-age=2147483648");
    return std::move(http_response);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRedirectRequest(
      const net::test_server::HttpRequest& request) const {
    auto redirect_it = redirects_.find(request.GetURL());
    if (redirect_it == redirects_.end())
      return nullptr;

    auto http_response =
        base::MakeUnique<net::test_server::BasicHttpResponse>();
    http_response->set_code(redirect_it->second.code);
    http_response->AddCustomHeader("Location", redirect_it->second.url.spec());
    return std::move(http_response);
  }

  size_t UpdateAndGetVisitCount(const GURL& main_frame_url) {
    return ++visit_count_[main_frame_url];
  }

  ResourcePrefetchPredictor* predictor_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::map<GURL, ResourceSummary> resources_;
  std::map<GURL, RedirectEdge> redirects_;
  std::map<GURL, size_t> visit_count_;
  std::set<NavigationID> navigation_id_history_;
};

IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest, Simple) {
  // These resources have default priorities that correspond to
  // blink::typeToPriority function.
  AddResource(GetURL(kImagePath), content::RESOURCE_TYPE_IMAGE, net::LOWEST);
  AddResource(GetURL(kStylePath), content::RESOURCE_TYPE_STYLESHEET,
              net::HIGHEST);
  AddResource(GetURL(kScriptPath), content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  AddResource(GetURL(kFontPath), content::RESOURCE_TYPE_FONT_RESOURCE,
              net::HIGHEST);
  TestLearningAndPrefetching(GetURL(kHtmlSubresourcesPath));
}

IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest, Redirect) {
  AddRedirectChain(GetURL(kRedirectPath), {{net::HTTP_MOVED_PERMANENTLY,
                                            GetURL(kHtmlSubresourcesPath)}});
  AddResource(GetURL(kImagePath), content::RESOURCE_TYPE_IMAGE, net::LOWEST);
  AddResource(GetURL(kStylePath), content::RESOURCE_TYPE_STYLESHEET,
              net::HIGHEST);
  AddResource(GetURL(kScriptPath), content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  AddResource(GetURL(kFontPath), content::RESOURCE_TYPE_FONT_RESOURCE,
              net::HIGHEST);
  TestLearningAndPrefetching(GetURL(kRedirectPath));
}

IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest, RedirectChain) {
  AddRedirectChain(GetURL(kRedirectPath),
                   {{net::HTTP_FOUND, GetURL(kRedirectPath2)},
                    {net::HTTP_MOVED_PERMANENTLY, GetURL(kRedirectPath3)},
                    {net::HTTP_FOUND, GetURL(kHtmlSubresourcesPath)}});
  AddResource(GetURL(kImagePath), content::RESOURCE_TYPE_IMAGE, net::LOWEST);
  AddResource(GetURL(kStylePath), content::RESOURCE_TYPE_STYLESHEET,
              net::HIGHEST);
  AddResource(GetURL(kScriptPath), content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  AddResource(GetURL(kFontPath), content::RESOURCE_TYPE_FONT_RESOURCE,
              net::HIGHEST);
  TestLearningAndPrefetching(GetURL(kRedirectPath));
}

IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest,
                       HttpToHttpsRedirect) {
  EnableHttpsServer();
  AddRedirectChain(GetURL(kRedirectPath),
                   {{net::HTTP_MOVED_PERMANENTLY,
                     https_server()->GetURL(kHtmlSubresourcesPath)}});
  AddResource(https_server()->GetURL(kImagePath), content::RESOURCE_TYPE_IMAGE,
              net::LOWEST);
  AddResource(https_server()->GetURL(kStylePath),
              content::RESOURCE_TYPE_STYLESHEET, net::HIGHEST);
  AddResource(https_server()->GetURL(kScriptPath),
              content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  AddResource(https_server()->GetURL(kFontPath),
              content::RESOURCE_TYPE_FONT_RESOURCE, net::HIGHEST);
  TestLearningAndPrefetching(GetURL(kRedirectPath));
}

IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest,
                       JavascriptDocumentWrite) {
  auto externalScript =
      AddExternalResource(GetURL(kScriptDocumentWritePath),
                          content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  externalScript->request.mime_type = kJavascriptMime;
  AddResource(GetURL(kImagePath), content::RESOURCE_TYPE_IMAGE, net::LOWEST);
  AddResource(GetURL(kStylePath), content::RESOURCE_TYPE_STYLESHEET,
              net::HIGHEST);
  AddResource(GetURL(kScriptPath), content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  TestLearningAndPrefetching(GetURL(kHtmlDocumentWritePath));
}

IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest,
                       JavascriptAppendChild) {
  auto externalScript =
      AddExternalResource(GetURL(kScriptAppendChildPath),
                          content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  externalScript->request.mime_type = kJavascriptMime;
  AddResource(GetURL(kImagePath), content::RESOURCE_TYPE_IMAGE, net::LOWEST);
  AddResource(GetURL(kStylePath), content::RESOURCE_TYPE_STYLESHEET,
              net::HIGHEST);
  // This script has net::LOWEST priority because it's executed asynchronously.
  AddResource(GetURL(kScriptPath), content::RESOURCE_TYPE_SCRIPT, net::LOWEST);
  TestLearningAndPrefetching(GetURL(kHtmlAppendChildPath));
}

IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest,
                       JavascriptInnerHtml) {
  auto externalScript = AddExternalResource(
      GetURL(kScriptInnerHtmlPath), content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  externalScript->request.mime_type = kJavascriptMime;
  AddResource(GetURL(kImagePath), content::RESOURCE_TYPE_IMAGE, net::LOWEST);
  AddResource(GetURL(kStylePath), content::RESOURCE_TYPE_STYLESHEET,
              net::HIGHEST);
  // https://www.w3.org/TR/2014/REC-html5-20141028/scripting-1.html#the-script-element
  // Script elements don't execute when inserted using innerHTML attribute.
  AddUnrecordedResources({GetURL(kScriptPath)});
  TestLearningAndPrefetching(GetURL(kHtmlInnerHtmlPath));
}

// Requests originated by XMLHttpRequest have content::RESOURCE_TYPE_XHR.
// Actual resource type is inferred from the mime-type.
IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest, JavascriptXHR) {
  auto externalScript = AddExternalResource(
      GetURL(kScriptXHRPath), content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  externalScript->request.mime_type = kJavascriptMime;
  auto image = AddResource(GetURL(kImagePath), content::RESOURCE_TYPE_IMAGE,
                           net::HIGHEST);
  image->request.mime_type = kImageMime;
  auto style = AddResource(GetURL(kStylePath),
                           content::RESOURCE_TYPE_STYLESHEET, net::HIGHEST);
  style->request.mime_type = kStyleMime;
  auto script = AddResource(GetURL(kScriptPath), content::RESOURCE_TYPE_SCRIPT,
                            net::HIGHEST);
  script->request.mime_type = kJavascriptMime;
  TestLearningAndPrefetching(GetURL(kHtmlXHRPath));
}

// ResourcePrefetchPredictor ignores all resources requested from subframes.
IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest,
                       IframeShouldBeIgnored) {
  // Included from html_iframe.html.
  AddResource(GetURL(kImagePath2), content::RESOURCE_TYPE_IMAGE, net::LOWEST);
  AddResource(GetURL(kStylePath2), content::RESOURCE_TYPE_STYLESHEET,
              net::HIGHEST);
  AddResource(GetURL(kScriptPath2), content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  // Included from <iframe src="html_subresources.html"> and not recored.
  AddUnrecordedResources({GetURL(kImagePath), GetURL(kStylePath),
                          GetURL(kScriptPath), GetURL(kFontPath)});
  TestLearningAndPrefetching(GetURL(kHtmlIframePath));
}

IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest,
                       CrossSiteNavigation) {
  AddResource(embedded_test_server()->GetURL(kFooHost, kImagePath),
              content::RESOURCE_TYPE_IMAGE, net::LOWEST);
  AddResource(embedded_test_server()->GetURL(kFooHost, kStylePath),
              content::RESOURCE_TYPE_STYLESHEET, net::HIGHEST);
  AddResource(embedded_test_server()->GetURL(kFooHost, kScriptPath),
              content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  AddResource(embedded_test_server()->GetURL(kFooHost, kFontPath),
              content::RESOURCE_TYPE_FONT_RESOURCE, net::HIGHEST);
  TestLearningAndPrefetching(
      embedded_test_server()->GetURL(kFooHost, kHtmlSubresourcesPath));
  ClearResources();

  AddResource(embedded_test_server()->GetURL(kBarHost, kImagePath),
              content::RESOURCE_TYPE_IMAGE, net::LOWEST);
  AddResource(embedded_test_server()->GetURL(kBarHost, kStylePath),
              content::RESOURCE_TYPE_STYLESHEET, net::HIGHEST);
  AddResource(embedded_test_server()->GetURL(kBarHost, kScriptPath),
              content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  AddResource(embedded_test_server()->GetURL(kBarHost, kFontPath),
              content::RESOURCE_TYPE_FONT_RESOURCE, net::HIGHEST);
  // Navigating to kBarHost, although done in the same tab, will generate a new
  // process.
  TestLearningAndPrefetching(
      embedded_test_server()->GetURL(kBarHost, kHtmlSubresourcesPath));
}

// In this test we are trying to assess if :
// - Reloading in the same tab is using the same NavigationID.
// - Navigation into a new tab, window or popup yields different NavigationID's.
// - Navigating twice to a new browser/popup yields different NavigationID's.
IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest,
                       TabIdBehavingAsExpected) {
  AddResource(GetURL(kImagePath), content::RESOURCE_TYPE_IMAGE, net::LOWEST);
  AddResource(GetURL(kStylePath), content::RESOURCE_TYPE_STYLESHEET,
              net::HIGHEST);
  AddResource(GetURL(kScriptPath), content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  AddResource(GetURL(kFontPath), content::RESOURCE_TYPE_FONT_RESOURCE,
              net::HIGHEST);
  NavigateToURLAndCheckSubresources(GetURL(kHtmlSubresourcesPath));
  EXPECT_EQ(navigation_ids_history_size(), 1U);
  ClearCache();
  NavigateToURLAndCheckSubresources(GetURL(kHtmlSubresourcesPath));
  EXPECT_EQ(navigation_ids_history_size(), 1U);
  ClearCache();
  NavigateToURLAndCheckSubresources(GetURL(kHtmlSubresourcesPath),
                                    WindowOpenDisposition::NEW_BACKGROUND_TAB);
  EXPECT_EQ(navigation_ids_history_size(), 2U);
  ClearCache();
  NavigateToURLAndCheckSubresources(GetURL(kHtmlSubresourcesPath),
                                    WindowOpenDisposition::NEW_WINDOW);
  EXPECT_EQ(navigation_ids_history_size(), 3U);
  ClearCache();
  NavigateToURLAndCheckSubresources(GetURL(kHtmlSubresourcesPath),
                                    WindowOpenDisposition::NEW_POPUP);
  EXPECT_EQ(navigation_ids_history_size(), 4U);
}

}  // namespace predictors
