// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test.h"
#include "headless/public/devtools/domains/dom_snapshot.h"
#include "headless/public/devtools/domains/page.h"
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

std::vector<std::string> Structure(const GetSnapshotResult* snapshot) {
  return ElementsView<std::string, DOMNode>(
      *snapshot->GetDomNodes(),
      [](const auto& node) { return node.GetNodeType() == 1; },
      [](const auto& node) { return node.GetNodeName(); });
}

std::vector<std::string> Contents(const GetSnapshotResult* snapshot) {
  return ElementsView<std::string, DOMNode>(
      *snapshot->GetDomNodes(),
      [](const auto& node) { return node.GetNodeType() == 3; },
      [](const auto& node) { return node.GetNodeValue(); });
}

std::vector<std::string> TextLayout(const GetSnapshotResult* snapshot) {
  return ElementsView<std::string, LayoutTreeNode>(
      *snapshot->GetLayoutTreeNodes(),
      [](const auto& node) { return node.HasLayoutText(); },
      [](const auto& node) { return node.GetLayoutText(); });
}

}  // namespace

class HelloWorldTest : public HeadlessRenderTest {
 private:
  GURL GetPageUrl(HeadlessDevToolsClient* client) override {
    return embedded_test_server()->GetURL("/hello.html");
  }

  void VerifyDom(GetSnapshotResult* dom_snapshot) override {
    EXPECT_THAT(Structure(dom_snapshot),
                testing::ElementsAre("HTML", "HEAD", "BODY", "H1"));
    EXPECT_THAT(Contents(dom_snapshot),
                testing::ElementsAre("Hello headless world!", "\n"));
    EXPECT_THAT(TextLayout(dom_snapshot),
                testing::ElementsAre("Hello headless world!"));
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

}  // namespace headless
