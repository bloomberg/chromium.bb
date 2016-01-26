// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/frame.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/cpp/window_tree_host_factory.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/web_view/frame_connection.h"
#include "components/web_view/frame_tree.h"
#include "components/web_view/frame_tree_delegate.h"
#include "components/web_view/frame_user_data.h"
#include "components/web_view/test_frame_tree_delegate.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "mojo/shell/public/cpp/service_provider_impl.h"

using mus::Window;
using mus::WindowTreeConnection;

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
  frame_connection->Init(app, std::move(request),
                         base::Bind(&OnGotIdCallback, &run_loop));
  run_loop.Run();
  return frame_connection;
}

class TestFrameClient : public mojom::FrameClient {
 public:
  TestFrameClient()
      : connect_count_(0), last_dispatch_load_event_frame_id_(0) {}
  ~TestFrameClient() override {}

  int connect_count() const { return connect_count_; }

  mojo::Array<mojom::FrameDataPtr> connect_frames() {
    return std::move(connect_frames_);
  }

  mojo::Array<mojom::FrameDataPtr> adds() { return std::move(adds_); }

  // Sets a callback to run once OnConnect() is received.
  void set_on_connect_callback(const base::Closure& closure) {
    on_connect_callback_ = closure;
  }

  void set_on_loading_state_changed_callback(const base::Closure& closure) {
    on_loading_state_changed_callback_ = closure;
  }

  void set_on_dispatch_load_event_callback(const base::Closure& closure) {
    on_dispatch_load_event_callback_ = closure;
  }

  mojom::Frame* server_frame() { return server_frame_.get(); }

  mojo::InterfaceRequest<mojom::Frame> GetServerFrameRequest() {
    return GetProxy(&server_frame_);
  }

  void last_loading_state_changed_notification(uint32_t* frame_id,
                                               bool* loading) const {
    *frame_id = last_loading_state_changed_notification_.frame_id;
    *loading = last_loading_state_changed_notification_.loading;
  }

  uint32_t last_dispatch_load_event_frame_id() const {
    return last_dispatch_load_event_frame_id_;
  }

  base::TimeTicks last_navigation_start_time() const {
    return last_navigation_start_time_;
  }

  // mojom::FrameClient:
  void OnConnect(mojom::FramePtr frame,
                 uint32_t change_id,
                 uint32_t window_id,
                 mojom::WindowConnectType window_connect_type,
                 mojo::Array<mojom::FrameDataPtr> frames,
                 int64_t navigation_start_time_ticks,
                 const OnConnectCallback& callback) override {
    connect_count_++;
    connect_frames_ = std::move(frames);
    if (frame)
      server_frame_ = std::move(frame);
    callback.Run();
    if (!on_connect_callback_.is_null())
      on_connect_callback_.Run();

    last_navigation_start_time_ =
        base::TimeTicks::FromInternalValue(navigation_start_time_ticks);
  }
  void OnFrameAdded(uint32_t change_id, mojom::FrameDataPtr frame) override {
    adds_.push_back(std::move(frame));
  }
  void OnFrameRemoved(uint32_t change_id, uint32_t frame_id) override {}
  void OnFrameClientPropertyChanged(uint32_t frame_id,
                                    const mojo::String& name,
                                    mojo::Array<uint8_t> new_data) override {}
  void OnPostMessageEvent(uint32_t source_frame_id,
                          uint32_t target_frame_id,
                          mojom::HTMLMessageEventPtr event) override {}
  void OnWillNavigate(const mojo::String& origin,
                      const OnWillNavigateCallback& callback) override {
    callback.Run();
  }
  void OnFrameLoadingStateChanged(uint32_t frame_id, bool loading) override {
    last_loading_state_changed_notification_.frame_id = frame_id;
    last_loading_state_changed_notification_.loading = loading;

    if (!on_loading_state_changed_callback_.is_null())
      on_loading_state_changed_callback_.Run();
  }
  void OnDispatchFrameLoadEvent(uint32_t frame_id) override {
    last_dispatch_load_event_frame_id_ = frame_id;

    if (!on_dispatch_load_event_callback_.is_null())
      on_dispatch_load_event_callback_.Run();
  }
  void Find(int32_t request_id,
            const mojo::String& search_text,
            mojom::FindOptionsPtr options,
            bool wrap_within_frame,
            const FindCallback& callback) override {}
  void StopFinding(bool clear_selection) override {}
  void HighlightFindResults(int32_t request_id,
                            const mojo::String& search_test,
                            mojom::FindOptionsPtr options,
                            bool reset) override {}
  void StopHighlightingFindResults() override {}

