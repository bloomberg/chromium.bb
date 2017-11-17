// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <strstream>

#include "content/public/test/browser_test.h"
#include "headless/public/devtools/domains/dom_snapshot.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/test/headless_render_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {

namespace {

using dom_snapshot::GetSnapshotResult;
using dom_snapshot::DOMNode;
using dom_snapshot::LayoutTreeNode;

template <typename T, typename V>
std::vector<T> ElementsView(const std::vector<std::unique_ptr<V>>& elements,
                            std::function<bool(const V&)> filter,
                            std::function<T(const V&)> transform) {
  std::vector<T> result;
  for (const auto& element : elements) {
    if (filter(*element))
      result.push_back(transform(*element));
  }
  return result;
}

bool HasType(int type, const DOMNode& node) {
  return node.GetNodeType() == type;
}
bool HasName(const char* name, const DOMNode& node) {
  return node.GetNodeName() == name;
}
bool IsTag(const DOMNode& node) {
  return HasType(1, node);
}
bool IsText(const DOMNode& node) {
  return HasType(3, node);
}
bool IsTagWithName(const char* name, const DOMNode& node) {
  return IsTag(node) && HasName(name, node);
}

std::vector<std::string> Structure(const GetSnapshotResult* snapshot) {
  return ElementsView<std::string, DOMNode>(
      *snapshot->GetDomNodes(), IsTag,
      [](const auto& node) { return node.GetNodeName(); });
}

std::vector<std::string> Contents(const GetSnapshotResult* snapshot) {
  return ElementsView<std::string, DOMNode>(
      *snapshot->GetDomNodes(), IsText,
      [](const auto& node) { return node.GetNodeValue(); });
}

std::vector<std::string> TextLayout(const GetSnapshotResult* snapshot) {
  return ElementsView<std::string, LayoutTreeNode>(
      *snapshot->GetLayoutTreeNodes(),
      [](const auto& node) { return node.HasLayoutText(); },
      [](const auto& node) { return node.GetLayoutText(); });
}

std::vector<const DOMNode*> FilterDOM(
    const GetSnapshotResult* snapshot,
    std::function<bool(const DOMNode&)> filter) {
  return ElementsView<const DOMNode*, DOMNode>(
      *snapshot->GetDomNodes(), filter, [](const auto& n) { return &n; });
}

size_t IndexInDOM(const GetSnapshotResult* snapshot, const DOMNode* node) {
  for (size_t i = 0; i < snapshot->GetDomNodes()->size(); ++i) {
    if (snapshot->GetDomNodes()->at(i).get() == node)
      return i;
  }
  CHECK(false);
  return static_cast<size_t>(-1);
}

const DOMNode* GetAt(const GetSnapshotResult* snapshot, size_t index) {
  CHECK_LE(index, snapshot->GetDomNodes()->size());
  return snapshot->GetDomNodes()->at(index).get();
}

MATCHER_P(NodeName, expected, "") {
  return arg->GetNodeName() == expected;
}
MATCHER_P(NodeValue, expected, "") {
  return arg->GetNodeValue() == expected;
}
MATCHER_P(NodeType, expected, 0) {
  return arg->GetNodeType() == expected;
}

MATCHER_P(RemoteString, expected, "") {
  return arg->GetType() == runtime::RemoteObjectType::STRING &&
         arg->GetValue()->GetString() == expected;
}

using testing::ElementsAre;
using testing::StartsWith;

}  // namespace

class HelloWorldTest : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    return embedded_test_server()->GetURL("/hello.html");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(Structure(dom_snapshot),
                ElementsAre("HTML", "HEAD", "BODY", "H1"));
    EXPECT_THAT(Contents(dom_snapshot),
                ElementsAre("Hello headless world!", "\n"));
    EXPECT_THAT(TextLayout(dom_snapshot), ElementsAre("Hello headless world!"));
    AllDone();
  }
};
HEADLESS_ASYNC_DEVTOOLED_TEST_F(HelloWorldTest);

