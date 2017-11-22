// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <strstream>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/public/test/browser_test.h"
#include "headless/public/devtools/domains/dom_snapshot.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/test/headless_render_test.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define HEADLESS_RENDER_BROWSERTEST(clazz)                  \
  class HeadlessRenderBrowserTest##clazz : public clazz {}; \
  HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessRenderBrowserTest##clazz)

namespace headless {

namespace {

const std::string SOME_HOST = "http://www.example.com";
constexpr char SOME_URL[] = "http://example.com/foobar";
constexpr char TEXT_HTML[] = "text/html";

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

std::vector<const DOMNode*> FindTags(const GetSnapshotResult* snapshot,
                                     const char* name = nullptr) {
  return FilterDOM(snapshot, [name](const auto& n) {
    return IsTag(n) && (!name || HasName(name, n));
  });
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

const DOMNode* NextNode(const GetSnapshotResult* snapshot,
                        const DOMNode* node) {
  return GetAt(snapshot, IndexInDOM(snapshot, node) + 1);
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

MATCHER_P(RequestPath, expected, "") {
  return arg.relative_url == expected;
}

using testing::ElementsAre;
using testing::Eq;
using testing::StartsWith;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using net::test_server::BasicHttpResponse;
using net::test_server::RawHttpResponse;

}  // namespace

class HelloWorldTest : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse(SOME_URL, {R"|(
<!doctype html>
<h1>Hello headless world!</h1>
)|",
                                                    TEXT_HTML});
    return GURL(SOME_URL);
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(FindTags(dom_snapshot),
                ElementsAre(NodeName("HTML"), NodeName("HEAD"),
                            NodeName("BODY"), NodeName("H1")));
    EXPECT_THAT(
        FilterDOM(dom_snapshot, IsText),
        ElementsAre(NodeValue("Hello headless world!"), NodeValue("\n")));
    EXPECT_THAT(TextLayout(dom_snapshot), ElementsAre("Hello headless world!"));
  }
};
HEADLESS_RENDER_BROWSERTEST(HelloWorldTest);

class TimeoutTest : public HelloWorldTest {
 private:
  void OnPageRenderCompleted() override {
    // Never complete.
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    FAIL() << "Should not reach here";
  }

  void OnTimeout() override { SetTestCompleted(); }
};
HEADLESS_RENDER_BROWSERTEST(TimeoutTest);

class JavaScriptOverrideTitle_JsEnabled : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse(SOME_URL, {R"|(
<html>
  <head>
    <title>JavaScript is off</title>
    <script language="JavaScript">
      <!-- Begin
        document.title = 'JavaScript is on';
      //  End -->
    </script>
  </head>
  <body onload="settitle()">
    Hello, World!
  </body>
</html>
)|",
                                                    TEXT_HTML});
    return GURL(SOME_URL);
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    auto dom = FindTags(dom_snapshot, "TITLE");
    ASSERT_THAT(dom, ElementsAre(NodeName("TITLE")));
    const DOMNode* value = NextNode(dom_snapshot, dom[0]);
    EXPECT_THAT(value, NodeValue("JavaScript is on"));
  }
};
HEADLESS_RENDER_BROWSERTEST(JavaScriptOverrideTitle_JsEnabled);

class JavaScriptOverrideTitle_JsDisabled
    : public JavaScriptOverrideTitle_JsEnabled {
 private:
  void OverrideWebPreferences(WebPreferences* preferences) override {
    JavaScriptOverrideTitle_JsEnabled::OverrideWebPreferences(preferences);
    preferences->javascript_enabled = false;
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    auto dom = FindTags(dom_snapshot, "TITLE");
    ASSERT_THAT(dom, ElementsAre(NodeName("TITLE")));
    const DOMNode* value = NextNode(dom_snapshot, dom[0]);
    EXPECT_THAT(value, NodeValue("JavaScript is off"));
  }
};
HEADLESS_RENDER_BROWSERTEST(JavaScriptOverrideTitle_JsDisabled);

