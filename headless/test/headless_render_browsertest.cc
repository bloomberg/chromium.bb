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

// TODO(dats): For some reason we are missing all HTTP redirects.
// crbug.com/789298
#define DISABLE_HTTP_REDIRECTS_CHECKS

namespace headless {

namespace {

const std::string SOME_HOST = "http://www.example.com";
constexpr char SOME_URL[] = "http://example.com/foobar";
constexpr char TEXT_HTML[] = "text/html";

using dom_snapshot::GetSnapshotResult;
using dom_snapshot::DOMNode;
using dom_snapshot::LayoutTreeNode;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using net::test_server::BasicHttpResponse;
using net::test_server::RawHttpResponse;
using page::FrameScheduledNavigationReason;
using testing::ElementsAre;
using testing::UnorderedElementsAre;
using testing::Eq;
using testing::Ne;
using testing::StartsWith;

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

MATCHER_P(RedirectUrl, expected, "") {
  return arg.first == expected;
}

MATCHER_P(RedirectReason, expected, "") {
  return arg.second == expected;
}

const DOMNode* FindTag(const GetSnapshotResult* snapshot, const char* name) {
  auto tags = FindTags(snapshot, name);
  if (tags.empty())
    return nullptr;
  EXPECT_THAT(tags, ElementsAre(NodeName(name)));
  return tags[0];
}

TestInMemoryProtocolHandler::Response HttpRedirect(
    int code,
    const std::string& url,
    const std::string& status = "Moved") {
  CHECK(code >= 300 && code < 400);
  std::stringstream str;
  str << "HTTP/1.1 " << code << " " << status << "\r\nLocation: " << url
      << "\r\n\r\n";
  return TestInMemoryProtocolHandler::Response(str.str());
}

TestInMemoryProtocolHandler::Response HttpOk(const std::string& html) {
  return TestInMemoryProtocolHandler::Response(html, TEXT_HTML);
}

}  // namespace

class HelloWorldTest : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse(SOME_URL, HttpOk(R"|(<!doctype html>
<h1>Hello headless world!</h1>
)|"));
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
    EXPECT_THAT(GetProtocolHandler()->urls_requested(), ElementsAre(SOME_URL));
    EXPECT_FALSE(main_frame_.empty());
    EXPECT_TRUE(unconfirmed_frame_redirects_.empty());
    EXPECT_TRUE(confirmed_frame_redirects_.empty());
    EXPECT_THAT(frames_[main_frame_].size(), Eq(1u));
    const auto& frame = frames_[main_frame_][0];
    EXPECT_THAT(frame->GetUrl(), Eq(SOME_URL));
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
    GetProtocolHandler()->InsertResponse(SOME_URL, HttpOk(R"|(
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
)|"));
    return GURL(SOME_URL);
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    const DOMNode* value =
        NextNode(dom_snapshot, FindTag(dom_snapshot, "TITLE"));
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
    const DOMNode* value =
        NextNode(dom_snapshot, FindTag(dom_snapshot, "TITLE"));
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
    GetProtocolHandler()->InsertResponse(SOME_URL, HttpOk(R"|(
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
)|"));
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
    GetProtocolHandler()->InsertResponse(SOME_URL, HttpOk(R"|(
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
)|"));
    start_ = base::TimeTicks::Now();
    return GURL(SOME_URL);
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    base::TimeTicks end = base::TimeTicks::Now();
    EXPECT_THAT(
        FindTags(dom_snapshot),
        ElementsAre(NodeName("HTML"), NodeName("HEAD"), NodeName("BODY"),
                    NodeName("SCRIPT"), NodeName("DIV"), NodeName("P")));
    const DOMNode* value = NextNode(dom_snapshot, FindTag(dom_snapshot, "P"));
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
    GetProtocolHandler()->InsertResponse("http://www.example.com/", HttpOk(R"|(
<html>
  <head>
    <meta http-equiv="refresh" content="0; url=http://www.example.com/1"/>
    <title>Hello, World 0</title>
  </head>
  <body>http://www.example.com/</body>
</html>
)|"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/1", HttpOk(R"|(
<html>
  <head>
    <title>Hello, World 1</title>
    <script>
      document.location='http://www.example.com/2';
    </script>
  </head>
  <body>http://www.example.com/1</body>
</html>
)|"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/2", HttpOk(R"|(
<html>
  <head>
    <title>Hello, World 2</title>
    <script>
      setTimeout("document.location='http://www.example.com/3'", 1000);
    </script>
  </head>
  <body>http://www.example.com/2</body>
</html>
)|"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/3", HttpOk(R"|(
<html>
  <head>
    <title>Pass</title>
  </head>
  <body>
    http://www.example.com/3
    <img src="pass">
  </body>
</html>
)|"));
    return GURL("http://www.example.com/");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(
        GetProtocolHandler()->urls_requested(),
        ElementsAre("http://www.example.com/", "http://www.example.com/1",
                    "http://www.example.com/2", "http://www.example.com/3",
                    "http://www.example.com/pass"));
    const DOMNode* value =
        NextNode(dom_snapshot, FindTag(dom_snapshot, "TITLE"));
    EXPECT_THAT(value, NodeValue("Pass"));
    EXPECT_THAT(
        confirmed_frame_redirects_[main_frame_],
        ElementsAre(
            RedirectReason(FrameScheduledNavigationReason::META_TAG_REFRESH),
            RedirectReason(FrameScheduledNavigationReason::SCRIPT_INITIATED),
            RedirectReason(FrameScheduledNavigationReason::SCRIPT_INITIATED)));
    EXPECT_THAT(frames_[main_frame_].size(), Eq(4u));
  }
};
HEADLESS_RENDER_BROWSERTEST(ClientRedirectChain);

class ClientRedirectChain_NoJs : public ClientRedirectChain {
 private:
  void OverrideWebPreferences(WebPreferences* preferences) override {
    ClientRedirectChain::OverrideWebPreferences(preferences);
    preferences->javascript_enabled = false;
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(
        GetProtocolHandler()->urls_requested(),
        ElementsAre("http://www.example.com/", "http://www.example.com/1"));
    const DOMNode* value =
        NextNode(dom_snapshot, FindTag(dom_snapshot, "TITLE"));
    EXPECT_THAT(value, NodeValue("Hello, World 1"));
    EXPECT_THAT(confirmed_frame_redirects_[main_frame_],
                ElementsAre(RedirectReason(
                    FrameScheduledNavigationReason::META_TAG_REFRESH)));
    EXPECT_THAT(frames_[main_frame_].size(), Eq(2u));
  }
};
HEADLESS_RENDER_BROWSERTEST(ClientRedirectChain_NoJs);

class ServerRedirectChain : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse(
        "http://www.example.com/",
        HttpRedirect(302, "http://www.example.com/1"));
    GetProtocolHandler()->InsertResponse(
        "http://www.example.com/1",
        HttpRedirect(301, "http://www.example.com/2"));
    GetProtocolHandler()->InsertResponse(
        "http://www.example.com/2",
        HttpRedirect(302, "http://www.example.com/3"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/3",
                                         HttpOk("<p>Pass</p>"));
    return GURL("http://www.example.com/");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(
        GetProtocolHandler()->urls_requested(),
        ElementsAre("http://www.example.com/", "http://www.example.com/1",
                    "http://www.example.com/2", "http://www.example.com/3"));
    const DOMNode* value = NextNode(dom_snapshot, FindTag(dom_snapshot, "P"));
    EXPECT_THAT(value, NodeValue("Pass"));
#ifndef DISABLE_HTTP_REDIRECTS_CHECKS
    EXPECT_THAT(
        confirmed_frame_redirects_[main_frame_],
        ElementsAre(
            RedirectReason(FrameScheduledNavigationReason::HTTP_HEADER_REFRESH),
            RedirectReason(FrameScheduledNavigationReason::HTTP_HEADER_REFRESH),
            RedirectReason(
                FrameScheduledNavigationReason::HTTP_HEADER_REFRESH)));
    EXPECT_THAT(frames_[main_frame_].size(), Eq(4u));
#endif  // #ifndef DISABLE_HTTP_REDIRECTS_CHECKS
  }
};
HEADLESS_RENDER_BROWSERTEST(ServerRedirectChain);

class ServerRedirectToFailure : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse(
        "http://www.example.com/",
        HttpRedirect(302, "http://www.example.com/1"));
    GetProtocolHandler()->InsertResponse(
        "http://www.example.com/1",
        HttpRedirect(301, "http://www.example.com/FAIL"));
    return GURL("http://www.example.com/");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(
        GetProtocolHandler()->urls_requested(),
        ElementsAre("http://www.example.com/", "http://www.example.com/1",
                    "http://www.example.com/FAIL"));
  }
};
HEADLESS_RENDER_BROWSERTEST(ServerRedirectToFailure);

class ServerRedirectRelativeChain : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse(
        "http://www.example.com/",
        HttpRedirect(302, "http://www.mysite.com/1"));
    GetProtocolHandler()->InsertResponse("http://www.mysite.com/1",
                                         HttpRedirect(301, "/2"));
    GetProtocolHandler()->InsertResponse("http://www.mysite.com/2",
                                         HttpOk("<p>Pass</p>"));
    return GURL("http://www.example.com/");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(
        GetProtocolHandler()->urls_requested(),
        ElementsAre("http://www.example.com/", "http://www.mysite.com/1",
                    "http://www.mysite.com/2"));
    const DOMNode* value = NextNode(dom_snapshot, FindTag(dom_snapshot, "P"));
    EXPECT_THAT(value, NodeValue("Pass"));
  }
};
HEADLESS_RENDER_BROWSERTEST(ServerRedirectRelativeChain);

