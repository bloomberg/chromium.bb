// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "components/autofill_assistant/browser/web_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

class WebControllerBrowserTest : public content::ContentBrowserTest {
 public:
  WebControllerBrowserTest() {}
  ~WebControllerBrowserTest() override {}

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    http_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    http_server_->ServeFilesFromSourceDirectory(
        "components/test/data/autofill_assistant");
    ASSERT_TRUE(http_server_->Start());
    ASSERT_TRUE(NavigateToURL(
        shell(),
        http_server_->GetURL("/autofill_assistant_target_website.html")));
    web_controller_ = autofill_assistant::WebController::CreateForWebContents(
        shell()->web_contents());
  }

  void IsElementExists(const std::vector<std::string>& selectors,
                       bool expected_result) {
    base::RunLoop run_loop;
    web_controller_->ElementExists(
        selectors,
        base::BindOnce(&WebControllerBrowserTest::CheckElementExistCallback,
                       base::Unretained(this), run_loop.QuitClosure(),
                       expected_result));
    run_loop.Run();
  }

  void CheckElementExistCallback(const base::Closure& done_callback,
                                 bool expected_result,
                                 bool result) {
    ASSERT_EQ(expected_result, result);
    done_callback.Run();
  }

  void ClickElement(const std::vector<std::string>& selectors) {
    base::RunLoop run_loop;
    web_controller_->ClickElement(
        selectors,
        base::BindOnce(&WebControllerBrowserTest::ClickElementCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void ClickElementCallback(const base::Closure& done_callback,
                            bool result) {
    ASSERT_TRUE(result);
    done_callback.Run();
  }

  void WaitForElementRemove(const std::vector<std::string>& selectors) {
    base::RunLoop run_loop;
    web_controller_->ElementExists(
        selectors,
        base::BindOnce(
            &WebControllerBrowserTest::OnWaitForElementRemove,
            base::Unretained(this), run_loop.QuitClosure(), selectors));
    run_loop.Run();
  }

  void OnWaitForElementRemove(const base::Closure& done_callback,
                              const std::vector<std::string>& selectors,
                              bool result) {
    done_callback.Run();
    if (result) {
      WaitForElementRemove(selectors);
    }
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<autofill_assistant::WebController> web_controller_;

  DISALLOW_COPY_AND_ASSIGN(WebControllerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, IsElementExists) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#button");
  IsElementExists(selectors, true);
  selectors.emplace_back("#whatever");
  IsElementExists(selectors, false);

  // IFrame.
  selectors.clear();
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#button");
  IsElementExists(selectors, true);
  selectors.emplace_back("#whatever");
  IsElementExists(selectors, false);

  selectors.clear();
  selectors.emplace_back("#iframe");
  selectors.emplace_back("[name=name]");
  IsElementExists(selectors, true);

  // Shadow DOM.
  selectors.clear();
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#shadowsection");
  selectors.emplace_back("#shadowbutton");
  IsElementExists(selectors, true);
  selectors.emplace_back("#whatever");
  IsElementExists(selectors, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, ClickElement) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#button");
  ClickElement(selectors);

  WaitForElementRemove(selectors);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest,
                       ClickElementInIFrame) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#shadowsection");
  selectors.emplace_back("#shadowbutton");
  ClickElement(selectors);

  selectors.clear();
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#button");
  WaitForElementRemove(selectors);
}
}  // namespace