 private:
  struct LoadingStateChangedNotification {
    LoadingStateChangedNotification() : frame_id(0), loading(false) {}
    ~LoadingStateChangedNotification() {}

    uint32_t frame_id;
    bool loading;
  };

  int connect_count_;
  mojo::Array<mojom::FrameDataPtr> connect_frames_;
  mojom::FramePtr server_frame_;
  mojo::Array<mojom::FrameDataPtr> adds_;
  base::Closure on_connect_callback_;
  base::Closure on_loading_state_changed_callback_;
  base::Closure on_dispatch_load_event_callback_;
  LoadingStateChangedNotification last_loading_state_changed_notification_;
  uint32_t last_dispatch_load_event_frame_id_;
  base::TimeTicks last_navigation_start_time_;

  DISALLOW_COPY_AND_ASSIGN(TestFrameClient);
};

class FrameTest;

// WindowAndFrame maintains the Window and TestFrameClient associated with
// a single FrameClient. In other words this maintains the data structures
// needed to represent a client side frame. To obtain one use
// FrameTest::WaitForViewAndFrame().
class WindowAndFrame : public mus::WindowTreeDelegate {
 public:
  ~WindowAndFrame() override {
    if (window_)
      delete window_->connection();
  }

  // The Window associated with the frame.
  mus::Window* window() { return window_; }
  TestFrameClient* test_frame_client() { return &test_frame_tree_client_; }
  mojom::Frame* server_frame() {
    return test_frame_tree_client_.server_frame();
  }

 private:
  friend class FrameTest;

  WindowAndFrame()
      : window_(nullptr), frame_client_binding_(&test_frame_tree_client_) {}

  void set_window(Window* window) { window_ = window; }

  // Runs a message loop until the window and frame data have been received.
  void WaitForViewAndFrame() { run_loop_.Run(); }

  mojo::InterfaceRequest<mojom::Frame> GetServerFrameRequest() {
    return test_frame_tree_client_.GetServerFrameRequest();
  }

  mojom::FrameClientPtr GetFrameClientPtr() {
    return frame_client_binding_.CreateInterfacePtrAndBind();
  }

  void Bind(mojo::InterfaceRequest<mojom::FrameClient> request) {
    ASSERT_FALSE(frame_client_binding_.is_bound());
    test_frame_tree_client_.set_on_connect_callback(
        base::Bind(&WindowAndFrame::OnGotConnect, base::Unretained(this)));
    frame_client_binding_.Bind(std::move(request));
  }

  void OnGotConnect() { QuitRunLoopIfNecessary(); }

  void QuitRunLoopIfNecessary() {
    if (window_ && test_frame_tree_client_.connect_count())
      run_loop_.Quit();
  }

  // Overridden from WindowTreeDelegate:
  void OnEmbed(Window* root) override {
    window_ = root;
    QuitRunLoopIfNecessary();
  }
  void OnConnectionLost(WindowTreeConnection* connection) override {
    window_ = nullptr;
  }

  mus::Window* window_;
  base::RunLoop run_loop_;
  TestFrameClient test_frame_tree_client_;
  mojo::Binding<mojom::FrameClient> frame_client_binding_;

  DISALLOW_COPY_AND_ASSIGN(WindowAndFrame);
};

