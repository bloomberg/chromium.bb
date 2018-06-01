// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/simple_connection_listener.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"

namespace {
const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/data_use_measurement");
}  // namespace

class PageLoadCappingBrowserTest : public InProcessBrowserTest {
 protected:
  PageLoadCappingBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {network::features::kRendererSideResourceScheduler,
         features::kResourceLoadScheduler},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest, PageLoadCappingBlocksLoads) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  // The main resource and the favicon should be fetched. Additionally, images
  // will not be fetched after script is injected.
  net::test_server::SimpleConnectionListener listener(
      2, net::test_server::SimpleConnectionListener::
             FAIL_ON_ADDITIONAL_CONNECTIONS);

  https_test_server.SetConnectionListener(&listener);

  https_test_server.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));

  ASSERT_TRUE(https_test_server.Start());

  // Load a mostly empty page.
  ui_test_utils::NavigateToURL(browser(),
                               https_test_server.GetURL("/page_capping.html"));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Block new resource loads.
  std::vector<blink::mojom::PauseSubresourceLoadingHandlePtr> handles =
      contents->PauseSubresourceLoading();

  // Adds images to the page. They should not be allowed to load.
  // Running this 5 times makes 5 round trips to the renderer, making it very
  // likely the earlier requests would have started by the time all of the calls
  // have been made.
  for (size_t i = 0; i < 5; ++i) {
    std::string create_image_script =
        std::string(
            "var image = document.createElement('img'); "
            "document.body.appendChild(image); image.src = 'image")
            .append(base::IntToString(i))
            .append(".png';");
    EXPECT_TRUE(content::ExecuteScript(contents, create_image_script));
  }
  listener.WaitForConnections();
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest,
                       PageLoadCappingBlocksLoadsAndResume) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  // The main resource and the favicon should be fetched. Additionally, 5 images
  // will not be fetched after script is injected, but will once the page is
  // resumed.
  net::test_server::SimpleConnectionListener listener(
      7, net::test_server::SimpleConnectionListener::
             FAIL_ON_ADDITIONAL_CONNECTIONS);

  https_test_server.SetConnectionListener(&listener);

  https_test_server.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));

  ASSERT_TRUE(https_test_server.Start());

  // Load a mostly empty page.
  ui_test_utils::NavigateToURL(browser(),
                               https_test_server.GetURL("/page_capping.html"));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Block new resource loads.
  std::vector<blink::mojom::PauseSubresourceLoadingHandlePtr> handles =
      contents->PauseSubresourceLoading();

  // Adds images to the page. They should not be allowed to load.
  // Running this 5 times makes 5 round trips to the renderer, making it very
  // likely the earlier requests would have started by the time all of the calls
  // have been made.
  for (size_t i = 0; i < 5; ++i) {
    std::string create_image_script =
        std::string(
            "var image = document.createElement('img'); "
            "document.body.appendChild(image); image.src = 'image")
            .append(base::IntToString(i))
            .append(".png';");
    EXPECT_TRUE(content::ExecuteScript(contents, create_image_script));
  }

  handles.clear();

  // Wait for the request to start.
  listener.WaitForConnections();
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest, PageLoadCappingAllowLoads) {
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  // The main resource and the favicon should be fetched. Additionally, an image
  // will be fetched after script is injected.
  net::test_server::SimpleConnectionListener listener(
      7, net::test_server::SimpleConnectionListener::
             FAIL_ON_ADDITIONAL_CONNECTIONS);

  https_test_server.SetConnectionListener(&listener);

  https_test_server.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));

  ASSERT_TRUE(https_test_server.Start());

  // Load a mostly empty page.
  ui_test_utils::NavigateToURL(browser(),
                               https_test_server.GetURL("/page_capping.html"));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Adds images to the page. They should be allowed to load.
  // Running this 5 times makes 5 round trips to the renderer, making it very
  // likely the earlier requests would have started by the time all of the calls
  // have been made.
  for (size_t i = 0; i < 5; ++i) {
    std::string create_image_script =
        std::string(
            "var image = document.createElement('img'); "
            "document.body.appendChild(image); image.src = 'image")
            .append(base::IntToString(i))
            .append(".png';");
    EXPECT_TRUE(content::ExecuteScript(contents, create_image_script));
  }

  // Wait for the request to start.
  listener.WaitForConnections();
}
