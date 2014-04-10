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

class DistillerPageWebContentsTest
  : public ContentBrowserTest,
    public DistillerPage::Delegate {
 public:
  // ContentBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    AddComponentsResources();
    SetUpTestServer();
    ContentBrowserTest::SetUpOnMainThread();
  }

  void DistillPage(const base::Closure& quit_closure, const std::string& url) {
    quit_closure_ = quit_closure;
    distiller_page_->LoadURL(embedded_test_server()->GetURL(url));
  }

  virtual void OnLoadURLDone() OVERRIDE {
    std::string script = ResourceBundle::GetSharedInstance().
        GetRawDataResource(IDR_DISTILLER_JS).as_string();
    distiller_page_->ExecuteJavaScript(script);
  }

  virtual void OnExecuteJavaScriptDone(const GURL& page_url,
                                       const base::Value* value) OVERRIDE {
    value_ = value->DeepCopy();
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
  const base::Value* value_;
};

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, BasicDistillationWorks) {
  base::WeakPtrFactory<DistillerPage::Delegate> weak_factory(this);
  DistillerPageWebContents distiller_page(
      weak_factory.GetWeakPtr(), shell()->web_contents()->GetBrowserContext());
  distiller_page_ = &distiller_page;
  distiller_page.Init();

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), "/simple_article.html");
  run_loop.Run();

  const base::ListValue* result_list = NULL;
  ASSERT_TRUE(value_->GetAsList(&result_list));
  ASSERT_EQ(4u, result_list->GetSize());
  std::string title;
  result_list->GetString(0, &title);
  EXPECT_EQ("Test Page Title", title);
  std::string html;
  result_list->GetString(1, &html);
  EXPECT_THAT(html, HasSubstr("Lorem ipsum"));
  EXPECT_THAT(html, Not(HasSubstr("questionable content")));
  std::string next_page_url;
  result_list->GetString(2, &next_page_url);
  EXPECT_EQ("", next_page_url);
  std::string unused;
  result_list->GetString(3, &unused);
  EXPECT_EQ("", unused);
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, HandlesRelativeLinks) {
  base::WeakPtrFactory<DistillerPage::Delegate> weak_factory(this);
  DistillerPageWebContents distiller_page(
      weak_factory.GetWeakPtr(), shell()->web_contents()->GetBrowserContext());
  distiller_page_ = &distiller_page;
  distiller_page.Init();

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), "/simple_article.html");
  run_loop.Run();

  const base::ListValue* result_list = NULL;
  ASSERT_TRUE(value_->GetAsList(&result_list));
  std::string html;
  result_list->GetString(1, &html);
  // A relative link should've been updated.
  EXPECT_THAT(html,
              ContainsRegex("href=\"http://127.0.0.1:.*/relativelink.html\""));
  EXPECT_THAT(html,
              HasSubstr("href=\"http://www.google.com/absolutelink.html\""));
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, HandlesRelativeImages) {
  base::WeakPtrFactory<DistillerPage::Delegate> weak_factory(this);
  DistillerPageWebContents distiller_page(
      weak_factory.GetWeakPtr(), shell()->web_contents()->GetBrowserContext());
  distiller_page_ = &distiller_page;
  distiller_page.Init();

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), "/simple_article.html");
  run_loop.Run();

  const base::ListValue* result_list = NULL;
  ASSERT_TRUE(value_->GetAsList(&result_list));
  std::string html;
  result_list->GetString(1, &html);
  // A relative link should've been updated.
  EXPECT_THAT(html,
              ContainsRegex("src=\"http://127.0.0.1:.*/relativeimage.png\""));
  EXPECT_THAT(html,
              HasSubstr("src=\"http://www.google.com/absoluteimage.png\""));
}

}  // namespace dom_distiller
