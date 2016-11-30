// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_factory.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace predictors {

namespace {

static const char kImagePath[] = "/predictors/image.png";
static const char kStylePath[] = "/predictors/style.css";
static const char kScriptPath[] = "/predictors/script.js";
static const char kFontPath[] = "/predictors/font.ttf";
static const char kHtmlSubresourcesPath[] =
    "/predictors/html_subresources.html";
static const char kRedirectPath[] = "/predictors/redirect.html";
static const char kRedirectPath2[] = "/predictors/redirect2.html";
static const char kRedirectPath3[] = "/predictors/redirect3.html";

struct ResourceSummary {
  ResourceSummary() : is_no_store(false), version(0) {}

  ResourcePrefetchPredictor::URLRequestSummary request;
  std::string content;
  bool is_no_store;
  size_t version;
};

struct RedirectEdge {
  // This response code should be returned by previous url in the chain.
  net::HttpStatusCode code;
  GURL url;
};

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

using PageRequestSummary = ResourcePrefetchPredictor::PageRequestSummary;
using URLRequestSummary = ResourcePrefetchPredictor::URLRequestSummary;

std::vector<URLRequestSummary> GetUniqueSubresources(
    const PageRequestSummary& summary) {
  std::vector<URLRequestSummary> subresources(summary.subresource_requests);
  std::stable_sort(subresources.begin(), subresources.end(),
                   [](const URLRequestSummary& x, const URLRequestSummary& y) {
                     return x.resource_url < y.resource_url;
                   });
  subresources.erase(
      std::unique(subresources.begin(), subresources.end(),
                  [](const URLRequestSummary& x, const URLRequestSummary& y) {
                    return x.resource_url == y.resource_url;
                  }),
      subresources.end());
  return subresources;
}

}  // namespace

// Helper class to track and allow waiting for ResourcePrefetchPredictor events.
// These events are also used to verify that ResourcePrefetchPredictor works as
// expected.
class ResourcePrefetchPredictorTestObserver : public TestObserver {
 public:
  using PageRequestSummary = ResourcePrefetchPredictor::PageRequestSummary;

  explicit ResourcePrefetchPredictorTestObserver(
      ResourcePrefetchPredictor* predictor,
      const size_t expected_url_visit_count,
      const PageRequestSummary& expected_summary)
      : TestObserver(predictor),
        url_visit_count_(expected_url_visit_count),
        summary_(expected_summary) {}

  // TestObserver:
  void OnNavigationLearned(size_t url_visit_count,
                           const PageRequestSummary& summary) override {
    EXPECT_EQ(url_visit_count, url_visit_count_);
    EXPECT_EQ(summary.main_frame_url, summary_.main_frame_url);
    EXPECT_EQ(summary.initial_url, summary_.initial_url);
    // Duplicate resources can be observed in a single navigation but
    // ResourcePrefetchPredictor only cares about the first occurrence of each.
    std::vector<ResourcePrefetchPredictor::URLRequestSummary> subresources =
        GetUniqueSubresources(summary);
    EXPECT_THAT(subresources, testing::UnorderedElementsAreArray(
                                  summary_.subresource_requests));
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  size_t url_visit_count_;
  PageRequestSummary summary_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetchPredictorTestObserver);
};

class ResourcePrefetchPredictorBrowserTest : public InProcessBrowserTest {
 protected:
  using URLRequestSummary = ResourcePrefetchPredictor::URLRequestSummary;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kSpeculativeResourcePrefetching,
        switches::kSpeculativeResourcePrefetchingEnabled);
  }

  void SetUpOnMainThread() override {
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

  void NavigateToURLAndCheckSubresources(const GURL& main_frame_url) {
    GURL endpoint_url = GetRedirectEndpoint(main_frame_url);
    std::vector<URLRequestSummary> url_request_summaries;
    for (const auto& kv : resources_) {
      if (kv.second.is_no_store)
        continue;
      url_request_summaries.push_back(
          GetURLRequestSummaryForResource(endpoint_url, kv.second));
    }
    ResourcePrefetchPredictorTestObserver observer(
        predictor_, UpdateAndGetVisitCount(main_frame_url),
        CreatePageRequestSummary(endpoint_url.spec(), main_frame_url.spec(),
                                 url_request_summaries));
    ui_test_utils::NavigateToURL(browser(), main_frame_url);
    observer.Wait();
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
    int process_id = web_contents->GetRenderProcessHost()->GetID();
    int frame_id = web_contents->GetMainFrame()->GetRoutingID();
    summary.navigation_id =
        CreateNavigationID(process_id, frame_id, main_frame_url.spec());
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
    auto resource_it = resources_.find(request.GetURL());
    if (resource_it == resources_.end())
      return nullptr;

    const ResourceSummary& summary = resource_it->second;
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
};

IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest, LearningSimple) {
  // These resources have default priorities that correspond to
  // blink::typeToPriority function.
  AddResource(GetURL(kImagePath), content::RESOURCE_TYPE_IMAGE, net::LOWEST);
  AddResource(GetURL(kStylePath), content::RESOURCE_TYPE_STYLESHEET,
              net::HIGHEST);
  AddResource(GetURL(kScriptPath), content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  AddResource(GetURL(kFontPath), content::RESOURCE_TYPE_FONT_RESOURCE,
              net::HIGHEST);
  NavigateToURLAndCheckSubresources(GetURL(kHtmlSubresourcesPath));
}

IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest,
                       LearningAfterRedirect) {
  AddRedirectChain(GetURL(kRedirectPath), {{net::HTTP_MOVED_PERMANENTLY,
                                            GetURL(kHtmlSubresourcesPath)}});
  AddResource(GetURL(kImagePath), content::RESOURCE_TYPE_IMAGE, net::LOWEST);
  AddResource(GetURL(kStylePath), content::RESOURCE_TYPE_STYLESHEET,
              net::HIGHEST);
  AddResource(GetURL(kScriptPath), content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  AddResource(GetURL(kFontPath), content::RESOURCE_TYPE_FONT_RESOURCE,
              net::HIGHEST);
  NavigateToURLAndCheckSubresources(GetURL(kRedirectPath));
}

IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest,
                       LearningAfterRedirectChain) {
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
  NavigateToURLAndCheckSubresources(GetURL(kRedirectPath));
}

IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest,
                       LearningAfterHttpToHttpsRedirect) {
  EnableHttpsServer();
  AddRedirectChain(GetURL(kRedirectPath),
                   {{net::HTTP_FOUND, https_server()->GetURL(kRedirectPath2)},
                    {net::HTTP_MOVED_PERMANENTLY,
                     https_server()->GetURL(kHtmlSubresourcesPath)}});
  AddResource(https_server()->GetURL(kImagePath), content::RESOURCE_TYPE_IMAGE,
              net::LOWEST);
  AddResource(https_server()->GetURL(kStylePath),
              content::RESOURCE_TYPE_STYLESHEET, net::HIGHEST);
  AddResource(https_server()->GetURL(kScriptPath),
              content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  AddResource(https_server()->GetURL(kFontPath),
              content::RESOURCE_TYPE_FONT_RESOURCE, net::HIGHEST);
  NavigateToURLAndCheckSubresources(GetURL(kRedirectPath));
}

}  // namespace predictors
