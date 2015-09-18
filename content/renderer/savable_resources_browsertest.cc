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
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

class SavableResourcesTest : public ContentBrowserTest {
 public:
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
      const std::set<GURL>& expected_resources_set) {
    // Convert local file path to file URL.
    GURL file_url = net::FilePathToFileURL(page_file_path);
    // Load the test file.
    NavigateToURL(shell(), file_url);

    PostTaskToInProcessRendererAndWait(base::Bind(
        &SavableResourcesTest::CheckResources, base::Unretained(this),
        page_file_path, expected_resources_set, file_url,
        shell()->web_contents()->GetMainFrame()->GetRoutingID()));
  }

  void CheckResources(const base::FilePath& page_file_path,
                      const std::set<GURL>& expected_resources_set,
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

    // Check all links of sub-resource
    for (const auto& resource : resources_list) {
      ASSERT_TRUE(expected_resources_set.count(resource) != 0);
    }
  }
};

IN_PROC_BROWSER_TEST_F(SavableResourcesTest,
                       GetSavableResourceLinksWithPageHasValidStyleLink) {
  const char* expected_sub_resource_links[] = {
    "style.css"
  };

  const char* expected_frame_links[] = {
    "simple_linked_stylesheet.html",
  };

  // Add all expected links of sub-resource to expected set.
  std::set<GURL> expected_resources_set;
    const base::FilePath expected_resource_url =
        GetTestFilePath("dom_serializer", expected_sub_resource_links[0]);
    expected_resources_set.insert(
        net::FilePathToFileURL(expected_resource_url));

    //expected frame set
    const base::FilePath expected_frame_url =
        GetTestFilePath("dom_serializer", expected_frame_links[0]);
    expected_resources_set.insert(
        net::FilePathToFileURL(expected_frame_url));

  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "simple_linked_stylesheet.html");
  GetSavableResourceLinksForPage(page_file_path, expected_resources_set);
}

// Test function GetAllSavableResourceLinksForCurrentPage with a web page
// which has valid savable resource links.
IN_PROC_BROWSER_TEST_F(SavableResourcesTest,
                       GetSavableResourceLinksWithPageHasValidLinks) {
  std::set<GURL> expected_resources_set;

  const char* expected_sub_resource_links[] = {
    "file:///c:/yt/css/base_all-vfl36460.css",
    "file:///c:/yt/js/base_all_with_bidi-vfl36451.js",
    "file:///c:/yt/img/pixel-vfl73.gif"
  };
  const char* expected_frame_links[] = {
    "youtube_1.htm",
    "youtube_2.htm"
  };
  // Add all expected links of sub-resource to expected set.
  for (size_t i = 0; i < arraysize(expected_sub_resource_links); ++i)
    expected_resources_set.insert(GURL(expected_sub_resource_links[i]));
  // Add all expected links of frame to expected set.
  for (size_t i = 0; i < arraysize(expected_frame_links); ++i) {
    const base::FilePath expected_frame_url =
        GetTestFilePath("dom_serializer", expected_frame_links[i]);
    expected_resources_set.insert(
        net::FilePathToFileURL(expected_frame_url));
  }

  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_1.htm");
  GetSavableResourceLinksForPage(page_file_path, expected_resources_set);
}

// Test function GetAllSavableResourceLinksForCurrentPage with a web page
// which does not have valid savable resource links.
IN_PROC_BROWSER_TEST_F(SavableResourcesTest,
                       GetSavableResourceLinksWithPageHasInvalidLinks) {
  std::set<GURL> expected_resources_set;

  const char* expected_frame_links[] = {
    "youtube_2.htm"
  };
  // Add all expected links of frame to expected set.
  for (size_t i = 0; i < arraysize(expected_frame_links); ++i) {
    base::FilePath expected_frame_url =
        GetTestFilePath("dom_serializer", expected_frame_links[i]);
    expected_resources_set.insert(
        net::FilePathToFileURL(expected_frame_url));
  }

  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_2.htm");
  GetSavableResourceLinksForPage(page_file_path, expected_resources_set);
}

}  // namespace content
