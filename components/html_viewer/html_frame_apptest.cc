// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/html_viewer/public/interfaces/test_html_viewer.mojom.h"
#include "components/mus/public/cpp/tests/window_server_test_base.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/web_view/frame.h"
#include "components/web_view/frame_connection.h"
#include "components/web_view/frame_tree.h"
#include "components/web_view/public/interfaces/frame.mojom.h"
#include "components/web_view/test_frame_tree_delegate.h"
#include "mojo/services/accessibility/public/interfaces/accessibility.mojom.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using mus::mojom::WindowTreeClientPtr;
using mus::WindowServerTestBase;
using web_view::Frame;
using web_view::FrameConnection;
using web_view::FrameTree;
using web_view::FrameTreeDelegate;
using web_view::mojom::FrameClient;

namespace mojo {

namespace {

const char kAddFrameWithEmptyPageScript[] =
    "var iframe = document.createElement(\"iframe\");"
    "iframe.src = \"http://127.0.0.1:%u/empty_page.html\";"
    "document.body.appendChild(iframe);";

void OnGotContentHandlerForRoot(bool* got_callback) {
  *got_callback = true;
  ignore_result(WindowServerTestBase::QuitRunLoop());
}

mojo::Connection* ConnectionForFrame(Frame* frame) {
  return static_cast<FrameConnection*>(frame->user_data())->connection();
}

std::string GetFrameText(Connection* connection) {
  html_viewer::TestHTMLViewerPtr test_html_viewer;
  connection->GetInterface(&test_html_viewer);
  std::string result;
  test_html_viewer->GetContentAsText([&result](const String& mojo_string) {
    result = mojo_string;
    ASSERT_TRUE(WindowServerTestBase::QuitRunLoop());
  });
  if (!WindowServerTestBase::DoRunLoopWithTimeout())
    ADD_FAILURE() << "Timed out waiting for execute to complete";
  //  test_html_viewer.WaitForIncomingResponse();
  return result;
}

scoped_ptr<base::Value> ExecuteScript(Connection* connection,
                                      const std::string& script) {
  html_viewer::TestHTMLViewerPtr test_html_viewer;
  connection->GetInterface(&test_html_viewer);
  scoped_ptr<base::Value> result;
  test_html_viewer->ExecuteScript(script, [&result](const String& json_string) {
    result = base::JSONReader::Read(json_string.To<std::string>());
    ASSERT_TRUE(WindowServerTestBase::QuitRunLoop());
  });
  if (!WindowServerTestBase::DoRunLoopWithTimeout())
    ADD_FAILURE() << "Timed out waiting for execute to complete";
  return result;
}

// FrameTreeDelegate that can block waiting for navigation to start.
class TestFrameTreeDelegateImpl : public web_view::TestFrameTreeDelegate {
 public:
  explicit TestFrameTreeDelegateImpl(mojo::Shell* shell)
      : TestFrameTreeDelegate(shell), frame_tree_(nullptr) {}
  ~TestFrameTreeDelegateImpl() override {}

  void set_frame_tree(FrameTree* frame_tree) { frame_tree_ = frame_tree; }

  // Resets the navigation state for |frame|.
  void ClearGotNavigate(Frame* frame) { frames_navigated_.erase(frame); }

  // Waits until |frame| has |count| children and the last child has navigated.
  bool WaitForChildFrameCount(Frame* frame, size_t count) {
    if (DidChildNavigate(frame, count))
      return true;

    waiting_for_frame_child_count_.reset(new FrameAndChildCount);
    waiting_for_frame_child_count_->frame = frame;
    waiting_for_frame_child_count_->count = count;

    return WindowServerTestBase::DoRunLoopWithTimeout();
  }

  // Returns true if |frame| has navigated. If |frame| hasn't navigated runs
  // a nested message loop until |frame| navigates.
  bool WaitForFrameNavigation(Frame* frame) {
    if (frames_navigated_.count(frame))
      return true;

    frames_waiting_for_navigate_.insert(frame);
    return WindowServerTestBase::DoRunLoopWithTimeout();
  }

  // TestFrameTreeDelegate:
  void DidStartNavigation(Frame* frame) override {
    frames_navigated_.insert(frame);

    if (waiting_for_frame_child_count_ &&
        DidChildNavigate(waiting_for_frame_child_count_->frame,
                         waiting_for_frame_child_count_->count)) {
      waiting_for_frame_child_count_.reset();
      ASSERT_TRUE(WindowServerTestBase::QuitRunLoop());
    }

    if (frames_waiting_for_navigate_.count(frame)) {
      frames_waiting_for_navigate_.erase(frame);
      ignore_result(WindowServerTestBase::QuitRunLoop());
    }
  }

