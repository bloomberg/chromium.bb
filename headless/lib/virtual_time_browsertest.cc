// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test.h"
#include "headless/public/devtools/domains/emulation.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/util/testing/test_in_memory_protocol_handler.h"
#include "headless/test/headless_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;
using testing::Contains;

namespace headless {

class VirtualTimeBrowserTest : public HeadlessAsyncDevTooledBrowserTest,
                               public emulation::ExperimentalObserver,
                               public runtime::Observer {
 public:
  void SetInitialURL(const std::string& initial_url) {
    initial_url_ = initial_url;
  }

  void RunDevTooledTest() override {
    devtools_client_->GetEmulation()->GetExperimental()->AddObserver(this);
    devtools_client_->GetRuntime()->AddObserver(this);
    devtools_client_->GetRuntime()->Enable(base::BindRepeating(
        &VirtualTimeBrowserTest::RuntimeEnabled, base::Unretained(this)));
  }


  void RuntimeEnabled() {
    runtime_enabled = true;
    MaybeSetVirtualTimePolicy();
  }

  virtual void MaybeSetVirtualTimePolicy() {
    if (!runtime_enabled)
      return;

    // To avoid race conditions start with virtual time paused.
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(emulation::VirtualTimePolicy::PAUSE)
            .Build(),
        base::BindRepeating(&VirtualTimeBrowserTest::SetVirtualTimePolicyDone,
                            base::Unretained(this)));

    SetAfterLoadVirtualTimePolicy();
  }

  void SetVirtualTimePolicyDone(
      std::unique_ptr<emulation::SetVirtualTimePolicyResult> result) {
    EXPECT_LT(0, result->GetVirtualTimeBase());
    // Virtual time is paused, so start navigating.
    devtools_client_->GetPage()->Navigate(initial_url_);
  }

  virtual void SetAfterLoadVirtualTimePolicy() {
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(
                emulation::VirtualTimePolicy::PAUSE_IF_NETWORK_FETCHES_PENDING)
            .SetBudget(5000)
            .SetWaitForNavigation(true)
            .Build());
  }

  // runtime::Observer implementation:
  void OnConsoleAPICalled(
      const runtime::ConsoleAPICalledParams& params) override {
    // We expect the arguments always to be a single string.
    const std::vector<std::unique_ptr<runtime::RemoteObject>>& args =
        *params.GetArgs();
    if (args.size() == 1u && args[0]->HasValue())
      log_.push_back(args[0]->GetValue()->GetString());
  }

  std::string initial_url_;
  std::vector<std::string> log_;
  bool initial_load_seen_ = false;
  bool runtime_enabled = false;
};

class VirtualTimeObserverTest : public VirtualTimeBrowserTest {
 public:
  VirtualTimeObserverTest() {
    EXPECT_TRUE(embedded_test_server()->Start());
    SetInitialURL(
        embedded_test_server()->GetURL("/virtual_time_test.html").spec());
  }

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    std::vector<std::string> expected_log = {"Paused @ 0ms",
                                             "Advanced to 10ms",
                                             "step1",
                                             "Advanced to 110ms",
                                             "step2",
                                             "Paused @ 110ms",
                                             "Advanced to 120ms",
                                             "Advanced to 210ms",
                                             "step3",
                                             "Paused @ 210ms",
                                             "Advanced to 220ms",
                                             "Advanced to 310ms",
                                             "step4",
                                             "pass"};
    // Note after the PASS step there are a number of virtual time advances, but
    // this list seems to be non-deterministic because there's all sorts of
    // timers in the system.  We don't really care about that here.
    ASSERT_GE(log_.size(), expected_log.size());
    for (size_t i = 0; i < expected_log.size(); i++) {
      EXPECT_EQ(expected_log[i], log_[i]) << "At index " << i;
    }
    EXPECT_THAT(log_, Contains("Advanced to 5000ms"));
    EXPECT_THAT(log_, Contains("Paused @ 5000ms"));
    FinishAsynchronousTest();
  }

  void OnVirtualTimeAdvanced(
      const emulation::VirtualTimeAdvancedParams& params) override {
    log_.push_back(base::StringPrintf("Advanced to %.fms",
                                      params.GetVirtualTimeElapsed()));
  }

  void OnVirtualTimePaused(
      const emulation::VirtualTimePausedParams& params) override {
    log_.push_back(
        base::StringPrintf("Paused @ %.fms", params.GetVirtualTimeElapsed()));
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(VirtualTimeObserverTest);

class MaxVirtualTimeTaskStarvationCountTest : public VirtualTimeBrowserTest {
 public:
  MaxVirtualTimeTaskStarvationCountTest() {
    EXPECT_TRUE(embedded_test_server()->Start());
    SetInitialURL(embedded_test_server()
                      ->GetURL("/virtual_time_starvation_test.html")
                      .spec());
  }

  void SetAfterLoadVirtualTimePolicy() override {
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(
                emulation::VirtualTimePolicy::PAUSE_IF_NETWORK_FETCHES_PENDING)
            .SetBudget(4001)
            .SetMaxVirtualTimeTaskStarvationCount(100)
            .SetWaitForNavigation(true)
            .Build());
  }

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    // If SetMaxVirtualTimeTaskStarvationCount was not set, this callback would
    // never fire.
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(MaxVirtualTimeTaskStarvationCountTest);

namespace {
static constexpr char kIndexHtml[] = R"(
<html>
<body>
<iframe src="/iframe.html" width="400" height="200" id="iframe1">
</iframe>
</body>
</html>
)";

static constexpr char kIFrame[] = R"(
<html>
<head>
<link rel="stylesheet" type="text/css" href="style.css">
</head>
<body>
<h1>Hello from the iframe!</h1>
</body>
</html>
)";

