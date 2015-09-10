// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/frame.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "components/view_manager/public/cpp/view_tree_connection.h"
#include "components/view_manager/public/cpp/view_tree_delegate.h"
#include "components/view_manager/public/cpp/view_tree_host_factory.h"
#include "components/web_view/frame.h"
#include "components/web_view/frame_connection.h"
#include "components/web_view/frame_tree.h"
#include "components/web_view/frame_tree_delegate.h"
#include "components/web_view/frame_user_data.h"
#include "components/web_view/test_frame_tree_delegate.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"
#include "mojo/application/public/cpp/service_provider_impl.h"

using mojo::View;
using mojo::ViewTreeConnection;

namespace web_view {

namespace {

base::RunLoop* current_run_loop = nullptr;

void TimeoutRunLoop(const base::Closure& timeout_task, bool* timeout) {
  CHECK(current_run_loop);
  *timeout = true;
  timeout_task.Run();
}

bool DoRunLoopWithTimeout() {
  if (current_run_loop != nullptr)
    return false;

  bool timeout = false;
  base::RunLoop run_loop;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&TimeoutRunLoop, run_loop.QuitClosure(), &timeout),
      TestTimeouts::action_timeout());

  current_run_loop = &run_loop;
  current_run_loop->Run();
  current_run_loop = nullptr;
  return !timeout;
}

void QuitRunLoop() {
  current_run_loop->Quit();
  current_run_loop = nullptr;
}

}  // namespace

void OnGotIdCallback(base::RunLoop* run_loop) {
  run_loop->Quit();
}

// Creates a new FrameConnection. This runs a nested message loop until the
// content handler id is obtained.
scoped_ptr<FrameConnection> CreateFrameConnection(mojo::ApplicationImpl* app) {
  scoped_ptr<FrameConnection> frame_connection(new FrameConnection);
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(app->url());
  base::RunLoop run_loop;
  frame_connection->Init(app, request.Pass(),
                         base::Bind(&OnGotIdCallback, &run_loop));
  run_loop.Run();
  return frame_connection;
}

class TestFrameTreeClient : public FrameTreeClient {
 public:
  TestFrameTreeClient() : connect_count_(0) {}
  ~TestFrameTreeClient() override {}

  int connect_count() const { return connect_count_; }

  mojo::Array<FrameDataPtr> connect_frames() { return connect_frames_.Pass(); }

  mojo::Array<FrameDataPtr> adds() { return adds_.Pass(); }

  // Sets a callback to run once OnConnect() is received.
  void set_on_connect_callback(const base::Closure& closure) {
    on_connect_callback_ = closure;
  }

  FrameTreeServer* server() { return server_.get(); }

  // TestFrameTreeClient:
  void OnConnect(FrameTreeServerPtr server,
                 uint32_t change_id,
                 uint32_t view_id,
                 ViewConnectType view_connect_type,
                 mojo::Array<FrameDataPtr> frames,
                 const OnConnectCallback& callback) override {
    connect_count_++;
    connect_frames_ = frames.Pass();
    server_ = server.Pass();
    callback.Run();
    if (!on_connect_callback_.is_null())
      on_connect_callback_.Run();
  }
  void OnFrameAdded(uint32_t change_id, FrameDataPtr frame) override {
    adds_.push_back(frame.Pass());
  }
  void OnFrameRemoved(uint32_t change_id, uint32_t frame_id) override {}
  void OnFrameClientPropertyChanged(uint32_t frame_id,
                                    const mojo::String& name,
                                    mojo::Array<uint8_t> new_data) override {}
  void OnPostMessageEvent(uint32_t source_frame_id,
                          uint32_t target_frame_id,
                          HTMLMessageEventPtr event) override {}
  void OnWillNavigate(uint32_t frame_id) override {}

 private:
  int connect_count_;
  mojo::Array<FrameDataPtr> connect_frames_;
  FrameTreeServerPtr server_;
  mojo::Array<FrameDataPtr> adds_;
  base::Closure on_connect_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestFrameTreeClient);
};

class FrameTest;

// ViewAndFrame maintains the View and TestFrameTreeClient associated with
// a single FrameTreeClient. In other words this maintains the data structures
// needed to represent a client side frame. To obtain one use
// FrameTest::WaitForViewAndFrame().
class ViewAndFrame : public mojo::ViewTreeDelegate {
 public:
  ~ViewAndFrame() override {
    if (view_)
      delete view_->connection();
  }