class FrameTest : public mojo::test::ApplicationTestBase,
                  public mojo::ApplicationDelegate,
                  public mus::WindowTreeDelegate,
                  public mojo::InterfaceFactory<mus::mojom::WindowTreeClient>,
                  public mojo::InterfaceFactory<mojom::FrameClient> {
 public:
  FrameTest() : most_recent_connection_(nullptr), window_manager_(nullptr) {}

  WindowTreeConnection* most_recent_connection() {
    return most_recent_connection_;
  }

 protected:
  WindowTreeConnection* window_manager() { return window_manager_; }
  TestFrameTreeDelegate* frame_tree_delegate() {
    return frame_tree_delegate_.get();
  }
  FrameTree* frame_tree() { return frame_tree_.get(); }
  WindowAndFrame* root_window_and_frame() {
    return root_window_and_frame_.get();
  }

  scoped_ptr<WindowAndFrame> NavigateFrameWithStartTime(
      WindowAndFrame* window_and_frame,
      base::TimeTicks navigation_start_time) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From(application_impl()->url());
    request->originating_time_ticks = navigation_start_time.ToInternalValue();
    window_and_frame->server_frame()->RequestNavigate(
        mojom::NavigationTargetType::EXISTING_FRAME,
        window_and_frame->window()->id(), std::move(request));
    return WaitForViewAndFrame();
  }

  scoped_ptr<WindowAndFrame> NavigateFrame(WindowAndFrame* window_and_frame) {
    return NavigateFrameWithStartTime(window_and_frame, base::TimeTicks());
  }

  // Creates a new shared frame as a child of |parent|.
  scoped_ptr<WindowAndFrame> CreateChildWindowAndFrame(WindowAndFrame* parent) {
    mus::Window* child_frame_window =
        parent->window()->connection()->NewWindow();
    parent->window()->AddChild(child_frame_window);

    scoped_ptr<WindowAndFrame> window_and_frame(new WindowAndFrame);
    window_and_frame->set_window(child_frame_window);

    mojo::Map<mojo::String, mojo::Array<uint8_t>> client_properties;
    client_properties.mark_non_null();
    parent->server_frame()->OnCreatedFrame(
        window_and_frame->GetServerFrameRequest(),
        window_and_frame->GetFrameClientPtr(), child_frame_window->id(),
        std::move(client_properties));
    frame_tree_delegate()->WaitForCreateFrame();
    return HasFatalFailure() ? nullptr : std::move(window_and_frame);
  }

  // Runs a message loop until the data necessary to represent to a client side
  // frame has been obtained.
  scoped_ptr<WindowAndFrame> WaitForViewAndFrame() {
    DCHECK(!window_and_frame_);
    window_and_frame_.reset(new WindowAndFrame);
    window_and_frame_->WaitForViewAndFrame();
    return std::move(window_and_frame_);
  }

 private:
  // ApplicationTestBase:
  ApplicationDelegate* GetApplicationDelegate() override { return this; }

  // ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService<mus::mojom::WindowTreeClient>(this);
    connection->AddService<mojom::FrameClient>(this);
    return true;
  }

  // Overridden from WindowTreeDelegate:
  void OnEmbed(Window* root) override {
    most_recent_connection_ = root->connection();
    QuitRunLoop();
  }
  void OnConnectionLost(WindowTreeConnection* connection) override {}

  // Overridden from testing::Test:
  void SetUp() override {
    ApplicationTestBase::SetUp();

    mus::CreateSingleWindowTreeHost(application_impl(),
                                    mus::mojom::WindowTreeHostClientPtr(), this,
                                    &host_, nullptr, nullptr);

    ASSERT_TRUE(DoRunLoopWithTimeout());
    std::swap(window_manager_, most_recent_connection_);

    // Creates a FrameTree, which creates a single frame. Wait for the
    // FrameClient to be connected to.
    frame_tree_delegate_.reset(new TestFrameTreeDelegate(application_impl()));
    scoped_ptr<FrameConnection> frame_connection =
        CreateFrameConnection(application_impl());
    mojom::FrameClient* frame_client = frame_connection->frame_client();
    mus::mojom::WindowTreeClientPtr window_tree_client =
        frame_connection->GetWindowTreeClient();
    mus::Window* frame_root_view = window_manager()->NewWindow();
    (*window_manager()->GetRoots().begin())->AddChild(frame_root_view);
    frame_tree_.reset(new FrameTree(
        0u, frame_root_view, std::move(window_tree_client),
        frame_tree_delegate_.get(), frame_client, std::move(frame_connection),
        Frame::ClientPropertyMap(), base::TimeTicks::Now()));
    root_window_and_frame_ = WaitForViewAndFrame();
  }

  // Overridden from testing::Test:
  void TearDown() override {
    root_window_and_frame_.reset();
    frame_tree_.reset();
    frame_tree_delegate_.reset();
    ApplicationTestBase::TearDown();
  }

  // Overridden from mojo::InterfaceFactory<mus::mojom::WindowTreeClient>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request) override {
    if (window_and_frame_) {
      mus::WindowTreeConnection::Create(
          window_and_frame_.get(), std::move(request),
          mus::WindowTreeConnection::CreateType::DONT_WAIT_FOR_EMBED);
    } else {
      mus::WindowTreeConnection::Create(
          this, std::move(request),
          mus::WindowTreeConnection::CreateType::DONT_WAIT_FOR_EMBED);
    }
  }

  // Overridden from mojo::InterfaceFactory<mojom::FrameClient>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojom::FrameClient> request) override {
    ASSERT_TRUE(window_and_frame_);
    window_and_frame_->Bind(std::move(request));
  }

  scoped_ptr<TestFrameTreeDelegate> frame_tree_delegate_;
  scoped_ptr<FrameTree> frame_tree_;
  scoped_ptr<WindowAndFrame> root_window_and_frame_;

  mus::mojom::WindowTreeHostPtr host_;

  // Used to receive the most recent window manager loaded by an embed action.
  WindowTreeConnection* most_recent_connection_;
  // The Window Manager connection held by the window manager (app running at
  // the
  // root window).
  WindowTreeConnection* window_manager_;

  scoped_ptr<WindowAndFrame> window_and_frame_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(FrameTest);
};

