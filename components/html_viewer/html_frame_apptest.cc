// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
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
#include "mandoline/tab/test_frame_tree_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "third_party/mojo_services/src/accessibility/public/interfaces/accessibility.mojom.h"

using mandoline::Frame;
using mandoline::FrameConnection;
using mandoline::FrameTree;
using mandoline::FrameTreeClient;

namespace mojo {

namespace {

const char kAddFrameWithEmptyPageScript[] =
    "var iframe = document.createElement(\"iframe\");"
    "iframe.src = \"http://127.0.0.1:%u/files/empty_page.html\";"
    "document.body.appendChild(iframe);";

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

// FrameTreeDelegate that can block waiting for navigation to start.
class TestFrameTreeDelegateImpl : public mandoline::TestFrameTreeDelegate {
 public:
  explicit TestFrameTreeDelegateImpl(mojo::ApplicationImpl* app)
      : app_(app),
        frame_tree_(nullptr),
        waiting_for_navigate_(false),
        got_navigate_(false) {}
  ~TestFrameTreeDelegateImpl() override {}

  void set_frame_tree(FrameTree* frame_tree) { frame_tree_ = frame_tree; }

  void clear_got_navigate() { got_navigate_ = false; }

  bool waiting_for_navigate() const { return waiting_for_navigate_; }

  // Waits for a navigation to occur. This immediately returns true if a
  // navigation has already occurred. In other words, take care when using this,
  // you may need to clear_got_navigate() before calling this.
  bool WaitForNavigateFrame() {
    if (waiting_for_navigate_)
      return false;

    if (got_navigate_)
      return true;

    base::AutoReset<bool> resetter(&waiting_for_navigate_, true);
    return ViewManagerTestBase::DoRunLoopWithTimeout() && got_navigate_;
  }

  // TestFrameTreeDelegate:
  bool CanNavigateFrame(
      Frame* target,
      mojo::URLRequestPtr request,
      mandoline::FrameTreeClient** frame_tree_client,
      scoped_ptr<mandoline::FrameUserData>* frame_user_data,
      mojo::ViewManagerClientPtr* view_manager_client) override {
    scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
    frame_connection->Init(app_, request.Pass(), view_manager_client);
    *frame_tree_client = frame_connection->frame_tree_client();
    *frame_user_data = frame_connection.Pass();

    return true;
  }

  void DidStartNavigation(Frame* frame) override {
    got_navigate_ = true;

    if (waiting_for_navigate_)
      ignore_result(ViewManagerTestBase::QuitRunLoop());
  }

 private:
  mojo::ApplicationImpl* app_;
  FrameTree* frame_tree_;
  bool waiting_for_navigate_;
  bool got_navigate_;

  DISALLOW_COPY_AND_ASSIGN(TestFrameTreeDelegateImpl);
};

}  // namespace

class HTMLFrameTest : public ViewManagerTestBase {
 public:
  HTMLFrameTest() {}
  ~HTMLFrameTest() override {}

 protected:
  // Creates the frame tree showing an empty page at the root and adds (via
  // script) a frame showing the same empty page.
  Frame* LoadEmptyPageAndCreateFrame() {
    View* embed_view = window_manager()->CreateView();
    frame_tree_delegate_.reset(
        new TestFrameTreeDelegateImpl(application_impl()));
    FrameConnection* root_connection =
        InitFrameTree(embed_view, "http://127.0.0.1:%u/files/empty_page2.html");
    const std::string frame_text =
        GetFrameText(root_connection->application_connection());
    if (frame_text != "child2") {
      ADD_FAILURE() << "unexpected text " << frame_text;
      return nullptr;
    }

    return CreateEmptyChildFrame(frame_tree_->root());
  }

  Frame* CreateEmptyChildFrame(Frame* parent) {
    const size_t initial_frame_count = parent->children().size();
    // Dynamically add a new frame.
    ExecuteScript(ApplicationConnectionForFrame(parent),
                  AddPortToString(kAddFrameWithEmptyPageScript));

    // Wait for the frame to appear.
    if ((parent->children().size() != initial_frame_count + 1u ||
         !parent->children().back()->user_data()) &&
        !WaitForNavigateFrame()) {
      ADD_FAILURE() << "timed out waiting for child";
      return nullptr;
    }

    if (parent->view()->children().size() != initial_frame_count + 1u) {
      ADD_FAILURE() << "unexpected number of children "
                    << parent->view()->children().size();
      return nullptr;
    }

    return parent->FindFrame(parent->view()->children().back()->id());
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
    frame_tree_delegate_.reset(
        new TestFrameTreeDelegateImpl(application_impl()));
    scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
    FrameConnection* result = frame_connection.get();
    ViewManagerClientPtr view_manager_client;
    frame_connection->Init(application_impl(), BuildRequestForURL(url_string),
                           &view_manager_client);
    FrameTreeClient* frame_tree_client = frame_connection->frame_tree_client();
    frame_tree_.reset(new FrameTree(view, frame_tree_delegate_.get(),
                                    frame_tree_client,
                                    frame_connection.Pass(),
                                    Frame::ClientPropertyMap()));
    frame_tree_delegate_->set_frame_tree(frame_tree_.get());
    view->Embed(view_manager_client.Pass());
    return result;
  }