class MixedRedirectChain : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse("http://www.example.com/", HttpOk(R"|(
 <html>
   <head>
     <meta http-equiv="refresh" content="0; url=http://www.example.com/1"/>
     <title>Hello, World 0</title>
   </head>
   <body>http://www.example.com/</body>
 </html>
 )|"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/1", HttpOk(R"|(
 <html>
   <head>
     <title>Hello, World 1</title>
     <script>
       document.location='http://www.example.com/2';
     </script>
   </head>
   <body>http://www.example.com/1</body>
 </html>
 )|"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/2",
                                         HttpRedirect(302, "3"));
    GetProtocolHandler()->InsertResponse(
        "http://www.example.com/3",
        HttpRedirect(301, "http://www.example.com/4"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/4",
                                         HttpOk("<p>Pass</p>"));
    return GURL("http://www.example.com/");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(
        GetProtocolHandler()->urls_requested(),
        ElementsAre("http://www.example.com/", "http://www.example.com/1",
                    "http://www.example.com/2", "http://www.example.com/3",
                    "http://www.example.com/4"));
    const DOMNode* value = NextNode(dom_snapshot, FindTag(dom_snapshot, "P"));
    EXPECT_THAT(value, NodeValue("Pass"));
  }
};
HEADLESS_RENDER_BROWSERTEST(MixedRedirectChain);