class TimeoutTest : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    base::RunLoop run_loop;
    client->GetPage()->Disable(run_loop.QuitClosure());
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    run_loop.Run();
    return embedded_test_server()->GetURL("/hello.html");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    FAIL() << "Should not reach here";
  }

  void OnTimeout() override { AllDone(); }
};
HEADLESS_ASYNC_DEVTOOLED_TEST_F(TimeoutTest);

class JavaScriptOverrideTitle_JsEnabled : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    return embedded_test_server()->GetURL(
        "/render/javascript_override_title.html");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    auto dom = FilterDOM(
        dom_snapshot, [](const auto& n) { return IsTagWithName("TITLE", n); });
    ASSERT_THAT(dom, ElementsAre(NodeName("TITLE")));
    size_t pos = IndexInDOM(dom_snapshot, dom[0]);
    const DOMNode* value = GetAt(dom_snapshot, pos + 1);
    EXPECT_THAT(value, NodeValue("JavaScript is on"));
    AllDone();
  }
};
HEADLESS_ASYNC_DEVTOOLED_TEST_F(JavaScriptOverrideTitle_JsEnabled);

class JavaScriptOverrideTitle_JsDisabled : public HeadlessRenderTest {
 private:
  void OverrideWebPreferences(WebPreferences* preferences) override {
    HeadlessRenderTest::OverrideWebPreferences(preferences);
    preferences->javascript_enabled = false;
  }

  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    return embedded_test_server()->GetURL(
        "/render/javascript_override_title.html");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    auto dom = FilterDOM(
        dom_snapshot, [](const auto& n) { return IsTagWithName("TITLE", n); });
    ASSERT_THAT(dom, ElementsAre(NodeName("TITLE")));
    size_t pos = IndexInDOM(dom_snapshot, dom[0]);
    const DOMNode* value = GetAt(dom_snapshot, pos + 1);
    EXPECT_THAT(value, NodeValue("JavaScript is off"));
    AllDone();
  }
};
HEADLESS_ASYNC_DEVTOOLED_TEST_F(JavaScriptOverrideTitle_JsDisabled);

class JavaScriptConsoleErrors : public HeadlessRenderTest,
                                public runtime::ExperimentalObserver {
 private:
  HeadlessDevToolsClient* client_;
  std::vector<std::string> messages_;
  bool log_called_ = false;

  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    client_ = client;
    client_->GetRuntime()->GetExperimental()->AddObserver(this);
    base::RunLoop run_loop;
    client_->GetRuntime()->GetExperimental()->Enable(run_loop.QuitClosure());
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    run_loop.Run();
    return embedded_test_server()->GetURL("/render/console_errors.html");
  }

  void OnConsoleAPICalled(
      const runtime::ConsoleAPICalledParams& params) override {
    EXPECT_THAT(*params.GetArgs(), ElementsAre(RemoteString("Hello, Script!")));
    log_called_ = true;
  }

  void OnExceptionThrown(
      const runtime::ExceptionThrownParams& params) override {
    const runtime::ExceptionDetails* details = params.GetExceptionDetails();
    messages_.push_back(details->GetText() + " " +
                        details->GetException()->GetDescription());
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    client_->GetRuntime()->GetExperimental()->Disable();
    client_->GetRuntime()->GetExperimental()->RemoveObserver(this);
    EXPECT_TRUE(log_called_);
    EXPECT_THAT(messages_,
                ElementsAre(StartsWith("Uncaught SyntaxError:"),
                            StartsWith("Uncaught ReferenceError: func1"),
                            StartsWith("Uncaught ReferenceError: func2"),
                            StartsWith("Uncaught ReferenceError: func3")));
    AllDone();
  }
};
HEADLESS_ASYNC_DEVTOOLED_TEST_F(JavaScriptConsoleErrors);

}  // namespace headless
