// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/values.h"
#include "components/dom_distiller/content/distiller_page_web_contents.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "content/public/browser/browser_context.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::ContentBrowserTest;

namespace {
  // TODO(bengr): Once JavaScript has landed to extract article content from
  //              a loaded page, test the interaction of that script with
  //              DistillerPageWebContents.
  static const char kTitle[] = "Test Page Title";
  static const char kHtml[] =
      "<body>T<img src='http://t.com/t.jpg' id='0'></body>";
  static const char kImageUrl[] = "http://t.com/t.jpg";

  static const char kScript[] =
      " (function () {"
      "   var result = new Array(3);"
      "   result[0] = \"Test Page Title\";"
      "   result[1] = \"<body>T<img src='http://t.com/t.jpg' id='0'></body>\";"
      "   result[2] = \"http://t.com/t.jpg\";"
      "   return result;"
      " }())";
}  // namespace

namespace dom_distiller {

class DistillerPageWebContentsTest
  : public ContentBrowserTest,
    public DistillerPage::Delegate {
 public:
  void DistillPage(const base::Closure& quit_closure, const std::string& url) {
    quit_closure_ = quit_closure;
    distiller_page_->LoadURL(
        embedded_test_server()->GetURL(url));
  }

  virtual void OnLoadURLDone() OVERRIDE {
    distiller_page_->ExecuteJavaScript(kScript);
  }

  virtual void OnExecuteJavaScriptDone(const base::Value* value) OVERRIDE {
    value_ = value->DeepCopy();
    quit_closure_.Run();
  }

 protected:
  DistillerPageWebContents* distiller_page_;
  base::Closure quit_closure_;
  const base::Value* value_;
};

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, LoadPage) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  DistillerPageWebContents distiller_page(
      this, shell()->web_contents()->GetBrowserContext());
  distiller_page_ = &distiller_page;
  distiller_page.Init();
  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), "/simple_page.html");
  run_loop.Run();

  const base::ListValue* result_list = NULL;
  ASSERT_TRUE(value_->GetAsList(&result_list));
  ASSERT_EQ(3u, result_list->GetSize());
  std::string title;
  result_list->GetString(0, &title);
  ASSERT_EQ(kTitle, title);
  std::string html;
  result_list->GetString(1, &html);
  ASSERT_EQ(kHtml, html);
  std::string image_url;
  result_list->GetString(2, &image_url);
  ASSERT_EQ(kImageUrl, image_url);
}

}  // namespace dom_distiller