class FramesRedirectChain : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse(
        "http://www.example.com/",
        HttpRedirect(302, "http://www.example.com/1"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/1", HttpOk(R"|(
<html>
 <frameset>
  <frame src="http://www.example.com/frameA/">
  <frame src="http://www.example.com/frameB/">
 </frameset>
</html>
)|"));

    // Frame A
    GetProtocolHandler()->InsertResponse("http://www.example.com/frameA/",
                                         HttpOk(R"|(
<html>
 <head>
  <script>document.location='http://www.example.com/frameA/1'</script>
 </head>
 <body>HELLO WORLD 1</body>
</html>
)|"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/frameA/1",
                                         HttpRedirect(301, "/frameA/2"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/frameA/2",
                                         HttpOk("<p>FRAME A</p>"));

    // Frame B
    GetProtocolHandler()->InsertResponse("http://www.example.com/frameB/",
                                         HttpOk(R"|(
<html>
 <head><title>HELLO WORLD 2</title></head>
 <body>
  <iframe src="http://www.example.com/iframe/"></iframe>
 </body>
</html>
)|"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/iframe/",
                                         HttpOk(R"|(
<html>
 <head>
  <script>document.location='http://www.example.com/iframe/1'</script>
 </head>
 <body>HELLO WORLD 1</body>
</html>
)|"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/iframe/1",
                                         HttpRedirect(302, "/iframe/2"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/iframe/2",
                                         HttpRedirect(301, "3"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/iframe/3",
                                         HttpOk("<p>IFRAME B</p>"));
    return GURL("http://www.example.com/");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(
        GetProtocolHandler()->urls_requested(),
        UnorderedElementsAre(
            "http://www.example.com/", "http://www.example.com/1",
            "http://www.example.com/frameA/", "http://www.example.com/frameA/1",
            "http://www.example.com/frameA/2", "http://www.example.com/frameB/",
            "http://www.example.com/iframe/", "http://www.example.com/iframe/1",
            "http://www.example.com/iframe/2",
            "http://www.example.com/iframe/3"));
    auto dom = FindTags(dom_snapshot, "P");
    EXPECT_THAT(dom, ElementsAre(NodeName("P"), NodeName("P")));
    EXPECT_THAT(NextNode(dom_snapshot, dom[0]), NodeValue("FRAME A"));
    EXPECT_THAT(NextNode(dom_snapshot, dom[1]), NodeValue("IFRAME B"));

    const page::Frame* main_frame = nullptr;
    const page::Frame* a_frame = nullptr;
    const page::Frame* b_frame = nullptr;
    const page::Frame* i_frame = nullptr;
    EXPECT_THAT(frames_.size(), Eq(4u));
    for (const auto& it : frames_) {
      if (it.second.back()->GetUrl() == "http://www.example.com/1")
        main_frame = it.second.back().get();
      else if (it.second.back()->GetUrl() == "http://www.example.com/frameA/2")
        a_frame = it.second.back().get();
      else if (it.second.back()->GetUrl() == "http://www.example.com/frameB/")
        b_frame = it.second.back().get();
      else if (it.second.back()->GetUrl() == "http://www.example.com/iframe/3")
        i_frame = it.second.back().get();
      else
        ADD_FAILURE() << "Unexpected frame URL: " << it.second.back()->GetUrl();
    }

#ifndef DISABLE_HTTP_REDIRECTS_CHECKS
    EXPECT_THAT(frames_[main_frame->GetId()].size(), Eq(2u));
    EXPECT_THAT(frames_[a_frame->GetId()].size(), Eq(3u));
    EXPECT_THAT(frames_[b_frame->GetId()].size(), Eq(1u));
    EXPECT_THAT(frames_[i_frame->GetId()].size(), Eq(4u));
    EXPECT_THAT(confirmed_frame_redirects_[main_frame->GetId()],
                ElementsAre(RedirectReason(
                    FrameScheduledNavigationReason::HTTP_HEADER_REFRESH)));
    EXPECT_THAT(
        confirmed_frame_redirects_[a_frame->GetId()],
        ElementsAre(
            RedirectReason(FrameScheduledNavigationReason::SCRIPT_INITIATED),
            RedirectReason(
                FrameScheduledNavigationReason::HTTP_HEADER_REFRESH)));
    EXPECT_THAT(
        confirmed_frame_redirects_[i_frame->GetId()],
        ElementsAre(
            RedirectReason(FrameScheduledNavigationReason::SCRIPT_INITIATED),
            RedirectReason(FrameScheduledNavigationReason::HTTP_HEADER_REFRESH),
            RedirectReason(
                FrameScheduledNavigationReason::HTTP_HEADER_REFRESH)));
#endif  // #ifndef DISABLE_HTTP_REDIRECTS_CHECKS
  }
};
HEADLESS_RENDER_BROWSERTEST(FramesRedirectChain);

class DoubleRedirect : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse("http://www.example.com/", HttpOk(R"|(
<html>
  <head>
    <title>Hello, World 1</title>
    <script>
      document.location='http://www.example.com/1';
      document.location='http://www.example.com/2';
    </script>
  </head>
  <body>http://www.example.com/1</body>
</html>
)|"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/2",
                                         HttpOk("<p>Pass</p>"));
    return GURL("http://www.example.com/");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(
        GetProtocolHandler()->urls_requested(),
        ElementsAre("http://www.example.com/", "http://www.example.com/2"));
    EXPECT_THAT(NextNode(dom_snapshot, FindTag(dom_snapshot, "P")),
                NodeValue("Pass"));
    EXPECT_THAT(confirmed_frame_redirects_[main_frame_],
                ElementsAre(RedirectReason(
                    FrameScheduledNavigationReason::SCRIPT_INITIATED)));
    EXPECT_THAT(frames_[main_frame_].size(), Eq(2u));
  }
};
HEADLESS_RENDER_BROWSERTEST(DoubleRedirect);

class RedirectAfterCompletion : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse("http://www.example.com/", HttpOk(R"|(
<html>
 <head>
  <meta http-equiv='refresh' content='120; url=http://www.example.com/1'>
 </head>
 <body><p>Pass</p></body>
</html>
)|"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/1",
                                         HttpOk("<p>Fail</p>"));
    return GURL("http://www.example.com/");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(GetProtocolHandler()->urls_requested(),
                ElementsAre("http://www.example.com/"));
    EXPECT_THAT(NextNode(dom_snapshot, FindTag(dom_snapshot, "P")),
                NodeValue("Pass"));
    EXPECT_THAT(confirmed_frame_redirects_[main_frame_], ElementsAre());
    EXPECT_THAT(frames_[main_frame_].size(), Eq(1u));
  }
};
HEADLESS_RENDER_BROWSERTEST(RedirectAfterCompletion);

