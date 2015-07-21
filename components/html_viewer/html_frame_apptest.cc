// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
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

const char kAddFrameWithEmptyPageScript[] =
    "var iframe = document.createElement(\"iframe\");"
    "iframe.src = \"http://127.0.0.1:%u/files/empty_page.html\";"
    "document.body.appendChild(iframe);";

bool EnableOOPIFs() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kOOPIF);
}

mojo::ApplicationConnection* ApplicationConnectionForFrame(Frame* frame) {
  return static_cast<FrameConnection*>(frame->user_data())
      ->application_connection();
}

std::string GetFrameText(ApplicationConnection* connection) {
  html_viewer::TestHTMLViewerPtr test_html_viewer;
  connection->ConnectToService(&test_html_viewer);
  std::string result;
  test_html_viewer->GetContentAsText([&result](const String& mojo_string) {
    result = mojo_string;
    ASSERT_TRUE(ViewManagerTestBase::QuitRunLoop());
  });
  if (!ViewManagerTestBase::DoRunLoopWithTimeout())
    ADD_FAILURE() << "Timed out waiting for execute to complete";
  //  test_html_viewer.WaitForIncomingResponse();
  return result;
}

scoped_ptr<base::Value> ExecuteScript(ApplicationConnection* connection,
                                      const std::string& script) {
  html_viewer::TestHTMLViewerPtr test_html_viewer;
  connection->ConnectToService(&test_html_viewer);
  scoped_ptr<base::Value> result;
  test_html_viewer->ExecuteScript(script, [&result](const String& json_string) {
    result = base::JSONReader::Read(json_string.To<std::string>());
    ASSERT_TRUE(ViewManagerTestBase::QuitRunLoop());
  });
  if (!ViewManagerTestBase::DoRunLoopWithTimeout())
    ADD_FAILURE() << "Timed out waiting for execute to complete";
  return result.Pass();
}

}  // namespace

class HTMLFrameTest : public ViewManagerTestBase {
 public:
  HTMLFrameTest() {}
  ~HTMLFrameTest() override {}

 protected:
  Frame* LoadEmptyPageAndCreateFrame() {
    View* embed_view = window_manager()->CreateView();
    FrameConnection* root_connection =
        InitFrameTree(embed_view, "http://127.0.0.1:%u/files/empty_page.html");
    const std::string frame_text =
        GetFrameText(root_connection->application_connection());
    if (frame_text != "child") {
      ADD_FAILURE() << "unexpected text " << frame_text;
      return nullptr;
    }

    // Dynamically add a new frame.
    ExecuteScript(root_connection->application_connection(),
                  AddPortToString(kAddFrameWithEmptyPageScript));

    // Wait for the frame to appear.
    if (frame_tree_->root()->children().empty() &&
        !WaitForEmbedForDescendant()) {
      ADD_FAILURE() << "timed out waiting for child";
      return nullptr;
    }

    if (embed_view->children().size() != 1u) {
      ADD_FAILURE() << "unexpected number of children "
                    << embed_view->children().size();
      return nullptr;
    }
    return frame_tree_->root()->FindFrame(embed_view->children()[0]->id());
  }

  std::string AddPortToString(const std::string& string) {
    const uint16_t assigned_port = http_server_->host_port_pair().port();
    return base::StringPrintf(string.c_str(), assigned_port);
  }

  mojo::URLRequestPtr BuildRequestForURL(const std::string& url_string) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From(AddPortToString(url_string));
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

TEST_F(HTMLFrameTest, PageWithSingleFrame) {
  if (!EnableOOPIFs())
    return;

  View* embed_view = window_manager()->CreateView();

  FrameConnection* root_connection = InitFrameTree(
      embed_view, "http://127.0.0.1:%u/files/page_with_single_frame.html");

  ASSERT_EQ("Page with single frame",
            GetFrameText(root_connection->application_connection()));

  // page_with_single_frame contains a child frame. The child frame should
  // create a new View and Frame.
  if (frame_tree_->root()->children().empty())
    ASSERT_TRUE(WaitForEmbedForDescendant());

  ASSERT_EQ(1u, embed_view->children().size());
  Frame* child_frame =
      frame_tree_->root()->FindFrame(embed_view->children()[0]->id());
  ASSERT_TRUE(child_frame);

  ASSERT_EQ("child",
            GetFrameText(static_cast<FrameConnection*>(child_frame->user_data())
                             ->application_connection()));
}

namespace {}  // namespace

TEST_F(HTMLFrameTest, DynamicallyAddFrameAndVerifyParent) {
  if (!EnableOOPIFs())
    return;

  Frame* child_frame = LoadEmptyPageAndCreateFrame();
  ASSERT_TRUE(child_frame);

  mojo::ApplicationConnection* child_frame_connection =
      ApplicationConnectionForFrame(child_frame);

  ASSERT_EQ("child", GetFrameText(child_frame_connection));
  // The child's parent should not be itself:
  const char kGetWindowParentNameScript[] =
      "window.parent == window ? 'parent is self' : 'parent not self';";
  scoped_ptr<base::Value> parent_value(
      ExecuteScript(child_frame_connection, kGetWindowParentNameScript));
  ASSERT_TRUE(parent_value->IsType(base::Value::TYPE_LIST));
  base::ListValue* parent_list;
  ASSERT_TRUE(parent_value->GetAsList(&parent_list));
  ASSERT_EQ(1u, parent_list->GetSize());
  std::string parent_name;
  ASSERT_TRUE(parent_list->GetString(0u, &parent_name));
  EXPECT_EQ("parent not self", parent_name);
}

TEST_F(HTMLFrameTest, DynamicallyAddFrameAndSeeNameChange) {
  if (!EnableOOPIFs())
    return;

  Frame* child_frame = LoadEmptyPageAndCreateFrame();
  ASSERT_TRUE(child_frame);

  mojo::ApplicationConnection* child_frame_connection =
      ApplicationConnectionForFrame(child_frame);

  // Change the name of the child's window.
  ExecuteScript(child_frame_connection, "window.name = 'new_child';");

  // Eventually the parent should see the change. There is no convenient way
  // to observer this change, so we repeatedly ask for it and timeout if we
  // never get the right value.
  const base::TimeTicks start_time(base::TimeTicks::Now());
  std::string find_window_result;
  do {
    scoped_ptr<base::Value> script_value(
        ExecuteScript(ApplicationConnectionForFrame(frame_tree_->root()),
                      "window.frames['new_child'] != null ? 'found frame' : "
                      "'unable to find frame';"));
    if (script_value->IsType(base::Value::TYPE_LIST)) {
      base::ListValue* script_value_as_list;
      if (script_value->GetAsList(&script_value_as_list) &&
          script_value_as_list->GetSize() == 1) {
        script_value_as_list->GetString(0u, &find_window_result);
      }
    }
  } while (find_window_result != "found frame" &&
           base::TimeTicks::Now() - start_time <
               TestTimeouts::action_timeout());
  EXPECT_EQ("found frame", find_window_result);
}

}  // namespace mojo
