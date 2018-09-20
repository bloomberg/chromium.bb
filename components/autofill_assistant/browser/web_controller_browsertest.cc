// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "components/autofill_assistant/browser/web_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace autofill_assistant {

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
    web_controller_ =
        WebController::CreateForWebContents(shell()->web_contents());
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

  void FocusElement(const std::vector<std::string>& selectors) {
    base::RunLoop run_loop;
    web_controller_->FocusElement(
        selectors,
        base::BindOnce(&WebControllerBrowserTest::OnFocusElement,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnFocusElement(base::Closure done_callback, bool result) {
    ASSERT_TRUE(result);
    std::move(done_callback).Run();
  }

  void SelectOption(const std::vector<std::string>& selectors,
                    const std::string& option) {
    base::RunLoop run_loop;
    web_controller_->SelectOption(
        selectors, option,
        base::BindOnce(&WebControllerBrowserTest::OnSelectOption,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnSelectOption(base::Closure done_callback, bool result) {
    ASSERT_TRUE(result);
    std::move(done_callback).Run();
  }

  void FindElement(const std::vector<std::string>& selectors,
                   size_t expected_index,
                   bool is_main_frame) {
    base::RunLoop run_loop;
    web_controller_->FindElement(
        selectors,
        base::BindOnce(&WebControllerBrowserTest::OnFindElement,
                       base::Unretained(this), run_loop.QuitClosure(),
                       expected_index, is_main_frame));
    run_loop.Run();
  }

  void OnFindElement(const base::Closure& done_callback,
                     size_t expected_index,
                     bool is_main_frame,
                     std::unique_ptr<WebController::FindElementResult> result) {
    done_callback.Run();
    if (is_main_frame) {
      ASSERT_EQ(shell()->web_contents()->GetMainFrame(),
                result->container_frame_host);
    } else {
      ASSERT_NE(shell()->web_contents()->GetMainFrame(),
                result->container_frame_host);
    }
    ASSERT_EQ(result->container_frame_selector_index, expected_index);
    ASSERT_FALSE(result->object_id.empty());
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<WebController> web_controller_;

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

  // IFrame inside IFrame.
  selectors.clear();
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#button");
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

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FindElement) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#button");
  FindElement(selectors, 0, true);

  // IFrame.
  selectors.clear();
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#button");
  FindElement(selectors, 0, false);

  selectors.clear();
  selectors.emplace_back("#iframe");
  selectors.emplace_back("[name=name]");
  FindElement(selectors, 0, false);

  // IFrame inside IFrame.
  selectors.clear();
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#button");
  FindElement(selectors, 1, false);
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, FocusElement) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#iframe");
  selectors.emplace_back("#focus");

  // The element is not visible initially.
  const std::string checkNotVisibleScript = R"(
      let iframe = document.querySelector("#iframe");
      let div = iframe.contentDocument.querySelector("#focus");
      let iframeRect = iframe.getBoundingClientRect();
      let divRect = div.getBoundingClientRect();
      iframeRect.y + divRect.y > window.innerHeight;
  )";
  EXPECT_EQ(true, content::EvalJs(shell(), checkNotVisibleScript));
  FocusElement(selectors);

  // Verify that the scroll moved the div in the iframe into view.
  const std::string checkVisibleScript = R"(
    const scrollTimeoutMs = 500;
    var timer = null;

    function check() {
      let iframe = document.querySelector("#iframe");
      let div = iframe.contentDocument.querySelector("#focus");
      let iframeRect = iframe.getBoundingClientRect();
      let divRect = div.getBoundingClientRect();
      return iframeRect.y + divRect.y < window.innerHeight;
    }
    function onScrollDone() {
      window.removeEventListener("scroll", onScroll);
      domAutomationController.send(check());
    }
    function onScroll(e) {
      if (timer != null) {
        clearTimeout(timer);
      }
      timer = setTimeout(onScrollDone, scrollTimeoutMs);
    }
    if (check()) {
      // Scrolling finished before this script started. Just return the result.
      domAutomationController.send(true);
    } else {
      window.addEventListener("scroll", onScroll);
    }
  )";
  EXPECT_EQ(true, content::EvalJsWithManualReply(shell(), checkVisibleScript));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SelectOption) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#select");
  SelectOption(selectors, "two");

  const std::string javascript = R"(
    let select = document.querySelector("#select");
    select.options[select.selectedIndex].label;
  )";
  EXPECT_EQ("Two", content::EvalJs(shell(), javascript));

  SelectOption(selectors, "one");
  EXPECT_EQ("One", content::EvalJs(shell(), javascript));
}

IN_PROC_BROWSER_TEST_F(WebControllerBrowserTest, SelectOptionInIframe) {
  std::vector<std::string> selectors;
  selectors.emplace_back("#iframe");
  selectors.emplace_back("select[name=state]");
  SelectOption(selectors, "NY");

  const std::string javascript = R"(
    let iframe = document.querySelector("iframe").contentDocument;
    let select = iframe.querySelector("select[name=state]");
    select.options[select.selectedIndex].label;
  )";
  EXPECT_EQ("NY", content::EvalJs(shell(), javascript));
}

}  // namespace
