// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "components/html_viewer/public/interfaces/test_html_viewer.mojom.h"
#include "components/view_manager/public/cpp/tests/view_manager_test_base.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "mandoline/tab/frame.h"
#include "mandoline/tab/frame_connection.h"
#include "mandoline/tab/frame_tree.h"
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "third_party/mojo_services/src/accessibility/public/interfaces/accessibility.mojom.h"

using mandoline::Frame;
using mandoline::FrameConnection;
using mandoline::FrameTree;
using mandoline::FrameTreeClient;

namespace mojo {

namespace {

// Switch to enable out of process iframes.
const char kOOPIF[] = "oopifs";

bool EnableOOPIFs() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kOOPIF);
}

std::string GetFrameText(ApplicationConnection* connection) {
  html_viewer::TestHTMLViewerPtr test_html_viewer;
  connection->ConnectToService(&test_html_viewer);
  std::string result;
  test_html_viewer->GetContentAsText(
      [&result](const String& mojo_string) { result = mojo_string; });
  test_html_viewer.WaitForIncomingResponse();
  return result;
}

}  // namespace

class HTMLFrameTest : public ViewManagerTestBase {
 public:
  HTMLFrameTest() {}
  ~HTMLFrameTest() override {}

 protected:
  mojo::URLRequestPtr BuildRequestForURL(const std::string& url_string) {
    const uint16_t assigned_port = http_server_->host_port_pair().port();
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From(
        base::StringPrintf(url_string.c_str(), assigned_port));
    return request.Pass();
  }

  FrameConnection* InitFrameTree(View* view, const std::string& url_string) {
    scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
    FrameConnection* result = frame_connection.get();
    ViewManagerClientPtr view_manager_client;
    frame_connection->Init(application_impl(), BuildRequestForURL(url_string),
                           &view_manager_client);
    FrameTreeClient* frame_tree_client = frame_connection->frame_tree_client();
    frame_tree_.reset(new FrameTree(view, nullptr, frame_tree_client,
                                    frame_connection.Pass()));
    view->Embed(view_manager_client.Pass());
    return result;
  }

  bool WaitForEmbedForDescendant() {
    if (embed_run_loop_)
      return false;
    embed_run_loop_.reset(new base::RunLoop);
    embed_run_loop_->Run();
    embed_run_loop_.reset();
    return true;
  }

  // ViewManagerTest:
  void SetUp() override {
    ViewManagerTestBase::SetUp();

    // Make it so we get OnEmbedForDescendant().
    window_manager()->SetEmbedRoot();

    // Start a test server.
    http_server_.reset(new net::SpawnedTestServer(
        net::SpawnedTestServer::TYPE_HTTP, net::SpawnedTestServer::kLocalhost,
        base::FilePath(FILE_PATH_LITERAL("components/test/data/html_viewer"))));
    ASSERT_TRUE(http_server_->Start());
  }
  void TearDown() override {
    frame_tree_.reset();
    http_server_.reset();
    ViewManagerTestBase::TearDown();
  }
  void OnEmbedForDescendant(View* view,
                            URLRequestPtr request,
                            ViewManagerClientPtr* client) override {
    Frame* frame = Frame::FindFirstFrameAncestor(view);
    scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
    frame_connection->Init(application_impl(), request.Pass(), client);
    FrameTreeClient* frame_tree_client = frame_connection->frame_tree_client();
    frame_tree_->CreateOrReplaceFrame(frame, view, frame_tree_client,
                                      frame_connection.Pass());
    if (embed_run_loop_)
      embed_run_loop_->Quit();
  }

  scoped_ptr<net::SpawnedTestServer> http_server_;
  scoped_ptr<FrameTree> frame_tree_;

 private:
  // A runloop specifically for OnEmbedForDescendant(). We use a separate
  // runloop here as it's possible at the time OnEmbedForDescendant() is invoked
  // a separate RunLoop is already running that we shouldn't quit.
  scoped_ptr<base::RunLoop> embed_run_loop_;

  DISALLOW_COPY_AND_ASSIGN(HTMLFrameTest);
};

TEST_F(HTMLFrameTest, HelloWorld) {
  if (!EnableOOPIFs())
    return;

  View* embed_view = window_manager()->CreateView();

  FrameConnection* root_connection = InitFrameTree(
      embed_view, "http://127.0.0.1:%u/files/page_with_single_frame.html");

  ASSERT_EQ("Page with single frame",
            GetFrameText(root_connection->application_connection()));

  // page_with_single_frame contains a child frame. The child frame should
  // create
  // a new View and Frame.
  if (embed_view->children().empty())
    ASSERT_TRUE(WaitForEmbedForDescendant());

  ASSERT_EQ(1u, embed_view->children().size());
  Frame* child_frame =
      frame_tree_->root()->FindFrame(embed_view->children()[0]->id());
  ASSERT_TRUE(child_frame);

  ASSERT_EQ("child",
            GetFrameText(static_cast<FrameConnection*>(child_frame->user_data())
                             ->application_connection()));
}

}  // namespace mojo