// Verifies the FrameData supplied to the root FrameClient::OnConnect().
TEST_F(FrameTest, RootFrameClientConnectData) {
  mojo::Array<mojom::FrameDataPtr> frames =
      root_window_and_frame()->test_frame_client()->connect_frames();
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(root_window_and_frame()->window()->id(), frames[0]->frame_id);
  EXPECT_EQ(0u, frames[0]->parent_id);
}

// Verifies the FrameData supplied to a child FrameClient::OnConnect().
// Crashes on linux_chromium_rel_ng only. http://crbug.com/567337
#if defined(OS_LINUX)
#define MAYBE_ChildFrameClientConnectData DISABLED_ChildFrameClientConnectData
#else
#define MAYBE_ChildFrameClientConnectData ChildFrameClientConnectData
#endif
TEST_F(FrameTest, MAYBE_ChildFrameClientConnectData) {
  scoped_ptr<WindowAndFrame> child_view_and_frame(
      CreateChildWindowAndFrame(root_window_and_frame()));
  ASSERT_TRUE(child_view_and_frame);
  // Initially created child frames don't get OnConnect().
  EXPECT_EQ(0, child_view_and_frame->test_frame_client()->connect_count());

  scoped_ptr<WindowAndFrame> navigated_child_view_and_frame =
      NavigateFrame(child_view_and_frame.get());

  mojo::Array<mojom::FrameDataPtr> frames_in_child =
      navigated_child_view_and_frame->test_frame_client()->connect_frames();
  EXPECT_EQ(child_view_and_frame->window()->id(),
            navigated_child_view_and_frame->window()->id());
  // We expect 2 frames. One for the root, one for the child.
  ASSERT_EQ(2u, frames_in_child.size());
  EXPECT_EQ(frame_tree()->root()->id(), frames_in_child[0]->frame_id);
  EXPECT_EQ(0u, frames_in_child[0]->parent_id);
  EXPECT_EQ(navigated_child_view_and_frame->window()->id(),
            frames_in_child[1]->frame_id);
  EXPECT_EQ(frame_tree()->root()->id(), frames_in_child[1]->parent_id);
}

