// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "headless/grit/headless_browsertest_resources.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_tab_socket.h"
#include "headless/public/headless_web_contents.h"
#include "headless/public/util/testing/test_in_memory_protocol_handler.h"
#include "headless/test/tab_socket_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace headless {

namespace {
static constexpr char kIndexHtml[] = R"(
<html>
<body>
<script src="bindings.js"></script>
</body>
</html>
)";
}  // namespace

class HeadlessJsBindingsTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public HeadlessDevToolsClient::RawProtocolListener,
      public TestInMemoryProtocolHandler::RequestDeferrer,
      public HeadlessTabSocket::Listener,
      public page::ExperimentalObserver {
 public:
  void SetUp() override {
    options()->mojo_service_names.insert("headless::TabSocket");
    HeadlessAsyncDevTooledBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    base::ThreadRestrictions::SetIOAllowed(true);
    base::FilePath pak_path;
    ASSERT_TRUE(PathService::Get(base::DIR_MODULE, &pak_path));
    pak_path = pak_path.AppendASCII("headless_browser_tests.pak");
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path, ui::SCALE_FACTOR_NONE);
  }

  void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder) override {
    builder.AddTabSocketMojoBindings();
    builder.EnableUnsafeNetworkAccessWithMojoBindings(true);
  }

  void CustomizeHeadlessWebContents(
      HeadlessWebContents::Builder& builder) override {
    builder.SetWindowSize(gfx::Size(0, 0));
    builder.SetInitialURL(GURL("http://test.com/index.html"));

    http_handler_->SetHeadlessBrowserContext(browser_context_);
  }

  bool GetAllowTabSockets() override { return true; }

  ProtocolHandlerMap GetProtocolHandlers() override {
    ProtocolHandlerMap protocol_handlers;
    std::unique_ptr<TestInMemoryProtocolHandler> http_handler(
        new TestInMemoryProtocolHandler(browser()->BrowserIOThread(), this));
    http_handler_ = http_handler.get();
    bindings_js_ = ui::ResourceBundle::GetSharedInstance()
                       .GetRawDataResource(DEVTOOLS_BINDINGS_TEST)
                       .as_string();
    http_handler->InsertResponse("http://test.com/index.html",
                                 {kIndexHtml, "text/html"});
    http_handler->InsertResponse(
        "http://test.com/bindings.js",
        {bindings_js_.c_str(), "application/javascript"});
    protocol_handlers[url::kHttpScheme] = std::move(http_handler);
    return protocol_handlers;
  }

  void RunDevTooledTest() override {
    devtools_client_->GetPage()->GetExperimental()->AddObserver(this);
    devtools_client_->GetPage()->Enable();
    headless_tab_socket_ = web_contents_->GetHeadlessTabSocket();
    CHECK(headless_tab_socket_);
    headless_tab_socket_->SetListener(this);
    devtools_client_->SetRawProtocolListener(this);

    headless_tab_socket_->InstallMainFrameMainWorldHeadlessTabSocketBindings(
        base::BindOnce(&HeadlessJsBindingsTest::OnInstalledHeadlessTabSocket,
                       base::Unretained(this)));
  }

  void OnRequest(const GURL& url,
                 base::RepeatingClosure complete_request) override {
    if (!tab_socket_installed_ && url.spec() == "http://test.com/bindings.js") {
      complete_request_ = std::move(complete_request);
    } else {
      complete_request.Run();
    }
  }

  void OnInstalledHeadlessTabSocket(base::Optional<int> context_id) {
    main_world_execution_context_id_ = *context_id;
    tab_socket_installed_ = true;
    if (complete_request_) {
      browser()->BrowserIOThread()->PostTask(FROM_HERE, complete_request_);
      complete_request_ = base::RepeatingClosure();
    }
  }

  virtual void RunJsBindingsTest() = 0;

  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    RunJsBindingsTest();
  }

  virtual void OnResult(const std::string& result) = 0;

  void FailOnJsEvaluateException(
      std::unique_ptr<runtime::EvaluateResult> result) {
    if (!result->HasExceptionDetails())
      return;

    FinishAsynchronousTest();

    const runtime::ExceptionDetails* exception_details =
        result->GetExceptionDetails();
    FAIL() << exception_details->GetText()
           << (exception_details->HasException()
                   ? exception_details->GetException()->GetDescription().c_str()
                   : "");
  }

  void OnMessageFromContext(const std::string& json_message,
                            int v8_execution_context_id) override {
    EXPECT_EQ(main_world_execution_context_id_, v8_execution_context_id);
    std::unique_ptr<base::Value> message =
        base::JSONReader::Read(json_message, base::JSON_PARSE_RFC);
    const base::Value* method_value = message->FindKey("method");
    if (!method_value) {
      FinishAsynchronousTest();
      FAIL() << "Badly formed message " << json_message;
      return;
    }

    const base::Value* params_value = message->FindKey("params");
    if (!params_value) {
      FinishAsynchronousTest();
      FAIL() << "Badly formed message " << json_message;
      return;
    }

    const base::Value* id_value = message->FindKey("id");
    if (!id_value) {
      FinishAsynchronousTest();
      FAIL() << "Badly formed message " << json_message;
      return;
    }

    if (method_value->GetString() == "__Result") {
      OnResult(params_value->FindKey("result")->GetString());
      return;
    }

    devtools_client_->SendRawDevToolsMessage(json_message);
  }

  bool OnProtocolMessage(const std::string& devtools_agent_host_id,
                         const std::string& json_message,
                         const base::DictionaryValue& parsed_message) override {
    if (main_world_execution_context_id_ == -1)
      return false;

    const base::Value* id_value = parsed_message.FindKey("id");
    // If |parsed_message| contains an id we know this is a message reply.
    if (id_value) {
      int id = id_value->GetInt();
      // We are only interested in message replies (ones with an id) where the
      // id is odd. The reason is HeadlessDevToolsClientImpl uses even/oddness
      // to distinguish between commands send from the C++ bindings and those
      // via HeadlessDevToolsClientImpl::SendRawDevToolsMessage.
      if ((id % 2) == 0)
        return false;

      headless_tab_socket_->SendMessageToContext(
          json_message, main_world_execution_context_id_);
      return true;
    }

    const base::Value* method_value = parsed_message.FindKey("method");
    if (!method_value)
      return false;

    headless_tab_socket_->SendMessageToContext(
        json_message, main_world_execution_context_id_);

    // Check which domain the event belongs to, if it's the DOM domain then
    // assume js handled it.
    std::vector<base::StringPiece> sections =
        SplitStringPiece(method_value->GetString(), ".", base::KEEP_WHITESPACE,
                         base::SPLIT_WANT_ALL);
    return sections[0] == "DOM" || sections[0] == "Runtime";
  }

 protected:
  TestInMemoryProtocolHandler* http_handler_;  // NOT OWNED
  HeadlessTabSocket* headless_tab_socket_;     // NOT OWNED
  int main_world_execution_context_id_ = -1;
  std::string bindings_js_;
  base::RepeatingClosure complete_request_;
  bool tab_socket_installed_ = false;
};