class RedirectPostMethod : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse("http://www.example.com/", HttpOk(R"|(
<html>
 <body onload='document.forms[0].submit();'>
  <form action='1' method='post'>
   <input name='foo' value='bar'>
  </form>
 </body>
</html>
)|"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/1",
                                         HttpRedirect(307, "/2"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/2",
                                         HttpOk("<p>Pass</p>"));
    return GURL("http://www.example.com/");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(
        GetProtocolHandler()->urls_requested(),
        ElementsAre("http://www.example.com/", "http://www.example.com/1",
                    "http://www.example.com/2"));
    EXPECT_THAT(NextNode(dom_snapshot, FindTag(dom_snapshot, "P")),
                NodeValue("Pass"));
  }
};
HEADLESS_RENDER_BROWSERTEST(RedirectPostMethod);

class RedirectKeepsFragment : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse("http://www.example.com/#foo",
                                         HttpRedirect(302, "/1"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/1#foo",
                                         HttpRedirect(302, "/2"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/2#foo",
                                         HttpOk(R"|(
<body>
 <p id="content"></p>
 <script>
  document.getElementById('content').textContent = window.location.href;
 </script>
</body>
)|"));
    return GURL("http://www.example.com/#foo");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(GetProtocolHandler()->urls_requested(),
                ElementsAre("http://www.example.com/#foo",
                            "http://www.example.com/1#foo",
                            "http://www.example.com/2#foo"));
    EXPECT_THAT(NextNode(dom_snapshot, FindTag(dom_snapshot, "P")),
                NodeValue("http://www.example.com/2#foo"));
  }
};
HEADLESS_RENDER_BROWSERTEST(RedirectKeepsFragment);

class RedirectReplacesFragment : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse("http://www.example.com/#foo",
                                         HttpRedirect(302, "/1#bar"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/1#bar",
                                         HttpRedirect(302, "/2"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/2#bar",
                                         HttpOk(R"|(
<body>
 <p id="content"></p>
 <script>
  document.getElementById('content').textContent = window.location.href;
 </script>
</body>
)|"));
    return GURL("http://www.example.com/#foo");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(GetProtocolHandler()->urls_requested(),
                ElementsAre("http://www.example.com/#foo",
                            "http://www.example.com/1#bar",
                            "http://www.example.com/2#bar"));
    EXPECT_THAT(NextNode(dom_snapshot, FindTag(dom_snapshot, "P")),
                NodeValue("http://www.example.com/2#bar"));
  }
};
HEADLESS_RENDER_BROWSERTEST(RedirectReplacesFragment);

class RedirectNewFragment : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    GetProtocolHandler()->InsertResponse("http://www.example.com/",
                                         HttpRedirect(302, "/1#foo"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/1#foo",
                                         HttpRedirect(302, "/2"));
    GetProtocolHandler()->InsertResponse("http://www.example.com/2#foo",
                                         HttpOk(R"|(
<body>
 <p id="content"></p>
 <script>
  document.getElementById('content').textContent = window.location.href;
 </script>
</body>
)|"));
    return GURL("http://www.example.com/");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(
        GetProtocolHandler()->urls_requested(),
        ElementsAre("http://www.example.com/", "http://www.example.com/1#foo",
                    "http://www.example.com/2#foo"));
    EXPECT_THAT(NextNode(dom_snapshot, FindTag(dom_snapshot, "P")),
                NodeValue("http://www.example.com/2#foo"));
  }
};
HEADLESS_RENDER_BROWSERTEST(RedirectNewFragment);

}  // namespace headless