TEST_F(FrameTest, OnViewEmbeddedInFrameDisconnected) {
  scoped_ptr<WindowAndFrame> child_view_and_frame(
      CreateChildWindowAndFrame(root_window_and_frame()));
  ASSERT_TRUE(child_view_and_frame);

  scoped_ptr<WindowAndFrame> navigated_child_view_and_frame =
      NavigateFrame(child_view_and_frame.get());

  // Delete the WindowTreeConnection for the child, which should trigger
  // notification.
  delete navigated_child_view_and_frame->window()->connection();
  ASSERT_EQ(1u, frame_tree()->root()->children().size());
  ASSERT_NO_FATAL_FAILURE(frame_tree_delegate()->WaitForFrameDisconnected(
      frame_tree()->root()->children()[0]));
  ASSERT_EQ(1u, frame_tree()->root()->children().size());
}

TEST_F(FrameTest, NotifyRemoteParentWithLoadingState) {
  scoped_ptr<WindowAndFrame> child_view_and_frame(
      CreateChildWindowAndFrame(root_window_and_frame()));
  uint32_t child_frame_id = child_view_and_frame->window()->id();

  {
    base::RunLoop run_loop;
    root_window_and_frame()
        ->test_frame_client()
        ->set_on_loading_state_changed_callback(run_loop.QuitClosure());

    child_view_and_frame->server_frame()->LoadingStateChanged(true, .5);

    run_loop.Run();

    uint32_t frame_id = 0;
    bool loading = false;
    root_window_and_frame()
        ->test_frame_client()
        ->last_loading_state_changed_notification(&frame_id, &loading);
    EXPECT_EQ(child_frame_id, frame_id);
    EXPECT_TRUE(loading);
  }
  {
    base::RunLoop run_loop;
    root_window_and_frame()
        ->test_frame_client()
        ->set_on_loading_state_changed_callback(run_loop.QuitClosure());

    ASSERT_TRUE(child_view_and_frame);
    ASSERT_TRUE(child_view_and_frame->server_frame());

    child_view_and_frame->server_frame()->LoadingStateChanged(false, 1);

    run_loop.Run();

    uint32_t frame_id = 0;
    bool loading = false;
    root_window_and_frame()
        ->test_frame_client()
        ->last_loading_state_changed_notification(&frame_id, &loading);
    EXPECT_EQ(child_frame_id, frame_id);
    EXPECT_FALSE(loading);
  }
}

TEST_F(FrameTest, NotifyRemoteParentWithLoadEvent) {
  scoped_ptr<WindowAndFrame> child_view_and_frame(
      CreateChildWindowAndFrame(root_window_and_frame()));
  uint32_t child_frame_id = child_view_and_frame->window()->id();

  base::RunLoop run_loop;
  root_window_and_frame()
      ->test_frame_client()
      ->set_on_dispatch_load_event_callback(run_loop.QuitClosure());

  child_view_and_frame->server_frame()->DispatchLoadEventToParent();

  run_loop.Run();

  uint32_t frame_id = root_window_and_frame()
                          ->test_frame_client()
                          ->last_dispatch_load_event_frame_id();
  EXPECT_EQ(child_frame_id, frame_id);
}

TEST_F(FrameTest, PassAlongNavigationStartTime) {
  scoped_ptr<WindowAndFrame> child_view_and_frame(
      CreateChildWindowAndFrame(root_window_and_frame()));
  ASSERT_TRUE(child_view_and_frame);

  base::TimeTicks navigation_start_time = base::TimeTicks::FromInternalValue(1);
  scoped_ptr<WindowAndFrame> navigated_child_view_and_frame =
      NavigateFrameWithStartTime(child_view_and_frame.get(),
                                 navigation_start_time);
  EXPECT_EQ(navigation_start_time,
            navigated_child_view_and_frame->test_frame_client()
                ->last_navigation_start_time());
}

}  // namespace web_view