  bool WaitForNavigateFrame() {
    if (frame_tree_delegate_->waiting_for_navigate())
      return false;

    frame_tree_delegate_->clear_got_navigate();
    return frame_tree_delegate_->WaitForNavigateFrame();
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

  scoped_ptr<net::SpawnedTestServer> http_server_;
  scoped_ptr<FrameTree> frame_tree_;

  scoped_ptr<TestFrameTreeDelegateImpl> frame_tree_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HTMLFrameTest);
};

TEST_F(HTMLFrameTest, PageWithSingleFrame) {
  View* embed_view = window_manager()->CreateView();

  FrameConnection* root_connection = InitFrameTree(
      embed_view, "http://127.0.0.1:%u/files/page_with_single_frame.html");

  ASSERT_EQ("Page with single frame",
            GetFrameText(root_connection->application_connection()));

  // page_with_single_frame contains a child frame. The child frame should
  // create a new View and Frame.
  if (frame_tree_->root()->children().empty() ||
      !frame_tree_->root()->children().back()->user_data()) {
    ASSERT_TRUE(WaitForNavigateFrame());
  }

  ASSERT_EQ(1u, embed_view->children().size());
  Frame* child_frame =
      frame_tree_->root()->FindFrame(embed_view->children()[0]->id());
  ASSERT_TRUE(child_frame);

  ASSERT_EQ("child",
            GetFrameText(static_cast<FrameConnection*>(child_frame->user_data())
                             ->application_connection()));
}

// Creates two frames. The parent navigates the child frame by way of changing
// the location of the child frame.
TEST_F(HTMLFrameTest, ChangeLocationOfChildFrame) {
  View* embed_view = window_manager()->CreateView();

  InitFrameTree(embed_view,
                "http://127.0.0.1:%u/files/page_with_single_frame.html");

  // page_with_single_frame contains a child frame. The child frame should
  // create a new View and Frame.
  if (frame_tree_->root()->children().empty() ||
      !frame_tree_->root()->children().back()->user_data()) {
    ASSERT_TRUE(WaitForNavigateFrame());
  }

  ASSERT_EQ(
      "child",
      GetFrameText(static_cast<FrameConnection*>(
                       frame_tree_->root()->children().back()->user_data())
                       ->application_connection()));

  // Change the location and wait for the navigation to occur.
  const char kNavigateFrame[] =
      "window.frames[0].location = "
      "'http://127.0.0.1:%u/files/empty_page2.html'";
  frame_tree_delegate_->clear_got_navigate();
  ExecuteScript(ApplicationConnectionForFrame(frame_tree_->root()),
                AddPortToString(kNavigateFrame));
  ASSERT_TRUE(WaitForNavigateFrame());

  // The navigation should have changed the text of the frame.
  ASSERT_EQ(1u, frame_tree_->root()->children().size());
  Frame* child_frame = frame_tree_->root()->children()[0];
  ASSERT_TRUE(child_frame->user_data());
  ASSERT_EQ("child2",
            GetFrameText(static_cast<FrameConnection*>(child_frame->user_data())
                             ->application_connection()));
}

TEST_F(HTMLFrameTest, DynamicallyAddFrameAndVerifyParent) {
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
  Frame* child_frame = LoadEmptyPageAndCreateFrame();
  ASSERT_TRUE(child_frame);

  mojo::ApplicationConnection* child_frame_connection =
      ApplicationConnectionForFrame(child_frame);

  // Change the name of the child's window.
  ExecuteScript(child_frame_connection, "window.name = 'new_child';");

  // Eventually the parent should see the change. There is no convenient way
  // to observe this change, so we repeatedly ask for it and timeout if we
  // never get the right value.
  const base::TimeTicks start_time(base::TimeTicks::Now());
  std::string find_window_result;
  do {
    find_window_result.clear();
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

// Triggers dynamic addition and removal of a frame.
TEST_F(HTMLFrameTest, FrameTreeOfThreeLevels) {
  // Create a child frame, and in that child frame create another child frame.
  Frame* child_frame = LoadEmptyPageAndCreateFrame();
  ASSERT_TRUE(child_frame);

  ASSERT_TRUE(CreateEmptyChildFrame(child_frame));

  // Make sure the parent can see the child and child's child. There is no
  // convenient way to observe this change, so we repeatedly ask for it and
  // timeout if we never get the right value.
  const char kGetChildChildFrameCount[] =
      "if (window.frames.length > 0)"
      "  window.frames[0].frames.length.toString();"
      "else"
      "  '0';";
  const base::TimeTicks start_time(base::TimeTicks::Now());
  std::string child_child_frame_count;
  do {
    child_child_frame_count.clear();
    scoped_ptr<base::Value> script_value(
        ExecuteScript(ApplicationConnectionForFrame(frame_tree_->root()),
                      kGetChildChildFrameCount));
    if (script_value->IsType(base::Value::TYPE_LIST)) {
      base::ListValue* script_value_as_list;
      if (script_value->GetAsList(&script_value_as_list) &&
          script_value_as_list->GetSize() == 1) {
        script_value_as_list->GetString(0u, &child_child_frame_count);
      }
    }
  } while (child_child_frame_count != "1" &&
           base::TimeTicks::Now() - start_time <
               TestTimeouts::action_timeout());
  EXPECT_EQ("1", child_child_frame_count);

  // Remove the child's child and make sure the root doesn't see it anymore.
  const char kRemoveLastIFrame[] =
      "document.body.removeChild(document.body.lastChild);";
  ExecuteScript(ApplicationConnectionForFrame(child_frame), kRemoveLastIFrame);
  do {
    child_child_frame_count.clear();
    scoped_ptr<base::Value> script_value(
        ExecuteScript(ApplicationConnectionForFrame(frame_tree_->root()),
                      kGetChildChildFrameCount));
    if (script_value->IsType(base::Value::TYPE_LIST)) {
      base::ListValue* script_value_as_list;
      if (script_value->GetAsList(&script_value_as_list) &&
          script_value_as_list->GetSize() == 1) {
        script_value_as_list->GetString(0u, &child_child_frame_count);
      }
    }
  } while (child_child_frame_count != "0" &&
           base::TimeTicks::Now() - start_time <
               TestTimeouts::action_timeout());
  ASSERT_EQ("0", child_child_frame_count);
}

// Verifies PostMessage() works across frames.
TEST_F(HTMLFrameTest, PostMessage) {
  Frame* child_frame = LoadEmptyPageAndCreateFrame();
  ASSERT_TRUE(child_frame);

  mojo::ApplicationConnection* child_frame_connection =
      ApplicationConnectionForFrame(child_frame);
  ASSERT_EQ("child", GetFrameText(child_frame_connection));

  // Register an event handler in the child frame.
  const char kRegisterPostMessageHandler[] =
      "window.messageData = null;"
      "function messageFunction(event) {"
      "  window.messageData = event.data;"
      "}"
      "window.addEventListener('message', messageFunction, false);";
  ExecuteScript(child_frame_connection, kRegisterPostMessageHandler);

  frame_tree_delegate_->clear_got_navigate();

  // Post a message from the parent to the child.
  const char kPostMessageFromParent[] =
      "window.frames[0].postMessage('hello from parent', '*');";
  ExecuteScript(ApplicationConnectionForFrame(frame_tree_->root()),
                kPostMessageFromParent);

  // Wait for the child frame to see the message.
  const base::TimeTicks start_time(base::TimeTicks::Now());
  std::string message_in_child;
  do {
    const char kGetMessageData[] = "window.messageData;";
    scoped_ptr<base::Value> script_value(
        ExecuteScript(child_frame_connection, kGetMessageData));
    if (script_value->IsType(base::Value::TYPE_LIST)) {
      base::ListValue* script_value_as_list;
      if (script_value->GetAsList(&script_value_as_list) &&
          script_value_as_list->GetSize() == 1) {
        script_value_as_list->GetString(0u, &message_in_child);
      }
    }
  } while (message_in_child != "hello from parent" &&
           base::TimeTicks::Now() - start_time <
               TestTimeouts::action_timeout());
  EXPECT_EQ("hello from parent", message_in_child);
}

}  // namespace mojo