static constexpr char kCss[] = R"(
.test {
  color: blue;
}
)";

}  // namespace

class FrameDetatchWithPendingResourceLoadVirtualTimeTest
    : public VirtualTimeBrowserTest,
      public TestInMemoryProtocolHandler::RequestDeferrer {
 public:
  FrameDetatchWithPendingResourceLoadVirtualTimeTest() {
    SetInitialURL("http://test.com/index.html");
  }

  ProtocolHandlerMap GetProtocolHandlers() override {
    ProtocolHandlerMap protocol_handlers;
    std::unique_ptr<TestInMemoryProtocolHandler> http_handler(
        new TestInMemoryProtocolHandler(browser()->BrowserIOThread(), this));
    http_handler_ = http_handler.get();
    http_handler->InsertResponse("http://test.com/index.html",
                                 {kIndexHtml, "text/html"});
    http_handler->InsertResponse("http://test.com/iframe.html",
                                 {kIFrame, "text/html"});
    http_handler->InsertResponse("http://test.com/style.css",
                                 {kCss, "text/css"});
    protocol_handlers[url::kHttpScheme] = std::move(http_handler);
    return protocol_handlers;
  }

  void RunDevTooledTest() override {
    http_handler_->SetHeadlessBrowserContext(browser_context_);
    VirtualTimeBrowserTest::RunDevTooledTest();
  }

  void OnRequest(const GURL& url, base::Closure complete_request) override {
    // Note this is called on the IO thread.
    if (url.spec() == "http://test.com/style.css") {
      // Detach the iframe but leave the css resource fetch hanging.
      browser()->BrowserMainThread()->PostTask(
          FROM_HERE, base::BindRepeating(
                         &FrameDetatchWithPendingResourceLoadVirtualTimeTest::
                             DetatchIFrame,
                         base::Unretained(this)));
    } else {
      complete_request.Run();
    }
  }

  void DetatchIFrame() {
    devtools_client_->GetRuntime()->Evaluate(
        "let elem = document.getElementById('iframe1');\n"
        "elem.parentNode.removeChild(elem);");
  }

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    EXPECT_THAT(
        http_handler_->urls_requested(),
        ElementsAre("http://test.com/index.html", "http://test.com/iframe.html",
                    "http://test.com/style.css"));

    // Virtual time should still expire, despite the CSS resource load not
    // finishing.
    FinishAsynchronousTest();
  }

  TestInMemoryProtocolHandler* http_handler_;  // NOT OWNED
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(
    FrameDetatchWithPendingResourceLoadVirtualTimeTest);

class VirtualTimeLocalStorageTest : public VirtualTimeBrowserTest {
 public:
  VirtualTimeLocalStorageTest() {
    EXPECT_TRUE(embedded_test_server()->Start());
    SetInitialURL(embedded_test_server()
                      ->GetURL("/virtual_time_local_storage.html")
                      .spec());
  }