  // The View associated with the frame.
  mojo::View* view() { return view_; }
  TestFrameTreeClient* test_frame_tree_client() {
    return &test_frame_tree_client_;
  }
  FrameTreeServer* frame_tree_server() {
    return test_frame_tree_client_.server();
  }

 private:
  friend class FrameTest;

  ViewAndFrame()
      : view_(nullptr), frame_tree_binding_(&test_frame_tree_client_) {}

  // Runs a message loop until the view and frame data have been received.
  void WaitForViewAndFrame() { run_loop_.Run(); }

  void Bind(mojo::InterfaceRequest<FrameTreeClient> request) {
    ASSERT_FALSE(frame_tree_binding_.is_bound());
    test_frame_tree_client_.set_on_connect_callback(
        base::Bind(&ViewAndFrame::OnGotConnect, base::Unretained(this)));
    frame_tree_binding_.Bind(request.Pass());
  }

  void OnGotConnect() { QuitRunLoopIfNecessary(); }

  void QuitRunLoopIfNecessary() {
    if (view_ && test_frame_tree_client_.connect_count())
      run_loop_.Quit();
  }

  // Overridden from ViewTreeDelegate:
  void OnEmbed(View* root) override {
    view_ = root;
    QuitRunLoopIfNecessary();
  }
  void OnConnectionLost(ViewTreeConnection* connection) override {
    view_ = nullptr;
  }

  mojo::View* view_;
  base::RunLoop run_loop_;
  TestFrameTreeClient test_frame_tree_client_;
  mojo::Binding<FrameTreeClient> frame_tree_binding_;

  DISALLOW_COPY_AND_ASSIGN(ViewAndFrame);
};

class FrameTest : public mojo::test::ApplicationTestBase,
                  public mojo::ApplicationDelegate,
                  public mojo::ViewTreeDelegate,
                  public mojo::InterfaceFactory<mojo::ViewTreeClient>,
                  public mojo::InterfaceFactory<FrameTreeClient> {
 public:
  FrameTest() : most_recent_connection_(nullptr), window_manager_(nullptr) {}

  ViewTreeConnection* most_recent_connection() {
    return most_recent_connection_;
  }

  // Runs a message loop until the data necessary to represent to a client side
  // frame has been obtained.
  scoped_ptr<ViewAndFrame> WaitForViewAndFrame() {
    DCHECK(!view_and_frame_);
    view_and_frame_.reset(new ViewAndFrame);
    view_and_frame_->WaitForViewAndFrame();
    return view_and_frame_.Pass();
  }

  // ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService<mojo::ViewTreeClient>(this);
    connection->AddService<FrameTreeClient>(this);
    return true;
  }

  ViewTreeConnection* window_manager() { return window_manager_; }

  // ApplicationTestBase:
  ApplicationDelegate* GetApplicationDelegate() override { return this; }

  // Overridden from ViewTreeDelegate:
  void OnEmbed(View* root) override {
    most_recent_connection_ = root->connection();
    QuitRunLoop();
  }
  void OnConnectionLost(ViewTreeConnection* connection) override {}

 private:
  // Overridden from testing::Test:
  void SetUp() override {
    ApplicationTestBase::SetUp();

    mojo::CreateSingleViewTreeHost(application_impl(), this, &host_);

    ASSERT_TRUE(DoRunLoopWithTimeout());
    std::swap(window_manager_, most_recent_connection_);
  }

  // Overridden from testing::Test:
  void TearDown() override {
    ApplicationTestBase::TearDown();
  }

  // Overridden from mojo::InterfaceFactory<mojo::ViewTreeClient>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mojo::ViewTreeClient> request) override {
    if (view_and_frame_) {
      mojo::ViewTreeConnection::Create(view_and_frame_.get(), request.Pass());
    } else {
      mojo::ViewTreeConnection::Create(this, request.Pass());
    }
  }

  // Overridden from mojo::InterfaceFactory<FrameTreeClient>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<FrameTreeClient> request) override {
    ASSERT_TRUE(view_and_frame_);
    view_and_frame_->Bind(request.Pass());
  }

  mojo::ViewTreeHostPtr host_;

  // Used to receive the most recent view manager loaded by an embed action.
  ViewTreeConnection* most_recent_connection_;
  // The View Manager connection held by the window manager (app running at the
  // root view).
  ViewTreeConnection* window_manager_;

  scoped_ptr<ViewAndFrame> view_and_frame_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(FrameTest);
};

