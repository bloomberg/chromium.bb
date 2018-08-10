// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/previews/previews_lite_page_decider.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/navigation_handle.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const char kTestUrl[] = "https://www.test.com/";
}

class PreviewsLitePageNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  PreviewsLitePageNavigationThrottleTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    test_handle_ = content::NavigationHandle::CreateNavigationHandleForTesting(
        GURL(kTestUrl), main_rfh());

    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
  }

  void SimulateCommit() {
    test_handle_->CallDidCommitNavigationForTesting(test_handle_->GetURL());
  }

  void SimulateWillProcessResponse() {
    std::string headers("HTTP/1.1 200 OK\n\n");
    test_handle_->CallWillProcessResponseForTesting(
        main_rfh(),
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size()));
    SimulateCommit();
  }

  void CallDidFinishNavigation() { test_handle_.reset(); }

  content::NavigationHandle* handle() { return test_handle_.get(); }

  content::NavigationHandle* handle_with_url(std::string url) {
    test_handle_ = content::NavigationHandle::CreateNavigationHandleForTesting(
        GURL(url), main_rfh());
    return test_handle_.get();
  }

 private:
  std::unique_ptr<content::NavigationHandle> test_handle_;
};

TEST_F(PreviewsLitePageNavigationThrottleTest, TestGetPreviewsURL) {
  struct TestCase {
    std::string previews_host;
    std::string original_url;
    std::string previews_url;
  };
  const TestCase kTestCases[]{
      // Use https://play.golang.org/p/HUM2HxmUTOW to compute |previews_url|.
      {
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
      },
      {
          "https://previews.host.com",
          "http://original.host.com/path/path/path?query=yes",
          "https://6p7dar4ju6r4ynz7x3pucmlcltuqsf7z5auhvckzln7voglkt56q."
          "previews.host.com/p?u="
          "http%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
      },
      {
          "https://previews.host.com",
          "https://original.host.com:1443/path/path/path?query=yes",
          "https://mil6oxtqb4zpsbmutm4d7wrx5nlr6tzlxjp7y44u55zqhzsdzjpq."
          "previews.host.com/p?u=https%3A%2F%2Foriginal.host.com%3A1443"
          "%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
      },
      {
          "https://previews.host.com:1443",
          "http://original.host.com/path/path/path?query=yes",
          "https://6p7dar4ju6r4ynz7x3pucmlcltuqsf7z5auhvckzln7voglkt56q."
          "previews.host.com:1443/p?u="
          "http%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
      },
      {
          "https://previews.host.com:1443",
          "https://original.host.com:1443/path/path/path?query=yes",
          "https://mil6oxtqb4zpsbmutm4d7wrx5nlr6tzlxjp7y44u55zqhzsdzjpq."
          "previews.host.com:1443/p?u=https%3A%2F%2Foriginal.host.com%3A1443"
          "%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
      },
      {
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes#fragment",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes"
          "%23fragment",
      },
  };

  for (const TestCase& test_case : kTestCases) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        previews::features::kLitePageServerPreviews,
        {{"previews_host", test_case.previews_host}});

    std::unique_ptr<PreviewsLitePageDecider> decider =
        std::make_unique<PreviewsLitePageDecider>();
    std::unique_ptr<PreviewsLitePageNavigationThrottle> throttle =
        std::make_unique<PreviewsLitePageNavigationThrottle>(
            handle_with_url(test_case.original_url), decider.get());

    EXPECT_EQ(throttle->GetPreviewsURL(), test_case.previews_url);
  }

  // Boilerplate navigation to keep the test harness happy.
  SimulateWillProcessResponse();
  CallDidFinishNavigation();
}
