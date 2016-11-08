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

static const char kImageUrl[] = "/predictors/image.png";
static const char kStyleUrl[] = "/predictors/style.css";
static const char kScriptUrl[] = "/predictors/script.js";
static const char kFontUrl[] = "/predictors/font.ttf";
static const char kHtmlSubresourcesUrl[] = "/predictors/html_subresources.html";

struct ResourceSummary {
  ResourceSummary() : is_no_store(false), version(0) {}

  ResourcePrefetchPredictor::URLRequestSummary request;
  std::string content;
  bool is_no_store;
  size_t version;
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
    EXPECT_THAT(
        summary.subresource_requests,
        testing::UnorderedElementsAreArray(summary_.subresource_requests));
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
 public:
  using URLRequestSummary = ResourcePrefetchPredictor::URLRequestSummary;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kSpeculativeResourcePrefetching,
        switches::kSpeculativeResourcePrefetchingEnabled);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&ResourcePrefetchPredictorBrowserTest::HandleRequest,
                   base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
    predictor_ =
        ResourcePrefetchPredictorFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(predictor_);
    EnsurePredictorInitialized();
  }

  void NavigateToURLAndCheckSubresources(
      const std::string& main_frame_relative) {
    GURL main_frame_absolute =
        embedded_test_server()->GetURL(main_frame_relative);
    std::vector<URLRequestSummary> url_request_summaries;
    for (const auto& kv : resources_) {
      if (kv.second.is_no_store)
        continue;
      url_request_summaries.push_back(
          GetURLRequestSummaryForResource(main_frame_absolute, kv.second));
    }
    ResourcePrefetchPredictorTestObserver observer(
        predictor_, UpdateAndGetVisitCount(main_frame_relative),
        CreatePageRequestSummary(main_frame_absolute.spec(),
                                 main_frame_absolute.spec(),
                                 url_request_summaries));
    ui_test_utils::NavigateToURL(browser(), main_frame_absolute);
    observer.Wait();
  }

  ResourceSummary* AddResource(const std::string& relative_url,
                               content::ResourceType resource_type,
                               net::RequestPriority priority) {
    ResourceSummary resource;
    resource.request.resource_url =
        embedded_test_server()->GetURL(relative_url);
    resource.request.resource_type = resource_type;
    resource.request.priority = priority;
    auto result = resources_.insert(std::make_pair(relative_url, resource));
    return &(result.first->second);
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
      const ResourceSummary& resource_summary) {
    URLRequestSummary summary(resource_summary.request);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    int process_id = web_contents->GetRenderProcessHost()->GetID();
    int frame_id = web_contents->GetMainFrame()->GetRoutingID();
    summary.navigation_id =
        CreateNavigationID(process_id, frame_id, main_frame_url.spec());
    return summary;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto resource_it = resources_.find(request.relative_url);
    if (resource_it == resources_.end())
      return nullptr;

    const ResourceSummary& summary = resource_it->second;
    auto http_response =
        base::MakeUnique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    if (!summary.request.mime_type.empty())
      http_response->set_content_type(summary.request.mime_type);
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

  size_t UpdateAndGetVisitCount(const std::string& main_frame_relative) {
    return ++visit_count_[main_frame_relative];
  }

  ResourcePrefetchPredictor* predictor_;
  std::map<std::string, ResourceSummary> resources_;
  std::map<std::string, size_t> visit_count_;
};

IN_PROC_BROWSER_TEST_F(ResourcePrefetchPredictorBrowserTest, LearningSimple) {
  // These resources have default priorities that correspond to
  // blink::typeToPriority function.
  AddResource(kImageUrl, content::RESOURCE_TYPE_IMAGE, net::LOWEST);
  AddResource(kStyleUrl, content::RESOURCE_TYPE_STYLESHEET, net::HIGHEST);
  AddResource(kScriptUrl, content::RESOURCE_TYPE_SCRIPT, net::MEDIUM);
  AddResource(kFontUrl, content::RESOURCE_TYPE_FONT_RESOURCE, net::HIGHEST);
  NavigateToURLAndCheckSubresources(kHtmlSubresourcesUrl);
}

}  // namespace predictors