class JavaScriptConsoleErrors : public HeadlessRenderTest,
                                public runtime::ExperimentalObserver {
 private:
  HeadlessDevToolsClient* client_;
  std::vector<std::string> messages_;
  bool log_called_ = false;

  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse(SOME_URL, {R"|(
<html>
  <head>
    <script language="JavaScript">
      <![CDATA[
        function image() {
          window.open('<xsl:value-of select="/IMAGE/@href" />');
        }
      ]]>
    </script>
  </head>
  <body onload="func3()">
    <script type="text/javascript">
      func1()
    </script>
    <script type="text/javascript">
      func2();
    </script>
    <script type="text/javascript">
      console.log("Hello, Script!");
    </script>
  </body>
</html>
)|",
                                                    TEXT_HTML});
    client_ = client;
    client_->GetRuntime()->GetExperimental()->AddObserver(this);
    base::RunLoop run_loop;
    client_->GetRuntime()->GetExperimental()->Enable(run_loop.QuitClosure());
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    run_loop.Run();
    return GURL(SOME_URL);
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
  }
};
HEADLESS_RENDER_BROWSERTEST(JavaScriptConsoleErrors);

class DelayedCompletion : public HeadlessRenderTest {
 private:
  base::TimeTicks start_;

  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse(SOME_URL, {R"|(
<html>
  <body>
   <script type="text/javascript">
     setTimeout(() => {
       var div = document.getElementById('content');
       var p = document.createElement('p');
       p.textContent = 'delayed text';
       div.appendChild(p);
     }, 3000);
   </script>
    <div id="content"/>
  </body>
</html>
)|",
                                                    TEXT_HTML});
    start_ = base::TimeTicks::Now();
    return GURL(SOME_URL);
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    base::TimeTicks end = base::TimeTicks::Now();
    EXPECT_THAT(
        FindTags(dom_snapshot),
        ElementsAre(NodeName("HTML"), NodeName("HEAD"), NodeName("BODY"),
                    NodeName("SCRIPT"), NodeName("DIV"), NodeName("P")));
    auto dom = FindTags(dom_snapshot, "P");
    ASSERT_THAT(dom, ElementsAre(NodeName("P")));
    const DOMNode* value = NextNode(dom_snapshot, dom[0]);
    EXPECT_THAT(value, NodeValue("delayed text"));
    // The page delays output for 3 seconds. Due to virtual time this should
    // take significantly less actual time.
    base::TimeDelta passed = end - start_;
    EXPECT_THAT(passed.InSecondsF(), testing::Le(2.9f));
  }
};
HEADLESS_RENDER_BROWSERTEST(DelayedCompletion);

class ClientRedirectChain : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse("http://www.example.com/",
                                         {R"|(
<html>
  <head>
    <meta http-equiv="refresh" content="0; url=http://www.example.com/1"/>
    <title>Hello, World 0</title>
  </head>
  <body>http://www.example.com/</body>
</html>
)|",
                                          TEXT_HTML});
    GetProtocolHandler()->InsertResponse("http://www.example.com/1",
                                         {R"|(
<html>
  <head>
    <title>Hello, World 1</title>
    <script>
      document.location='http://www.example.com/2';
    </script>
  </head>
  <body>http://www.example.com/1</body>
</html>
)|",
                                          TEXT_HTML});
    GetProtocolHandler()->InsertResponse("http://www.example.com/2",
                                         {R"|(
<html>
  <head>
    <title>Hello, World 2</title>
    <script>
      setTimeout("document.location='http://www.example.com/3'", 1000);
    </script>
  </head>
  <body>http://www.example.com/2</body>
</html>
)|",
                                          TEXT_HTML});
    GetProtocolHandler()->InsertResponse("http://www.example.com/3",
                                         {R"|(
<html>
  <head>
    <title>Pass</title>
  </head>
  <body>
    http://www.example.com/3
    <img src="pass">
  </body>
</html>
)|",
                                          TEXT_HTML});

    return GURL("http://www.example.com/");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(GetProtocolHandler()->urls_requested(),
                ElementsAre(SOME_HOST + "/", SOME_HOST + "/1", SOME_HOST + "/2",
                            SOME_HOST + "/3", SOME_HOST + "/pass"));
    auto dom = FindTags(dom_snapshot, "TITLE");
    ASSERT_THAT(dom, ElementsAre(NodeName("TITLE")));
    const DOMNode* value = NextNode(dom_snapshot, dom[0]);
    EXPECT_THAT(value, NodeValue("Pass"));
  }
};
HEADLESS_RENDER_BROWSERTEST(ClientRedirectChain);

}  // namespace headless
