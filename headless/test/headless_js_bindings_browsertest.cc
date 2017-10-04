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
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_tab_socket.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/tab_socket_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace headless {

class HeadlessJsBindingsTest
    : public TabSocketTest,
      public HeadlessDevToolsClient::RawProtocolListener {
 public:
  void SetUpOnMainThread() override {
    base::ThreadRestrictions::SetIOAllowed(true);
    base::FilePath pak_path;
    ASSERT_TRUE(PathService::Get(base::DIR_MODULE, &pak_path));
    pak_path = pak_path.AppendASCII("headless_browser_tests.pak");
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path, ui::SCALE_FACTOR_NONE);
  }

  void RunTabSocketTest() override {
    headless_tab_socket_ = web_contents_->GetHeadlessTabSocket();
    CHECK(headless_tab_socket_);
    headless_tab_socket_->SetListener(this);
    devtools_client_->SetRawProtocolListener(this);
    CreateMainWorldTabSocket(
        main_frame_id(),
        base::Bind(&HeadlessJsBindingsTest::OnInstalledHeadlessTabSocket,
                   base::Unretained(this)));
  }

  void OnInstalledHeadlessTabSocket(int v8_exection_context_id) {
    main_world_execution_context_id_ = v8_exection_context_id;
    devtools_client_->GetRuntime()->Evaluate(
        ui::ResourceBundle::GetSharedInstance()
            .GetRawDataResource(DEVTOOLS_BINDINGS_TEST)
            .as_string(),
        base::Bind(&HeadlessJsBindingsTest::OnEvaluateResult,
                   base::Unretained(this)));
  }

  virtual void RunJsBindingsTest() = 0;
  virtual std::string GetExpectedResult() = 0;

  void OnEvaluateResult(std::unique_ptr<runtime::EvaluateResult> result) {
    if (!result->HasExceptionDetails()) {
      RunJsBindingsTest();
    } else {
      FailOnJsEvaluateException(std::move(result));
    }
  }

  void OnMessageFromContext(const std::string& json_message,
                            int v8_execution_context_id) override {
    EXPECT_EQ(main_world_execution_context_id_, v8_execution_context_id);
    std::unique_ptr<base::Value> message =
        base::JSONReader::Read(json_message, base::JSON_PARSE_RFC);
    const base::DictionaryValue* message_dict;
    const base::DictionaryValue* params_dict;
    std::string method;
    int id;
    if (!message || !message->GetAsDictionary(&message_dict) ||
        !message_dict->GetString("method", &method) ||
        !message_dict->GetDictionary("params", &params_dict) ||
        !message_dict->GetInteger("id", &id)) {
      FinishAsynchronousTest();
      FAIL() << "Badly formed message " << json_message;
      return;
    }

    if (method == "__Result") {
      std::string result;
      params_dict->GetString("result", &result);
      EXPECT_EQ(GetExpectedResult(), result);
      FinishAsynchronousTest();
      return;
    }

    devtools_client_->SendRawDevToolsMessage(json_message);
  }

  bool OnProtocolMessage(const std::string& devtools_agent_host_id,
                         const std::string& json_message,
                         const base::DictionaryValue& parsed_message) override {
    int id;
    // If |parsed_message| contains an id we know this is a message reply.
    if (parsed_message.GetInteger("id", &id)) {
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

    std::string method;
    if (!parsed_message.GetString("method", &method))
      return false;

    headless_tab_socket_->SendMessageToContext(
        json_message, main_world_execution_context_id_);

    // Check which domain the event belongs to, if it's the DOM domain then
    // assume js handled it.
    std::vector<base::StringPiece> sections = SplitStringPiece(
        method, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    return sections[0] == "DOM" || sections[0] == "Runtime";
  }

 private:
  HeadlessTabSocket* headless_tab_socket_;
  int main_world_execution_context_id_;
};

class SimpleCommandJsBindingsTest : public HeadlessJsBindingsTest {
 public:
  void RunJsBindingsTest() override {
    devtools_client_->GetRuntime()->Evaluate(
        "new chromium.BindingsTest().evalOneAddOne();",
        base::Bind(&HeadlessJsBindingsTest::FailOnJsEvaluateException,
                   base::Unretained(this)));
  }

  std::string GetExpectedResult() override { return "2"; }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(SimpleCommandJsBindingsTest);

class ExperimentalCommandJsBindingsTest : public HeadlessJsBindingsTest {
 public:
  void RunJsBindingsTest() override {
    devtools_client_->GetRuntime()->Evaluate(
        "new chromium.BindingsTest().getIsolatedWorldName();",
        base::Bind(&HeadlessJsBindingsTest::FailOnJsEvaluateException,
                   base::Unretained(this)));
  }

  std::string GetExpectedResult() override {
    return "Created Test Isolated World";
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(ExperimentalCommandJsBindingsTest);

class SimpleEventJsBindingsTest : public HeadlessJsBindingsTest {
 public:
  void RunJsBindingsTest() override {
    devtools_client_->GetRuntime()->Evaluate(
        "new chromium.BindingsTest().listenForChildNodeCountUpdated();",
        base::Bind(&HeadlessJsBindingsTest::FailOnJsEvaluateException,
                   base::Unretained(this)));
  }

  std::string GetExpectedResult() override {
    return "{\"nodeId\":4,\"childNodeCount\":1}";
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(SimpleEventJsBindingsTest);

}  // namespace headless
