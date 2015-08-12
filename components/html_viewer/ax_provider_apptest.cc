// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "components/view_manager/public/cpp/tests/view_manager_test_base.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo_services/src/accessibility/public/interfaces/accessibility.mojom.h"

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

// Switch to enable out of process iframes.
const char kOOPIF[] = "oopifs";

bool EnableOOPIFs() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kOOPIF);
}

class TestFrameTreeServer : public mandoline::FrameTreeServer {
 public:
  TestFrameTreeServer() {}
  ~TestFrameTreeServer() override {}

  // mandoline::FrameTreeServer:
  void PostMessageEventToFrame(uint32_t source_frame_id,
                               uint32_t target_frame_id,
                               mandoline::HTMLMessageEventPtr event) override {}
  void LoadingStarted(uint32_t frame_id) override {}
  void LoadingStopped(uint32_t frame_id) override {}
  void ProgressChanged(uint32_t frame_id, double progress) override {}
  void SetClientProperty(uint32_t frame_id,
                         const mojo::String& name,
                         mojo::Array<uint8_t> value) override {}
  void OnCreatedFrame(uint32_t parent_id,
                      uint32_t frame_id,
                      mojo::Map<mojo::String, mojo::Array<uint8_t>>
                          client_properties) override {}
  void RequestNavigate(mandoline::NavigationTargetType target_type,
                       uint32_t target_frame_id,
                       mojo::URLRequestPtr request) override {}
  void DidNavigateLocally(uint32_t frame_id, const mojo::String& url) override {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFrameTreeServer);
};

}  // namespace

using AXProviderTest = ViewManagerTestBase;

TEST_F(AXProviderTest, HelloWorld) {
  // Start a test server for net/data/test.html access.
  net::SpawnedTestServer server(
      net::SpawnedTestServer::TYPE_HTTP, net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("net/data")));
  ASSERT_TRUE(server.Start());

  // Connect to the URL through the mojo:html_viewer content handler.
  const uint16_t assigned_port = server.host_port_pair().port();
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(
      base::StringPrintf("http://127.0.0.1:%u/files/test.html", assigned_port));
  scoped_ptr<ApplicationConnection> connection =
      application_impl()->ConnectToApplication(request.Pass());

  // Embed the html_viewer in a View.
  ViewManagerClientPtr view_manager_client;
  connection->ConnectToService(&view_manager_client);
  View* embed_view = window_manager()->CreateView();
  embed_view->Embed(view_manager_client.Pass());

  if (EnableOOPIFs()) {
    TestFrameTreeServer frame_tree_server;
    mandoline::FrameTreeServerPtr frame_tree_server_ptr;
    mojo::Binding<mandoline::FrameTreeServer> frame_tree_server_binding(
        &frame_tree_server);
    frame_tree_server_binding.Bind(GetProxy(&frame_tree_server_ptr).Pass());

    mojo::Array<mandoline::FrameDataPtr> array(1u);
    array[0] = mandoline::FrameData::New().Pass();
    array[0]->frame_id = embed_view->id();
    array[0]->parent_id = 0u;

    mandoline::FrameTreeClientPtr frame_tree_client;
    connection->ConnectToService(&frame_tree_client);
    frame_tree_client->OnConnect(frame_tree_server_ptr.Pass(), 1u,
                                 array.Pass());
  }

  // Connect to the AxProvider of the HTML document and get the AxTree.
  AxProviderPtr ax_provider;
  connection->ConnectToService(&ax_provider);
  Array<AxNodePtr> ax_tree;
  ax_provider->GetTree([&ax_tree](Array<AxNodePtr> tree) {
                         ax_tree = tree.Pass();
                         EXPECT_TRUE(QuitRunLoop());
                       });
  ASSERT_TRUE(DoRunLoopWithTimeout());

  EXPECT_TRUE(AxTreeContainsText(ax_tree, "Hello "));
  EXPECT_TRUE(AxTreeContainsText(ax_tree, "World!"));
  EXPECT_FALSE(AxTreeContainsText(ax_tree, "foo"));
}

}  // namespace mojo
