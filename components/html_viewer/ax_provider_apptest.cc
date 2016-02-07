// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "components/mus/public/cpp/tests/window_server_test_base.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/web_view/public/interfaces/frame.mojom.h"
#include "mojo/converters/network/network_type_converters.h"
#include "mojo/services/accessibility/public/interfaces/accessibility.mojom.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

namespace {

// Returns true if the tree contains a text node with contents matching |text|.
bool AxTreeContainsText(const Array<AxNodePtr>& tree, const String& text) {
  for (size_t i = 0; i < tree.size(); ++i) {
    if (!tree[i]->text.is_null() && tree[i]->text->content == text)
      return true;
  }
  return false;
}

class TestFrame : public web_view::mojom::Frame {
 public:
  TestFrame() {}
  ~TestFrame() override {}

  // web_view::mojom::Frame:
  void PostMessageEventToFrame(
      uint32_t target_frame_id,
      web_view::mojom::HTMLMessageEventPtr event) override {}
  void LoadingStateChanged(bool loading, double progress) override {}
  void TitleChanged(const mojo::String& title) override {}
  void DidCommitProvisionalLoad() override {}
  void SetClientProperty(const mojo::String& name,
                         mojo::Array<uint8_t> value) override {}
  void OnCreatedFrame(
      mojo::InterfaceRequest<web_view::mojom::Frame> frame_request,
      web_view::mojom::FrameClientPtr client,
      uint32_t frame_id,
      mojo::Map<mojo::String, mojo::Array<uint8_t>> client_properties)
      override {}
  void RequestNavigate(web_view::mojom::NavigationTargetType target_type,
                       uint32_t target_frame_id,
                       mojo::URLRequestPtr request) override {}
  void DidNavigateLocally(const mojo::String& url) override {}
  void DispatchLoadEventToParent() override {}
  void OnFindInFrameCountUpdated(int32_t request_id,
                                 int32_t count,
                                 bool final_update) override {}
  void OnFindInPageSelectionUpdated(int32_t request_id,
                                    int32_t active_match_ordinal) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFrame);
};

}  // namespace

using AXProviderTest = mus::WindowServerTestBase;

TEST_F(AXProviderTest, HelloWorld) {
  // Start a test server for net/data/test.html access.
  net::EmbeddedTestServer server;
  server.ServeFilesFromSourceDirectory("net/data");
  ASSERT_TRUE(server.Start());

  // Connect to the URL through the mojo:html_viewer content handler.
  scoped_ptr<Connection> connection =
      shell()->ConnectToApplication(server.GetURL("/test.html").spec());

  // Embed the html_viewer in a Window.
  mus::mojom::WindowTreeClientPtr tree_client;
  connection->ConnectToService(&tree_client);
  mus::Window* embed_window = window_manager()->NewWindow();
  embed_window->Embed(std::move(tree_client));

  TestFrame frame;
  web_view::mojom::FramePtr frame_ptr;
  mojo::Binding<web_view::mojom::Frame> frame_binding(&frame);
  frame_binding.Bind(GetProxy(&frame_ptr));

  mojo::Array<web_view::mojom::FrameDataPtr> array(1u);
  array[0] = web_view::mojom::FrameData::New();
  array[0]->frame_id = embed_window->id();
  array[0]->parent_id = 0u;

  web_view::mojom::FrameClientPtr frame_client;
  connection->ConnectToService(&frame_client);
  frame_client->OnConnect(
      std::move(frame_ptr), 1u, embed_window->id(),
      web_view::mojom::WindowConnectType::USE_NEW, std::move(array),
      base::TimeTicks::Now().ToInternalValue(), base::Closure());

  // Connect to the AxProvider of the HTML document and get the AxTree.
  AxProviderPtr ax_provider;
  connection->ConnectToService(&ax_provider);
  Array<AxNodePtr> ax_tree;
  ax_provider->GetTree([&ax_tree](Array<AxNodePtr> tree) {
    ax_tree = std::move(tree);
    EXPECT_TRUE(QuitRunLoop());
  });
  ASSERT_TRUE(DoRunLoopWithTimeout());

  EXPECT_TRUE(AxTreeContainsText(ax_tree, "Hello "));
  EXPECT_TRUE(AxTreeContainsText(ax_tree, "World!"));
  EXPECT_FALSE(AxTreeContainsText(ax_tree, "foo"));
}

}  // namespace mojo