  void SetAfterLoadVirtualTimePolicy() override {
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(
                emulation::VirtualTimePolicy::PAUSE_IF_NETWORK_FETCHES_PENDING)
            .SetBudget(5000)
            .SetWaitForNavigation(true)
            .Build());
  }

  void OnConsoleAPICalled(
      const runtime::ConsoleAPICalledParams& params) override {
    EXPECT_EQ(runtime::ConsoleAPICalledType::LOG, params.GetType());
    ASSERT_EQ(1u, params.GetArgs()->size());
    ASSERT_EQ(runtime::RemoteObjectType::STRING,
              (*params.GetArgs())[0]->GetType());
    std::string count_string = (*params.GetArgs())[0]->GetValue()->GetString();
    int count;
    ASSERT_TRUE(base::StringToInt(count_string, &count)) << count_string;
    EXPECT_EQ(count, 400);
    console_log_seen_ = true;
  }

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    // If SetMaxVirtualTimeTaskStarvationCount was not set, this callback would
    // never fire.
    EXPECT_TRUE(console_log_seen_);
    FinishAsynchronousTest();
  }

  bool console_log_seen_ = false;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(VirtualTimeLocalStorageTest);

class VirtualTimeSessionStorageTest : public VirtualTimeBrowserTest {
 public:
  VirtualTimeSessionStorageTest() {
    EXPECT_TRUE(embedded_test_server()->Start());
    SetInitialURL(embedded_test_server()
                      ->GetURL("/virtual_time_session_storage.html")
                      .spec());
  }

  void SetAfterLoadVirtualTimePolicy() override {
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(
                emulation::VirtualTimePolicy::PAUSE_IF_NETWORK_FETCHES_PENDING)
            .SetBudget(5000)
            .SetWaitForNavigation(true)
            .Build());
  }

  void OnConsoleAPICalled(
      const runtime::ConsoleAPICalledParams& params) override {
    EXPECT_EQ(runtime::ConsoleAPICalledType::LOG, params.GetType());
    ASSERT_EQ(1u, params.GetArgs()->size());
    ASSERT_EQ(runtime::RemoteObjectType::STRING,
              (*params.GetArgs())[0]->GetType());
    std::string count_string = (*params.GetArgs())[0]->GetValue()->GetString();
    int count;
    ASSERT_TRUE(base::StringToInt(count_string, &count)) << count_string;
    EXPECT_EQ(count, 400);
    console_log_seen_ = true;
  }

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    // If SetMaxVirtualTimeTaskStarvationCount was not set, this callback would
    // never fire.
    EXPECT_TRUE(console_log_seen_);
    FinishAsynchronousTest();
  }

  bool console_log_seen_ = false;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(VirtualTimeSessionStorageTest);

class DeferredLoadDoesntBlockVirtualTimeTest : public VirtualTimeBrowserTest {
 public:
  DeferredLoadDoesntBlockVirtualTimeTest() {
    EXPECT_TRUE(embedded_test_server()->Start());
    SetInitialURL(embedded_test_server()->GetURL("/video.html").spec());
  }

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    // The video should not block virtual time.
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(DeferredLoadDoesntBlockVirtualTimeTest);

class Http404DoesntBlockVirtualTimeTest : public VirtualTimeBrowserTest {
 public:
  Http404DoesntBlockVirtualTimeTest() {
    EXPECT_TRUE(embedded_test_server()->Start());
    SetInitialURL(embedded_test_server()->GetURL("/NoSuchFile.html").spec());
  }

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    // The video should not block virtual time.
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(Http404DoesntBlockVirtualTimeTest);

class RedirectVirtualTimeTest : public VirtualTimeBrowserTest {
 public:
  RedirectVirtualTimeTest() { SetInitialURL("http://test.com/index.html"); }

  ProtocolHandlerMap GetProtocolHandlers() override {
    ProtocolHandlerMap protocol_handlers;
    std::unique_ptr<TestInMemoryProtocolHandler> http_handler(
        new TestInMemoryProtocolHandler(browser()->BrowserIOThread(), nullptr));
    http_handler_ = http_handler.get();
    http_handler->InsertResponse("http://test.com/index.html",
                                 {kIndexHtml, "text/html"});
    http_handler->InsertResponse(
        "http://test.com/iframe.html",
        {"HTTP/1.1 302 Found\r\nLocation: iframe2.html\r\n\r\n"});
    http_handler->InsertResponse("http://test.com/iframe2.html",
                                 {kIFrame, "text/html"});
    http_handler->InsertResponse(
        "http://test.com/style.css",
        {"HTTP/1.1 302 Found\r\nLocation: style2.css\r\n\r\n"});
    http_handler->InsertResponse("http://test.com/style2.css",
                                 {kCss, "text/css"});
    protocol_handlers[url::kHttpScheme] = std::move(http_handler);
    return protocol_handlers;
  }

