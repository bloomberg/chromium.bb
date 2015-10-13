// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/ax_provider_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "components/html_viewer/blink_platform_impl.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "gin/v8_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebViewClient.h"
#include "url/gurl.h"

using blink::WebData;
using blink::WebLocalFrame;
using blink::WebFrameClient;
using blink::WebURL;
using blink::WebView;
using blink::WebViewClient;

using mojo::Array;
using mojo::AxNode;
using mojo::AxNodePtr;

namespace {

class TestWebFrameClient : public WebFrameClient {
 public:
  ~TestWebFrameClient() override {}
  void didStopLoading() override {
    base::MessageLoop::current()->QuitWhenIdle();
  }
};

class TestWebViewClient : public WebViewClient {
 public:
  bool allowsBrokenNullLayerTreeView() const override { return true; }
  virtual ~TestWebViewClient() {}
};

class AxProviderImplTest : public testing::Test {
 public:
  AxProviderImplTest()
      : message_loop_(new base::MessageLoopForUI()),
        renderer_scheduler_(scheduler::RendererScheduler::Create()) {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
    gin::V8Initializer::LoadV8Snapshot();
    gin::V8Initializer::LoadV8Natives();
#endif
    blink::initialize(
        new html_viewer::BlinkPlatformImpl(nullptr, nullptr,
                                           renderer_scheduler_.get()));
  }

  ~AxProviderImplTest() override {
    renderer_scheduler_->Shutdown();
    message_loop_.reset();
    blink::shutdown();
  }

 private:
  scoped_ptr<base::MessageLoopForUI> message_loop_;
  scoped_ptr<scheduler::RendererScheduler> renderer_scheduler_;
};

struct NodeCatcher {
  void OnNodes(Array<AxNodePtr> nodes) { this->nodes = nodes.Pass(); }
  Array<AxNodePtr> nodes;
};

AxNodePtr CreateNode(int id,
                     int parent_id,
                     int next_sibling_id,
                     const mojo::RectPtr& bounds,
                     const std::string& url,
                     const std::string& text) {
  AxNodePtr node(AxNode::New());
  node->id = id;
  node->parent_id = parent_id;
  node->next_sibling_id = next_sibling_id;
  node->bounds = bounds.Clone();

  if (!url.empty()) {
    node->link = mojo::AxLink::New();
    node->link->url = url;
  }
  if (!text.empty()) {
    node->text = mojo::AxText::New();
    node->text->content = text;
  }
  return node.Pass();
}

}  // namespace

TEST_F(AxProviderImplTest, Basic) {
  TestWebViewClient web_view_client;
  TestWebFrameClient web_frame_client;
  WebView* view = WebView::create(&web_view_client);
  view->setMainFrame(WebLocalFrame::create(blink::WebTreeScopeType::Document,
                                           &web_frame_client));
  view->mainFrame()->loadHTMLString(
      WebData(
          "<html><body>foo<a "
          "href='http://monkey.net'>bar</a>baz</body></html>"),
      WebURL(GURL("http://someplace.net")));
  base::MessageLoop::current()->Run();

  mojo::AxProviderPtr service_ptr;
  html_viewer::AxProviderImpl ax_provider_impl(view,
                                               mojo::GetProxy(&service_ptr));
  NodeCatcher catcher;
  ax_provider_impl.GetTree(
      base::Bind(&NodeCatcher::OnNodes, base::Unretained(&catcher)));

  std::map<uint32, AxNode*> lookup;
  for (size_t i = 0; i < catcher.nodes.size(); ++i) {
    auto& node = catcher.nodes[i];
    lookup[node->id] = node.get();
  }

  typedef decltype(lookup)::value_type MapEntry;
  auto is_link = [](MapEntry pair) { return !pair.second->link.is_null(); };
  auto is_text = [](MapEntry pair, const char* content) {
    return !pair.second->text.is_null() &&
           pair.second->text->content.To<std::string>() == content;
  };
  auto is_foo = [&is_text](MapEntry pair) { return is_text(pair, "foo"); };
  auto is_bar = [&is_text](MapEntry pair) { return is_text(pair, "bar"); };
  auto is_baz = [&is_text](MapEntry pair) { return is_text(pair, "baz"); };

  EXPECT_EQ(1, std::count_if(lookup.begin(), lookup.end(), is_link));
  EXPECT_EQ(1, std::count_if(lookup.begin(), lookup.end(), is_foo));
  EXPECT_EQ(1, std::count_if(lookup.begin(), lookup.end(), is_bar));
  EXPECT_EQ(1, std::count_if(lookup.begin(), lookup.end(), is_baz));

  auto root = lookup[1u];
  auto link = std::find_if(lookup.begin(), lookup.end(), is_link)->second;
  auto foo = std::find_if(lookup.begin(), lookup.end(), is_foo)->second;
  auto bar = std::find_if(lookup.begin(), lookup.end(), is_bar)->second;
  auto baz = std::find_if(lookup.begin(), lookup.end(), is_baz)->second;

  // Test basic content of each node. The properties we copy (like parent_id)
  // here are tested differently below.
  EXPECT_TRUE(CreateNode(root->id, 0, 0, root->bounds, "", "")->Equals(*root));
  EXPECT_TRUE(CreateNode(foo->id, foo->parent_id, 0, foo->bounds, "", "foo")
                  ->Equals(*foo));
  EXPECT_TRUE(CreateNode(bar->id, bar->parent_id, 0, bar->bounds, "", "bar")
                  ->Equals(*bar));
  EXPECT_TRUE(CreateNode(baz->id, baz->parent_id, 0, baz->bounds, "", "baz")
                  ->Equals(*baz));
  EXPECT_TRUE(CreateNode(link->id,
                         link->parent_id,
                         link->next_sibling_id,
                         link->bounds,
                         "http://monkey.net/",
                         "")->Equals(*link));

  auto is_descendant_of = [&lookup](uint32 id, uint32 ancestor) {
    for (; (id = lookup[id]->parent_id) != 0;) {
      if (id == ancestor)
        return true;
    }
    return false;
  };

  EXPECT_TRUE(is_descendant_of(bar->id, link->id));
  for (auto pair : lookup) {
    AxNode* node = pair.second;
    if (node != root)
      EXPECT_TRUE(is_descendant_of(node->id, 1u));
    if (node != link && node != foo && node != bar && node != baz) {
      EXPECT_TRUE(CreateNode(node->id,
                             node->parent_id,
                             node->next_sibling_id,
                             node->bounds,
                             "",
                             ""));
    }
  }

  // TODO(aa): Test bounds.
  // TODO(aa): Test sibling ordering of foo/bar/baz.

  view->close();
}