class SimpleCommandJsBindingsTest : public HeadlessJsBindingsTest {
 public:
  void RunJsBindingsTest() override {
    devtools_client_->GetRuntime()->Evaluate(
        "new chromium.BindingsTest().evalOneAddOne();",
        base::BindOnce(&HeadlessJsBindingsTest::FailOnJsEvaluateException,
                       base::Unretained(this)));
  }

  void OnResult(const std::string& result) override {
    EXPECT_EQ("2", result);
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(SimpleCommandJsBindingsTest);

class ExperimentalCommandJsBindingsTest : public HeadlessJsBindingsTest {
 public:
  void RunJsBindingsTest() override {
    devtools_client_->GetRuntime()->Evaluate(
        "new chromium.BindingsTest().getIsolatedWorldName();",
        base::BindOnce(&HeadlessJsBindingsTest::FailOnJsEvaluateException,
                       base::Unretained(this)));
  }

  void OnResult(const std::string& result) override {
    EXPECT_EQ("Created Test Isolated World", result);
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(ExperimentalCommandJsBindingsTest);

class SimpleEventJsBindingsTest : public HeadlessJsBindingsTest {
 public:
  void RunJsBindingsTest() override {
    devtools_client_->GetRuntime()->Evaluate(
        "new chromium.BindingsTest().listenForChildNodeCountUpdated();",
        base::BindOnce(&HeadlessJsBindingsTest::FailOnJsEvaluateException,
                       base::Unretained(this)));
  }

  void OnResult(const std::string& result) override {
    EXPECT_EQ("{\"nodeId\":4,\"childNodeCount\":2}", result);
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(SimpleEventJsBindingsTest);

/*
 * Like SimpleCommandJsBindingsTest except it's run twice. On the first run
 * metadata is produced by v8 for http://test.com/bindings.js. On the second run
 * the metadata is used used leading to substantially faster execution time.
 */
class CachedJsBindingsTest : public HeadlessJsBindingsTest,
                             public HeadlessBrowserContext::Observer {
 public:
  void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder) override {
    builder.SetCaptureResourceMetadata(true);
    builder.SetOverrideWebPreferencesCallback(base::BindRepeating(
        &CachedJsBindingsTest::OverrideWebPreferences, base::Unretained(this)));
    HeadlessJsBindingsTest::CustomizeHeadlessBrowserContext(builder);
  }

  void OverrideWebPreferences(WebPreferences* preferences) {
    // Request eager code compilation.
    preferences->v8_cache_options =
        content::V8_CACHE_OPTIONS_FULLCODE_WITHOUT_HEAT_CHECK;
  }

  void RunDevTooledTest() override {
    browser_context_->AddObserver(this);
    HeadlessJsBindingsTest::RunDevTooledTest();
  }

  void RunJsBindingsTest() override {
    devtools_client_->GetRuntime()->Evaluate(
        "new chromium.BindingsTest().evalOneAddOne();",
        base::BindRepeating(&HeadlessJsBindingsTest::FailOnJsEvaluateException,
                            base::Unretained(this)));
  }

  void OnResult(const std::string& result) override {
    EXPECT_EQ("2", result);

    if (first_result) {
      tab_socket_installed_ = false;
      main_world_execution_context_id_ = -1;
      devtools_client_->GetPage()->Reload();
    } else {
      EXPECT_TRUE(metadata_received_);
      FinishAsynchronousTest();
    }
    first_result = false;
  }

  // Called on the IO thread.
  void OnRequest(const GURL& url,
                 base::RepeatingClosure complete_request) override {
    HeadlessJsBindingsTest::OnRequest(url, complete_request);
    if (!first_result && url.spec() == "http://test.com/bindings.js") {
      browser()->BrowserMainThread()->PostTask(
          FROM_HERE,
          base::BindOnce(&CachedJsBindingsTest::ReinstallTabSocketBindings,
                         base::Unretained(this)));
    }
  }

  void ReinstallTabSocketBindings() {
    headless_tab_socket_->InstallMainFrameMainWorldHeadlessTabSocketBindings(
        base::BindRepeating(
            &HeadlessJsBindingsTest::OnInstalledHeadlessTabSocket,
            base::Unretained(this)));
  }

  void OnMetadataForResource(const GURL& url,
                             net::IOBuffer* buf,
                             int buf_len) override {
    ASSERT_FALSE(metadata_received_);
    metadata_received_ = true;

    scoped_refptr<net::IOBufferWithSize> metadata(
        new net::IOBufferWithSize(buf_len));
    memcpy(metadata->data(), buf->data(), buf_len);
    http_handler_->SetResponseMetadata(url.spec(), metadata);
  }

  bool metadata_received_ = false;
  bool first_result = true;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(CachedJsBindingsTest);

}  // namespace headless