  void RunDevTooledTest() override {
    http_handler_->SetHeadlessBrowserContext(browser_context_);
    VirtualTimeBrowserTest::RunDevTooledTest();
  }

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    EXPECT_THAT(
        http_handler_->urls_requested(),
        ElementsAre("http://test.com/index.html", "http://test.com/iframe.html",
                    "http://test.com/iframe2.html", "http://test.com/style.css",
                    "http://test.com/style2.css"));

    // Virtual time should still expire, despite the CSS resource load not
    // finishing.
    FinishAsynchronousTest();
  }

  TestInMemoryProtocolHandler* http_handler_;  // NOT OWNED
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(RedirectVirtualTimeTest);

namespace {
static constexpr char kFooDotCom[] = R"(
<html>
<body>
<script> console.log(document.location.href); </script>
<iframe src='/a/'></iframe>\n"
</body>
</html>
)";

static constexpr char kFooDotComSlashA[] = R"(
<html>
<body>
<script> console.log(document.location.href); </script>
</body>
</html>
)";

static constexpr char kBarDotCom[] = R"(
<html>
<body>
<script> console.log(document.location.href); </script>
<iframe src='/b/' id='frame_b'></iframe>
<iframe src='/c/'></iframe>
</body>
</html>
)";

static constexpr char kBarDotComSlashB[] = R"(
<html>
<body>
<script> console.log(document.location.href); </script>
<iframe src='/d/'></iframe>
</body>
</html>
)";

static constexpr char kBarDotComSlashC[] = R"(
<html>
<body>
<script> console.log(document.location.href); </script>
</body>
</html>
)";

static constexpr char kBarDotComSlashD[] = R"(
<html>
<body>
<script> console.log(document.location.href); </script>
</body>
</html>
)";

static constexpr char kBarDotComSlashE[] = R"(
<html>
<body>
<script> console.log(document.location.href); </script>
<iframe src='/f/'></iframe>
</body>
</html>
)";

static constexpr char kBarDotComSlashF[] = R"(
<html>
<body>
<script> console.log(document.location.href); </script>
</body>
</html>
)";

}  // namespace

class VirtualTimeAndHistoryNavigationTest : public VirtualTimeBrowserTest {
 public:
  VirtualTimeAndHistoryNavigationTest() { SetInitialURL("http://foo.com/"); }

  ProtocolHandlerMap GetProtocolHandlers() override {
    ProtocolHandlerMap protocol_handlers;
    std::unique_ptr<TestInMemoryProtocolHandler> http_handler(
        new TestInMemoryProtocolHandler(browser()->BrowserIOThread(), nullptr));
    http_handler_ = http_handler.get();
    http_handler->InsertResponse("http://foo.com/", {kFooDotCom, "text/html"});
    http_handler->InsertResponse("http://foo.com/a/",
                                 {kFooDotComSlashA, "text/html"});
    http_handler->InsertResponse("http://bar.com/", {kBarDotCom, "text/html"});
    http_handler->InsertResponse("http://bar.com/b/",
                                 {kBarDotComSlashB, "text/html"});
    http_handler->InsertResponse("http://bar.com/c/",
                                 {kBarDotComSlashC, "text/html"});
    http_handler->InsertResponse("http://bar.com/d/",
                                 {kBarDotComSlashD, "text/html"});
    http_handler->InsertResponse("http://bar.com/e/",
                                 {kBarDotComSlashE, "text/html"});
    http_handler->InsertResponse("http://bar.com/f/",
                                 {kBarDotComSlashF, "text/html"});
    protocol_handlers[url::kHttpScheme] = std::move(http_handler);
    return protocol_handlers;
  }

  void RunDevTooledTest() override {
    http_handler_->SetHeadlessBrowserContext(browser_context_);
    VirtualTimeBrowserTest::RunDevTooledTest();
  }

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    if (step_ < test_commands_.size()) {
      devtools_client_->GetRuntime()->Evaluate(
          test_commands_[step_++],
          base::BindRepeating(
              &VirtualTimeAndHistoryNavigationTest::OnEvaluateResult,
              base::Unretained(this)));
    } else {
      FinishAsynchronousTest();
    }
  }

  void OnEvaluateResult(std::unique_ptr<runtime::EvaluateResult>) {
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(
                emulation::VirtualTimePolicy::PAUSE_IF_NETWORK_FETCHES_PENDING)
            .SetBudget(5000)
            .Build());
  }

  const std::vector<std::string> test_commands_ = {
      "document.location.href = 'http://bar.com/'",
      "document.getElementById('frame_b').src = '/e/'",
      "history.back()",
      "history.forward()",
      "history.go(-1)",
  };

  size_t step_ = 0;
  TestInMemoryProtocolHandler* http_handler_;  // NOT OWNED
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(VirtualTimeAndHistoryNavigationTest);

