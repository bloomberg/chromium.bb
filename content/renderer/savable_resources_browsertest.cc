// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/renderer/savable_resources.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

class SavableResourcesTest : public ContentBrowserTest {
 public:
  using UrlVectorMatcher = testing::Matcher<std::vector<GURL>>;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
#if defined(OS_WIN)
    // Don't want to try to create a GPU process.
    command_line->AppendSwitch(switches::kDisableGpu);
#endif
  }

  // Test function GetAllSavableResourceLinksForCurrentPage with a web page.
  // We expect result of GetAllSavableResourceLinksForCurrentPage exactly
  // matches expected_resources_set.
  void GetSavableResourceLinksForPage(
      const base::FilePath& page_file_path,
      const UrlVectorMatcher& expected_resources_matcher) {
    // Convert local file path to file URL.
    GURL file_url = net::FilePathToFileURL(page_file_path);
    // Load the test file.
    NavigateToURL(shell(), file_url);

    PostTaskToInProcessRendererAndWait(base::Bind(
        &SavableResourcesTest::CheckResources, base::Unretained(this),
        page_file_path, expected_resources_matcher, file_url,
        shell()->web_contents()->GetMainFrame()->GetRoutingID()));
  }

  void CheckResources(const base::FilePath& page_file_path,
                      const UrlVectorMatcher& expected_resources_matcher,
                      const GURL& file_url,
                      int render_frame_routing_id) {
    // Get all savable resource links for the page.
    std::vector<GURL> resources_list;
    std::vector<GURL> referrer_urls_list;
    std::vector<blink::WebReferrerPolicy> referrer_policies_list;
    SavableResourcesResult result(&resources_list, &referrer_urls_list,
                                  &referrer_policies_list);

    const char* savable_schemes[] = {
      "http",
      "https",
      "file",
      NULL
    };

    RenderFrame* render_frame =
        RenderFrame::FromRoutingID(render_frame_routing_id);

    ASSERT_TRUE(GetSavableResourceLinksForFrame(
        render_frame->GetWebFrame(),
        &result, savable_schemes));

    EXPECT_EQ(resources_list.size(), referrer_urls_list.size());
    EXPECT_EQ(resources_list.size(), referrer_policies_list.size());
    EXPECT_THAT(resources_list, expected_resources_matcher);
  }
};

IN_PROC_BROWSER_TEST_F(SavableResourcesTest,
                       GetSavableResourceLinksWithPageHasValidStyleLink) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "simple_linked_stylesheet.html");

  auto expected_subresources_matcher = testing::UnorderedElementsAre(
      net::FilePathToFileURL(GetTestFilePath("dom_serializer", "style.css")));

  GetSavableResourceLinksForPage(page_file_path, expected_subresources_matcher);
}

// Test function GetAllSavableResourceLinksForCurrentPage with a web page
// which has valid savable resource links.
IN_PROC_BROWSER_TEST_F(SavableResourcesTest,
                       GetSavableResourceLinksWithPageHasValidLinks) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_1.htm");

  auto expected_subresources_matcher = testing::UnorderedElementsAre(
      GURL("file:///c:/yt/css/base_all-vfl36460.css"),
      GURL("file:///c:/yt/js/base_all_with_bidi-vfl36451.js"),
      GURL("file:///c:/yt/img/pixel-vfl73.gif"));

  GetSavableResourceLinksForPage(page_file_path, expected_subresources_matcher);
}

// Test function GetAllSavableResourceLinksForCurrentPage with a web page
// which does not have valid savable resource links.
IN_PROC_BROWSER_TEST_F(SavableResourcesTest,
                       GetSavableResourceLinksWithPageHasInvalidLinks) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_2.htm");

  auto expected_subresources_matcher = testing::IsEmpty();
  GetSavableResourceLinksForPage(page_file_path, expected_subresources_matcher);
}

}  // namespace content
