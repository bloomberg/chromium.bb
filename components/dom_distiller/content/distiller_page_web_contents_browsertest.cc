// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "components/dom_distiller/content/distiller_page_web_contents.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "grit/component_resources.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/resource/resource_bundle.h"

using content::ContentBrowserTest;
using testing::ContainsRegex;
using testing::HasSubstr;
using testing::Not;

namespace dom_distiller {

class DistillerPageWebContentsTest : public ContentBrowserTest {
 public:
  // ContentBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    AddComponentsResources();
    SetUpTestServer();
    ContentBrowserTest::SetUpOnMainThread();
  }

  void DistillPage(const base::Closure& quit_closure, const std::string& url) {
    quit_closure_ = quit_closure;
    distiller_page_->DistillPage(
        embedded_test_server()->GetURL(url),
        base::Bind(&DistillerPageWebContentsTest::OnPageDistillationFinished,
                   this));
  }

  void OnPageDistillationFinished(
    scoped_ptr<DistilledPageInfo> distilled_page,
    bool distillation_successful) {
    page_info_ = distilled_page.Pass();
    quit_closure_.Run();
  }

 private:
  void AddComponentsResources() {
    base::FilePath pak_file;
    base::FilePath pak_dir;
    PathService::Get(base::DIR_MODULE, &pak_dir);
    pak_file = pak_dir.Append(FILE_PATH_LITERAL("components_resources.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_file, ui::SCALE_FACTOR_NONE);
  }

   void SetUpTestServer() {
     base::FilePath path;
     PathService::Get(base::DIR_SOURCE_ROOT, &path);
     path = path.AppendASCII("components/test/data/dom_distiller");
     embedded_test_server()->ServeFilesFromDirectory(path);
     ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
   }

 protected:
  DistillerPageWebContents* distiller_page_;
  base::Closure quit_closure_;
  scoped_ptr<DistilledPageInfo> page_info_;
};

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, BasicDistillationWorks) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext());
  distiller_page_ = &distiller_page;
  distiller_page.Init();

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), "/simple_article.html");
  run_loop.Run();

  EXPECT_EQ("Test Page Title", page_info_.get()->title);
  EXPECT_THAT(page_info_.get()->html, HasSubstr("Lorem ipsum"));
  EXPECT_THAT(page_info_.get()->html, Not(HasSubstr("questionable content")));
  EXPECT_EQ("", page_info_.get()->next_page_url);
  EXPECT_EQ("", page_info_.get()->prev_page_url);
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, HandlesRelativeLinks) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext());
  distiller_page_ = &distiller_page;
  distiller_page.Init();

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), "/simple_article.html");
  run_loop.Run();

  // A relative link should've been updated.
  EXPECT_THAT(page_info_.get()->html,
              ContainsRegex("href=\"http://127.0.0.1:.*/relativelink.html\""));
  EXPECT_THAT(page_info_.get()->html,
              HasSubstr("href=\"http://www.google.com/absolutelink.html\""));
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, HandlesRelativeImages) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext());
  distiller_page_ = &distiller_page;
  distiller_page.Init();

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), "/simple_article.html");
  run_loop.Run();

  // A relative link should've been updated.
  EXPECT_THAT(page_info_.get()->html,
              ContainsRegex("src=\"http://127.0.0.1:.*/relativeimage.png\""));
  EXPECT_THAT(page_info_.get()->html,
              HasSubstr("src=\"http://www.google.com/absoluteimage.png\""));
}

}  // namespace dom_distiller