namespace {
static constexpr char kIndexHtmlWithScript[] = R"(
<html>
<body>
<script src="/large.js"></script>
</body>
</html>
)";

static constexpr char kLargeDotJS[] = R"(
(function() {
var setTitle = function(newTitle) {
  document.title = newTitle;
};
%s
setTitle('Test PASS');
})();
)";

}  // namespace

class PendingScriptVirtualTimeTest : public VirtualTimeBrowserTest {
 public:
  PendingScriptVirtualTimeTest() {
    SetInitialURL("http://test.com/index.html");
  }

  ProtocolHandlerMap GetProtocolHandlers() override {
    ProtocolHandlerMap protocol_handlers;
    std::unique_ptr<TestInMemoryProtocolHandler> http_handler(
        new TestInMemoryProtocolHandler(browser()->BrowserIOThread(), nullptr));
    http_handler_ = http_handler.get();
    http_handler_->InsertResponse("http://test.com/index.html",
                                  {kIndexHtmlWithScript, "text/html"});
    // We want to pad |kLargeDotJS| out with some dummy code which is parsed
    // asynchronously to make sure the virtual_time_pauser in PendingScript
    // actually does something. We construct a large number of long unused
    // function declarations which seems to trigger the desired code path.
    std::string dummy;
    dummy.reserve(1024 * 1024 * 4);
    for (int i = 0; i < 1024; i++) {
      dummy.append(base::StringPrintf("var i%d=function(){return '", i));
      dummy.append(1024, 'A');
      dummy.append("';}\n");
    }
    large_js_ = base::StringPrintf(kLargeDotJS, dummy.c_str());
    http_handler_->InsertResponse(
        "http://test.com/large.js",
        {large_js_.c_str(), "application/javascript"});
    protocol_handlers[url::kHttpScheme] = std::move(http_handler);
    return protocol_handlers;
  }

  void RunDevTooledTest() override {
    http_handler_->SetHeadlessBrowserContext(browser_context_);
    VirtualTimeBrowserTest::RunDevTooledTest();
  }

  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    devtools_client_->GetRuntime()->Evaluate(
        "document.title",
        base::BindRepeating(&PendingScriptVirtualTimeTest::OnEvaluateResult,
                            base::Unretained(this)));
  }

  void OnEvaluateResult(std::unique_ptr<runtime::EvaluateResult> result) {
    EXPECT_EQ("Test PASS", result->GetResult()->GetValue()->GetString());
    FinishAsynchronousTest();
  }
  TestInMemoryProtocolHandler* http_handler_;  // NOT OWNED
  std::string large_js_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(PendingScriptVirtualTimeTest);

namespace {
static constexpr char kResourceErrorLoop[] = R"(
<html>
<script>
var counter = 1;
</script>
<img src="1" onerror="this.src='' + ++counter;">
</html>
)";
}

class VirtualTimeAndResourceErrorLoopTest : public VirtualTimeBrowserTest {
 public:
  VirtualTimeAndResourceErrorLoopTest() { SetInitialURL("http://foo.com/"); }

  ProtocolHandlerMap GetProtocolHandlers() override {
    ProtocolHandlerMap protocol_handlers;
    std::unique_ptr<TestInMemoryProtocolHandler> http_handler(
        new TestInMemoryProtocolHandler(browser()->BrowserIOThread(), nullptr));
    http_handler_ = http_handler.get();
    http_handler->InsertResponse("http://foo.com/",
                                 {kResourceErrorLoop, "text/html"});
    protocol_handlers[url::kHttpScheme] = std::move(http_handler);
    return protocol_handlers;
  }

  void RunDevTooledTest() override {
    http_handler_->SetHeadlessBrowserContext(browser_context_);
    VirtualTimeBrowserTest::RunDevTooledTest();
  }

  void SetAfterLoadVirtualTimePolicy() override {
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(
                emulation::VirtualTimePolicy::PAUSE_IF_NETWORK_FETCHES_PENDING)
            .SetBudget(5000)
            .SetMaxVirtualTimeTaskStarvationCount(1000000)  // Prevent flakes.
            .SetWaitForNavigation(true)
            .Build());
  }

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    // The budget is 5000 virtual ms.  The resources are delivered with 10
    // virtual ms delay, so we should have 500 urls.
    EXPECT_EQ(500u, http_handler_->urls_requested().size());
    FinishAsynchronousTest();
  }

  TestInMemoryProtocolHandler* http_handler_;  // NOT OWNED
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(VirtualTimeAndResourceErrorLoopTest);

}  // namespace headless
