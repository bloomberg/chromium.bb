// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/renovations/page_renovator.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/offline_pages/content/renovations/render_frame_script_injector.h"
#include "components/offline_pages/core/renovations/page_renovation_loader.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {

const char kDocRoot[] = "components/test/data/offline_pages";
const char kTestPageURL[] = "/renovator_test_page.html";
const char kTestRenovationScript[] =
    R"*(function foo() {
      var node = document.getElementById('foo');
      node.innerHTML = 'Good';
    }
    function bar() {
      var node = document.getElementById('bar');
      node.parentNode.removeChild(node);
    }
    function always() {
      var node = document.getElementById('always');
      node.parentNode.removeChild(node);
    }
    var map_renovations = {'foo':foo, 'bar':bar, 'always':always};
    function run_renovations(idlist) {
      for (var id of idlist) {
        map_renovations[id]();
      }
    })*";

// Scripts to check whether each renovation ran in the test page.
const char kCheckFooScript[] =
    "document.getElementById('foo').innerHTML == 'Good'";
const char kCheckBarScript[] = "document.getElementById('bar') == null";
const char kCheckAlwaysScript[] = "document.getElementById('always') == null";

// Renovation that should only run on pages from URL foo.bar
class FooPageRenovation : public PageRenovation {
 public:
  bool ShouldRun(const GURL& url) const override {
    return url.host() == "foo.bar";
  }
  std::string GetID() const override { return "foo"; }
};

// Renovation that should only run on pages from URL bar.qux
class BarPageRenovation : public PageRenovation {
 public:
  bool ShouldRun(const GURL& url) const override {
    return url.host() == "bar.qux";
  }
  std::string GetID() const override { return "bar"; }
};

// Renovation that should run on every page.
class AlwaysRenovation : public PageRenovation {
 public:
  bool ShouldRun(const GURL& url) const override { return true; }
  std::string GetID() const override { return "always"; }
};

}  // namespace

class PageRenovatorBrowserTest : public content::ContentBrowserTest {
 public:
  void SetUpOnMainThread() override;

  void QuitRunLoop();

 protected:
  net::EmbeddedTestServer test_server_;
  content::RenderFrameHost* render_frame_;
  std::unique_ptr<PageRenovationLoader> page_renovation_loader_;
  std::unique_ptr<PageRenovator> page_renovator_;

  std::unique_ptr<base::RunLoop> run_loop_;
};

void PageRenovatorBrowserTest::SetUpOnMainThread() {
  // Serve our test HTML.
  test_server_.ServeFilesFromSourceDirectory(kDocRoot);
  ASSERT_TRUE(test_server_.Start());

  // Navigate to test page.
  GURL url = test_server_.GetURL(kTestPageURL);
  content::NavigateToURL(shell(), url);
  render_frame_ = shell()->web_contents()->GetMainFrame();

  // Initialize renovations.
  std::vector<std::unique_ptr<PageRenovation>> renovations;
  renovations.push_back(base::MakeUnique<FooPageRenovation>());
  renovations.push_back(base::MakeUnique<BarPageRenovation>());
  renovations.push_back(base::MakeUnique<AlwaysRenovation>());

  page_renovation_loader_.reset(new PageRenovationLoader);
  page_renovation_loader_->SetSourceForTest(
      base::ASCIIToUTF16(kTestRenovationScript));
  page_renovation_loader_->SetRenovationsForTest(std::move(renovations));

  auto script_injector = base::MakeUnique<RenderFrameScriptInjector>(
      render_frame_, content::ISOLATED_WORLD_ID_CONTENT_END);
  GURL fake_url("http://foo.bar/");
  page_renovator_.reset(new PageRenovator(
      page_renovation_loader_.get(), std::move(script_injector), fake_url));

  run_loop_.reset(new base::RunLoop);
}

void PageRenovatorBrowserTest::QuitRunLoop() {
  base::Closure quit_task =
      content::GetDeferredQuitTaskForRunLoop(run_loop_.get());
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   quit_task);
}

IN_PROC_BROWSER_TEST_F(PageRenovatorBrowserTest, CorrectRenovationsRun) {
  // This should run FooPageRenovation and AlwaysRenovation, but not
  // BarPageRenovation.
  page_renovator_->RunRenovations(base::Bind(
      &PageRenovatorBrowserTest::QuitRunLoop, base::Unretained(this)));
  content::RunThisRunLoop(run_loop_.get());

  // Check that correct modifications were made to the page.
  std::unique_ptr<base::Value> fooResult =
      content::ExecuteScriptAndGetValue(render_frame_, kCheckFooScript);
  std::unique_ptr<base::Value> barResult =
      content::ExecuteScriptAndGetValue(render_frame_, kCheckBarScript);
  std::unique_ptr<base::Value> alwaysResult =
      content::ExecuteScriptAndGetValue(render_frame_, kCheckAlwaysScript);

  ASSERT_TRUE(fooResult.get() != nullptr);
  ASSERT_TRUE(barResult.get() != nullptr);
  ASSERT_TRUE(alwaysResult.get() != nullptr);
  EXPECT_TRUE(fooResult->GetBool());
  EXPECT_FALSE(barResult->GetBool());
  EXPECT_TRUE(alwaysResult->GetBool());
}

// TODO(collinbaker): add test for Wikipedia renovation here.

}  // namespace offline_pages