 private:
  struct FrameAndChildCount {
    Frame* frame;
    size_t count;
  };

  // Returns true if |frame| has |count| children and the last child frame
  // has navigated.
  bool DidChildNavigate(Frame* frame, size_t count) {
    return ((frame->children().size() == count) &&
            (frames_navigated_.count(frame->children()[count - 1])));
  }

  FrameTree* frame_tree_;
  // Any time DidStartNavigation() is invoked the frame is added here. Frames
  // are inserted as void* as this does not track destruction of the frames.
  std::set<void*> frames_navigated_;

  // The set of frames waiting for a navigation to occur.
  std::set<Frame*> frames_waiting_for_navigate_;

  // Set of frames waiting for a certain number of children and navigation.
  scoped_ptr<FrameAndChildCount> waiting_for_frame_child_count_;

  DISALLOW_COPY_AND_ASSIGN(TestFrameTreeDelegateImpl);
};

}  // namespace

class HTMLFrameTest : public WindowServerTestBase {
 public:
  HTMLFrameTest() {}
  ~HTMLFrameTest() override {}

 protected:
  // Creates the frame tree showing an empty page at the root and adds (via
  // script) a frame showing the same empty page.
  Frame* LoadEmptyPageAndCreateFrame() {
    mus::Window* embed_window = window_manager()->NewWindow();
    frame_tree_delegate_.reset(new TestFrameTreeDelegateImpl(shell()));
    FrameConnection* root_connection = InitFrameTree(
        embed_window, "http://127.0.0.1:%u/empty_page2.html");
    if (!root_connection) {
      ADD_FAILURE() << "unable to establish root connection";
      return nullptr;
    }
    const std::string frame_text = GetFrameText(root_connection->connection());
    if (frame_text != "child2") {
      ADD_FAILURE() << "unexpected text " << frame_text;
      return nullptr;
    }

    return CreateEmptyChildFrame(frame_tree_->root());
  }

  Frame* CreateEmptyChildFrame(Frame* parent) {
    const size_t initial_frame_count = parent->children().size();
    // Dynamically add a new frame.
    ExecuteScript(ConnectionForFrame(parent),
                  AddPortToString(kAddFrameWithEmptyPageScript));

    frame_tree_delegate_->WaitForChildFrameCount(parent,
                                                 initial_frame_count + 1);
    if (HasFatalFailure())
      return nullptr;

    return parent->FindFrame(parent->window()->children().back()->id());
  }

  std::string AddPortToString(const std::string& string) {
    const uint16_t assigned_port = http_server_->host_port_pair().port();
    return base::StringPrintf(string.c_str(), assigned_port);
  }

  mojo::URLRequestPtr BuildRequestForURL(const std::string& url_string) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From(AddPortToString(url_string));
    return request;
  }

  FrameConnection* InitFrameTree(mus::Window* view,
                                 const std::string& url_string) {
    frame_tree_delegate_.reset(new TestFrameTreeDelegateImpl(shell()));
    scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
    bool got_callback = false;
    frame_connection->Init(
        shell(), BuildRequestForURL(url_string),
        base::Bind(&OnGotContentHandlerForRoot, &got_callback));
    ignore_result(WindowServerTestBase::DoRunLoopWithTimeout());
    if (!got_callback)
      return nullptr;
    FrameConnection* result = frame_connection.get();
    FrameClient* frame_client = frame_connection->frame_client();
    WindowTreeClientPtr tree_client = frame_connection->GetWindowTreeClient();
    frame_tree_.reset(new FrameTree(
        result->GetContentHandlerID(), view, std::move(tree_client),
        frame_tree_delegate_.get(), frame_client, std::move(frame_connection),
        Frame::ClientPropertyMap(), base::TimeTicks::Now()));
    frame_tree_delegate_->set_frame_tree(frame_tree_.get());
    return result;
  }

  // ViewManagerTest:
  void SetUp() override {
    WindowServerTestBase::SetUp();

    // Start a test server.
    http_server_.reset(new net::EmbeddedTestServer);
    http_server_->ServeFilesFromSourceDirectory(
        "components/test/data/html_viewer");
    ASSERT_TRUE(http_server_->Start());
  }
  void TearDown() override {
    frame_tree_.reset();
    http_server_.reset();
    WindowServerTestBase::TearDown();
  }

  scoped_ptr<net::EmbeddedTestServer> http_server_;
  scoped_ptr<FrameTree> frame_tree_;

  scoped_ptr<TestFrameTreeDelegateImpl> frame_tree_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HTMLFrameTest);
};