// Verifies the root gets a connect.
TEST_F(FrameTest, RootGetsConnect) {
  TestFrameTreeDelegate tree_delegate(application_impl());
  TestFrameTreeClient root_client;
  FrameTree tree(0u, window_manager()->GetRoot(), nullptr, &tree_delegate,
                 &root_client, nullptr, Frame::ClientPropertyMap());
  ASSERT_EQ(1, root_client.connect_count());
  mojo::Array<FrameDataPtr> frames = root_client.connect_frames();
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(tree.root()->view()->id(), frames[0]->frame_id);
  EXPECT_EQ(0u, frames[0]->parent_id);
}

// Verifies adding a child to the root.
TEST_F(FrameTest, SingleChild) {
  TestFrameTreeDelegate tree_delegate(application_impl());
  TestFrameTreeClient root_client;
  FrameTree tree(0u, window_manager()->GetRoot(), nullptr, &tree_delegate,
                 &root_client, nullptr, Frame::ClientPropertyMap());

  View* child = window_manager()->CreateView();
  EXPECT_EQ(nullptr, Frame::FindFirstFrameAncestor(child));
  window_manager()->GetRoot()->AddChild(child);
  EXPECT_EQ(tree.root(), Frame::FindFirstFrameAncestor(child));

  TestFrameTreeClient child_client;
  Frame* child_frame =
      tree.CreateAndAddFrame(child, tree.root(), 0u, &child_client, nullptr);
  EXPECT_EQ(tree.root(), child_frame->parent());

  ASSERT_EQ(1, child_client.connect_count());
  mojo::Array<FrameDataPtr> frames_in_child = child_client.connect_frames();
  // We expect 2 frames. One for the root, one for the child.
  ASSERT_EQ(2u, frames_in_child.size());
  EXPECT_EQ(tree.root()->view()->id(), frames_in_child[0]->frame_id);
  EXPECT_EQ(0u, frames_in_child[0]->parent_id);
  EXPECT_EQ(child_frame->view()->id(), frames_in_child[1]->frame_id);
  EXPECT_EQ(tree.root()->view()->id(), frames_in_child[1]->parent_id);

  // We should have gotten notification of the add.
  EXPECT_EQ(1u, root_client.adds().size());
}

TEST_F(FrameTest, DisconnectingViewDestroysFrame) {
  TestFrameTreeDelegate tree_delegate(application_impl());
  scoped_ptr<FrameConnection> frame_connection =
      CreateFrameConnection(application_impl());
  FrameTreeClient* frame_tree_client = frame_connection->frame_tree_client();
  mojo::ViewTreeClientPtr view_tree_client =
      frame_connection->GetViewTreeClient();
  mojo::View* frame_root_view = window_manager()->CreateView();
  window_manager()->GetRoot()->AddChild(frame_root_view);
  FrameTree tree(0u, frame_root_view, view_tree_client.Pass(), &tree_delegate,
                 frame_tree_client, frame_connection.Pass(),
                 Frame::ClientPropertyMap());
  scoped_ptr<ViewAndFrame> vf1(WaitForViewAndFrame());

  // Create a new child frame.
  mojo::View* child_frame_view = vf1->view()->connection()->CreateView();
  vf1->view()->AddChild(child_frame_view);
  mojo::Map<mojo::String, mojo::Array<uint8_t>> client_properties;
  client_properties.mark_non_null();
  vf1->frame_tree_server()->OnCreatedFrame(child_frame_view->parent()->id(),
                                           child_frame_view->id(),
                                           client_properties.Pass());
  ASSERT_NO_FATAL_FAILURE(tree_delegate.WaitForCreateFrame());

  // Navigate the child frame, which should trigger a new ViewAndFrame.
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(application_impl()->url());
  vf1->frame_tree_server()->RequestNavigate(
      NAVIGATION_TARGET_TYPE_EXISTING_FRAME, child_frame_view->id(),
      request.Pass());
  scoped_ptr<ViewAndFrame> vf2(WaitForViewAndFrame());

  // Delete the ViewTreeConnection for vf2, which should trigger destroying
  // the frame. Deleting the ViewTreeConnection triggers
  // Frame::OnViewEmbeddedAppDisconnected.
  delete vf2->view()->connection();
  ASSERT_EQ(1u, tree.root()->children().size());
  ASSERT_NO_FATAL_FAILURE(
      tree_delegate.WaitForDestroyFrame(tree.root()->children()[0]));
  ASSERT_EQ(0u, tree.root()->children().size());
}

}  // namespace web_view
