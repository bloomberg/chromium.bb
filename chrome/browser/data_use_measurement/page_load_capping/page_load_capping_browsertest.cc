// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/simple_connection_listener.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"

namespace {
const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/data_use_measurement");
const char kImagePrefix[] = "/image";
}  // namespace

class PageLoadCappingBrowserTest : public InProcessBrowserTest {
 public:
  PageLoadCappingBrowserTest()
      : https_test_server_(std::make_unique<net::EmbeddedTestServer>(
            net::EmbeddedTestServer::TYPE_HTTPS)) {
    scoped_feature_list_.InitWithFeatures(
        {network::features::kRendererSideResourceScheduler,
         features::kResourceLoadScheduler},
        {});
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    // Check if this matches the image requests from the test suite.
    if (!StartsWith(request.relative_url, kImagePrefix,
                    base::CompareCase::SENSITIVE)) {
      return nullptr;
    }
    // This request should match "/image.*" for this test suite.
    images_attempted_++;

    // Return a 404. This is expected in the test, but embedded test server will
    // create warnings when serving its own 404 responses.
    std::unique_ptr<net::test_server::BasicHttpResponse> not_found_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    not_found_response->set_code(net::HTTP_NOT_FOUND);
    if (waiting_) {
      run_loop_->QuitWhenIdle();
      waiting_ = false;
    }
    return not_found_response;
  }

  void WaitForRequest() {
    EXPECT_FALSE(waiting_);
    waiting_ = true;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  size_t images_attempted() const { return images_attempted_; }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;

 private:
  size_t images_attempted_ = 0u;
  bool waiting_ = false;

  std::unique_ptr<base::RunLoop> run_loop_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest, PageLoadCappingBlocksLoads) {
  // Tests that subresource loading can be blocked from the browser process.
  https_test_server_->RegisterRequestHandler(base::BindRepeating(
      &PageLoadCappingBrowserTest::HandleRequest, base::Unretained(this)));
  https_test_server_->ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));

  ASSERT_TRUE(https_test_server_->Start());

  // Load a mostly empty page.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server_->GetURL("/page_capping.html"));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Block new subresource loads.
  std::vector<blink::mojom::PauseSubresourceLoadingHandlePtr> handles =
      contents->PauseSubresourceLoading();

  // Adds images to the page. They should not be allowed to load.
  // Running this 20 times makes 20 round trips to the renderer, making it very
  // likely the earliest request would have made it to the network by the time
  // all of the calls have been made.
  for (size_t i = 0; i < 20; ++i) {
    std::string create_image_script =
        std::string(
            "var image = document.createElement('img'); "
            "document.body.appendChild(image); image.src = '")
            .append(kImagePrefix)
            .append(base::IntToString(i))
            .append(".png';");
    EXPECT_TRUE(content::ExecuteScript(contents, create_image_script));
  }

  // No images should be loaded as subresource loading was paused.
  EXPECT_EQ(0u, images_attempted());
  https_test_server_.reset();
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest,
                       PageLoadCappingBlocksLoadsAndResume) {
  // Tests that after triggerring subresource pausing, resuming allows deferred
  // requests to be initiated.
  https_test_server_->RegisterRequestHandler(base::BindRepeating(
      &PageLoadCappingBrowserTest::HandleRequest, base::Unretained(this)));

  https_test_server_->ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));

  ASSERT_TRUE(https_test_server_->Start());

  // Load a mostly empty page.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server_->GetURL("/page_capping.html"));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Block new subresource loads.
  std::vector<blink::mojom::PauseSubresourceLoadingHandlePtr> handles =
      contents->PauseSubresourceLoading();

  // Adds an image to the page. It should not be allowed to load at first.
  // PageLoadCappingBlocksLoads tests that it is not loaded more robustly
  std::string create_image_script =
      std::string(
          "var image = document.createElement('img'); "
          "document.body.appendChild(image); image.src = '")
          .append(kImagePrefix)
          .append(".png';");
  ASSERT_TRUE(content::ExecuteScript(contents, create_image_script));

  // Previous image should be allowed to load now.
  handles.clear();

  // An image should be fetched because subresource loading was paused then
  // resumed.
  if (images_attempted() < 1u)
    WaitForRequest();
  EXPECT_EQ(1u, images_attempted());
  https_test_server_.reset();
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest, PageLoadCappingAllowLoads) {
  // Tests that the image request loads normally when the page has not been
  // paused.
  https_test_server_->ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  https_test_server_->RegisterRequestHandler(base::BindRepeating(
      &PageLoadCappingBrowserTest::HandleRequest, base::Unretained(this)));
  ASSERT_TRUE(https_test_server_->Start());

  // Load a mostly empty page.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server_->GetURL("/page_capping.html"));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Adds an image to the page. It should be allowed to load.
  std::string create_image_script =
      std::string(
          "var image = document.createElement('img'); "
          "document.body.appendChild(image); image.src = '")
          .append(kImagePrefix)
          .append(".png';");
  ASSERT_TRUE(content::ExecuteScript(contents, create_image_script));

  // An image should be fetched because subresource loading was never paused.
  if (images_attempted() < 1u)
    WaitForRequest();
  EXPECT_EQ(1u, images_attempted());
  https_test_server_.reset();
}