// Crashes on linux_chromium_rel_ng only. http://crbug.com/567337
#if defined(OS_LINUX)
#define MAYBE_PageWithSingleFrame DISABLED_PageWithSingleFrame
#else
#define MAYBE_PageWithSingleFrame PageWithSingleFrame
#endif
TEST_F(HTMLFrameTest, MAYBE_PageWithSingleFrame) {
  mus::Window* embed_window = window_manager()->NewWindow();

  FrameConnection* root_connection = InitFrameTree(
      embed_window, "http://127.0.0.1:%u/page_with_single_frame.html");
  ASSERT_TRUE(root_connection);

  ASSERT_EQ("Page with single frame",
            GetFrameText(root_connection->connection()));

  ASSERT_NO_FATAL_FAILURE(
      frame_tree_delegate_->WaitForChildFrameCount(frame_tree_->root(), 1u));

  ASSERT_EQ(1u, embed_window->children().size());
  Frame* child_frame =
      frame_tree_->root()->FindFrame(embed_window->children()[0]->id());
  ASSERT_TRUE(child_frame);

  ASSERT_EQ("child",
            GetFrameText(static_cast<FrameConnection*>(child_frame->user_data())
                             ->connection()));
}

// Creates two frames. The parent navigates the child frame by way of changing
// the location of the child frame.
// Crashes on linux_chromium_rel_ng only. http://crbug.com/567337
#if defined(OS_LINUX)
#define MAYBE_ChangeLocationOfChildFrame DISABLED_ChangeLocationOfChildFrame
#else
#define MAYBE_ChangeLocationOfChildFrame ChangeLocationOfChildFrame
#endif
TEST_F(HTMLFrameTest, MAYBE_ChangeLocationOfChildFrame) {
  mus::Window* embed_window = window_manager()->NewWindow();

  ASSERT_TRUE(InitFrameTree(
      embed_window, "http://127.0.0.1:%u/page_with_single_frame.html"));

  // page_with_single_frame contains a child frame. The child frame should
  // create a new View and Frame.
  ASSERT_NO_FATAL_FAILURE(
      frame_tree_delegate_->WaitForChildFrameCount(frame_tree_->root(), 1u));

  Frame* child_frame = frame_tree_->root()->children().back();

  ASSERT_EQ("child",
            GetFrameText(static_cast<FrameConnection*>(child_frame->user_data())
                             ->connection()));

  // Change the location and wait for the navigation to occur.
  const char kNavigateFrame[] =
      "window.frames[0].location = "
      "'http://127.0.0.1:%u/empty_page2.html'";
  frame_tree_delegate_->ClearGotNavigate(child_frame);
  ExecuteScript(ConnectionForFrame(frame_tree_->root()),
                AddPortToString(kNavigateFrame));
  ASSERT_TRUE(frame_tree_delegate_->WaitForFrameNavigation(child_frame));

  // There should still only be one frame.
  ASSERT_EQ(1u, frame_tree_->root()->children().size());

  // The navigation should have changed the text of the frame.
  ASSERT_TRUE(child_frame->user_data());
  ASSERT_EQ("child2",
            GetFrameText(static_cast<FrameConnection*>(child_frame->user_data())
                             ->connection()));
}

TEST_F(HTMLFrameTest, DynamicallyAddFrameAndVerifyParent) {
  Frame* child_frame = LoadEmptyPageAndCreateFrame();
  ASSERT_TRUE(child_frame);

  mojo::Connection* child_frame_connection = ConnectionForFrame(child_frame);

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

  mojo::Connection* child_frame_connection = ConnectionForFrame(child_frame);

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
        ExecuteScript(ConnectionForFrame(frame_tree_->root()),
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
        ExecuteScript(ConnectionForFrame(frame_tree_->root()),
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
  ExecuteScript(ConnectionForFrame(child_frame), kRemoveLastIFrame);
  do {
    child_child_frame_count.clear();
    scoped_ptr<base::Value> script_value(
        ExecuteScript(ConnectionForFrame(frame_tree_->root()),
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

  mojo::Connection* child_frame_connection = ConnectionForFrame(child_frame);
  ASSERT_EQ("child", GetFrameText(child_frame_connection));

  // Register an event handler in the child frame.
  const char kRegisterPostMessageHandler[] =
      "window.messageData = null;"
      "function messageFunction(event) {"
      "  window.messageData = event.data;"
      "}"
      "window.addEventListener('message', messageFunction, false);";
  ExecuteScript(child_frame_connection, kRegisterPostMessageHandler);

  // Post a message from the parent to the child.
  const char kPostMessageFromParent[] =
      "window.frames[0].postMessage('hello from parent', '*');";
  ExecuteScript(ConnectionForFrame(frame_tree_->root()),
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
