// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/mus/ws/ids.h"
#include "components/mus/ws/test_change_tracker.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/shell/public/cpp/application_test_base.h"

using mojo::Array;
using mojo::Callback;
using mojo::Connection;
using mojo::InterfaceRequest;
using mojo::RectPtr;
using mojo::ShellClient;
using mojo::String;
using mus::mojom::ErrorCode;
using mus::mojom::EventPtr;
using mus::mojom::ViewportMetricsPtr;
using mus::mojom::WindowDataPtr;
using mus::mojom::WindowTree;
using mus::mojom::WindowTreeClient;

namespace mus {
namespace ws {

namespace {

// Creates an id used for transport from the specified parameters.
Id BuildWindowId(ConnectionSpecificId connection_id,
                 ConnectionSpecificId window_id) {
  return (connection_id << 16) | window_id;
}

// Callback function from WindowTree functions.
// ----------------------------------

void WindowTreeResultCallback(base::RunLoop* run_loop,
                              std::vector<TestWindow>* windows,
                              Array<WindowDataPtr> results) {
  WindowDatasToTestWindows(results, windows);
  run_loop->Quit();
}

void EmbedCallbackImpl(base::RunLoop* run_loop,
                       bool* result_cache,
                       bool result,
                       ConnectionSpecificId connection_id) {
  *result_cache = result;
  run_loop->Quit();
}

// -----------------------------------------------------------------------------

bool EmbedUrl(mojo::Shell* shell,
              WindowTree* ws,
              const String& url,
              Id root_id) {
  bool result = false;
  base::RunLoop run_loop;
  {
    mojom::WindowTreeClientPtr client;
    shell->ConnectToInterface(url.get(), &client);
    ws->Embed(root_id, std::move(client),
              mojom::WindowTree::kAccessPolicyDefault,
              base::Bind(&EmbedCallbackImpl, &run_loop, &result));
  }
  run_loop.Run();
  return result;
}

bool Embed(WindowTree* ws, Id root_id, mojom::WindowTreeClientPtr client) {
  bool result = false;
  base::RunLoop run_loop;
  {
    ws->Embed(root_id, std::move(client),
              mojom::WindowTree::kAccessPolicyDefault,
              base::Bind(&EmbedCallbackImpl, &run_loop, &result));
  }
  run_loop.Run();
  return result;
}

void GetWindowTree(WindowTree* ws,
                   Id window_id,
                   std::vector<TestWindow>* windows) {
  base::RunLoop run_loop;
  ws->GetWindowTree(window_id,
                    base::Bind(&WindowTreeResultCallback, &run_loop, windows));
  run_loop.Run();
}

// Utility functions -----------------------------------------------------------

const Id kNullParentId = 0;
std::string IdToString(Id id) {
  return (id == kNullParentId) ? "null" : base::StringPrintf(
                                              "%d,%d", HiWord(id), LoWord(id));
}

std::string WindowParentToString(Id window, Id parent) {
  return base::StringPrintf("window=%s parent=%s", IdToString(window).c_str(),
                            IdToString(parent).c_str());
}

// -----------------------------------------------------------------------------

// A WindowTreeClient implementation that logs all changes to a tracker.
class TestWindowTreeClientImpl : public mojom::WindowTreeClient,
                                 public TestChangeTracker::Delegate,
                                 public mojom::WindowManager {
 public:
  TestWindowTreeClientImpl()
      : binding_(this),
        connection_id_(0),
        root_window_id_(0),
        // Start with a random large number so tests can use lower ids if they
        // want.
        next_change_id_(10000),
        waiting_change_id_(0),
        on_change_completed_result_(false),
        track_root_bounds_changes_(false) {
    tracker_.set_delegate(this);
  }

  void Bind(mojo::InterfaceRequest<mojom::WindowTreeClient> request) {
    binding_.Bind(std::move(request));
  }

  mojom::WindowTree* tree() { return tree_.get(); }
  TestChangeTracker* tracker() { return &tracker_; }
  Id root_window_id() const { return root_window_id_; }

  // Sets whether changes to the bounds of the root should be tracked. Normally
  // they are ignored (as during startup we often times get random size
  // changes).
  void set_track_root_bounds_changes(bool value) {
    track_root_bounds_changes_ = value;
  }

  // Runs a nested MessageLoop until |count| changes (calls to
  // WindowTreeClient functions) have been received.
  void WaitForChangeCount(size_t count) {
    if (tracker_.changes()->size() >= count)
      return;

    ASSERT_TRUE(wait_state_.get() == nullptr);
    wait_state_.reset(new WaitState);
    wait_state_->change_count = count;
    wait_state_->run_loop.Run();
    wait_state_.reset();
  }

  uint32_t GetAndAdvanceChangeId() { return next_change_id_++; }

  // Runs a nested MessageLoop until OnEmbed() has been encountered.
  void WaitForOnEmbed() {
    if (tree_)
      return;
    embed_run_loop_.reset(new base::RunLoop);
    embed_run_loop_->Run();
    embed_run_loop_.reset();
  }

  bool WaitForIncomingMethodCall() {
    return binding_.WaitForIncomingMethodCall();
  }

  bool WaitForChangeCompleted(uint32_t id) {
    waiting_change_id_ = id;
    change_completed_run_loop_.reset(new base::RunLoop);
    change_completed_run_loop_->Run();
    return on_change_completed_result_;
  }

  bool DeleteWindow(Id id) {
    const uint32_t change_id = GetAndAdvanceChangeId();
    tree()->DeleteWindow(change_id, id);
    return WaitForChangeCompleted(change_id);
  }

  bool AddWindow(Id parent, Id child) {
    const uint32_t change_id = GetAndAdvanceChangeId();
    tree()->AddWindow(change_id, parent, child);
    return WaitForChangeCompleted(change_id);
  }

  bool RemoveWindowFromParent(Id window_id) {
    const uint32_t change_id = GetAndAdvanceChangeId();
    tree()->RemoveWindowFromParent(change_id, window_id);
    return WaitForChangeCompleted(change_id);
  }

  bool ReorderWindow(Id window_id,
                     Id relative_window_id,
                     mojom::OrderDirection direction) {
    const uint32_t change_id = GetAndAdvanceChangeId();
    tree()->ReorderWindow(change_id, window_id, relative_window_id, direction);
    return WaitForChangeCompleted(change_id);
  }

  // Waits for all messages to be received by |ws|. This is done by attempting
  // to create a bogus window. When we get the response we know all messages
  // have been processed.
  bool WaitForAllMessages() {
    return NewWindowWithCompleteId(WindowIdToTransportId(InvalidWindowId())) ==
           0;
  }

  Id NewWindow(ConnectionSpecificId window_id) {
    return NewWindowWithCompleteId(BuildWindowId(connection_id_, window_id));
  }

  // Generally you want NewWindow(), but use this if you need to test given
  // a complete window id (NewWindow() ors with the connection id).
  Id NewWindowWithCompleteId(Id id) {
    mojo::Map<mojo::String, mojo::Array<uint8_t>> properties;
    const uint32_t change_id = GetAndAdvanceChangeId();
    tree()->NewWindow(change_id, id, std::move(properties));
    return WaitForChangeCompleted(change_id) ? id : 0;
  }

  bool SetWindowProperty(Id window_id,
                         const std::string& name,
                         const std::vector<uint8_t>* data) {
    Array<uint8_t> mojo_data;
    if (data)
      mojo_data = Array<uint8_t>::From(*data);
    const uint32_t change_id = GetAndAdvanceChangeId();
    tree()->SetWindowProperty(change_id, window_id, name, std::move(mojo_data));
    return WaitForChangeCompleted(change_id);
  }

  bool SetPredefinedCursor(Id window_id, mojom::Cursor cursor) {
    const uint32_t change_id = GetAndAdvanceChangeId();
    tree()->SetPredefinedCursor(change_id, window_id, cursor);
    return WaitForChangeCompleted(change_id);
  }

  bool SetWindowVisibility(Id window_id, bool visible) {
    const uint32_t change_id = GetAndAdvanceChangeId();
    tree()->SetWindowVisibility(change_id, window_id, visible);
    return WaitForChangeCompleted(change_id);
  }

 private:
  // Used when running a nested MessageLoop.
  struct WaitState {
    WaitState() : change_count(0) {}

    // Number of changes waiting for.
    size_t change_count;
    base::RunLoop run_loop;
  };

  // TestChangeTracker::Delegate:
  void OnChangeAdded() override {
    if (wait_state_.get() &&
        tracker_.changes()->size() >= wait_state_->change_count) {
      wait_state_->run_loop.Quit();
    }
  }

  // WindowTreeClient:
  void OnEmbed(ConnectionSpecificId connection_id,
               WindowDataPtr root,
               mojom::WindowTreePtr tree,
               Id focused_window_id,
               uint32_t access_policy) override {
    // TODO(sky): add coverage of |focused_window_id|.
    ASSERT_TRUE(root);
    root_window_id_ = root->window_id;
    tree_ = std::move(tree);
    connection_id_ = connection_id;
    tracker()->OnEmbed(connection_id, std::move(root));
    if (embed_run_loop_)
      embed_run_loop_->Quit();
  }
  void OnEmbeddedAppDisconnected(Id window_id) override {
    tracker()->OnEmbeddedAppDisconnected(window_id);
  }
  void OnUnembed(Id window_id) override { tracker()->OnUnembed(window_id); }
  void OnTopLevelCreated(uint32_t change_id,
                         mojom::WindowDataPtr data) override {
    tracker()->OnTopLevelCreated(change_id, std::move(data));
  }
  void OnWindowBoundsChanged(Id window_id,
                             RectPtr old_bounds,
                             RectPtr new_bounds) override {
    // The bounds of the root may change during startup on Android at random
    // times. As this doesn't matter, and shouldn't impact test exepctations,
    // it is ignored.
    if (window_id == root_window_id_ && !track_root_bounds_changes_)
      return;
    tracker()->OnWindowBoundsChanged(window_id, std::move(old_bounds),
                                     std::move(new_bounds));
  }
  void OnClientAreaChanged(
      uint32_t window_id,
      mojo::InsetsPtr new_client_area,
      mojo::Array<mojo::RectPtr> new_additional_client_areas) override {}
  void OnTransientWindowAdded(uint32_t window_id,
                              uint32_t transient_window_id) override {
    tracker()->OnTransientWindowAdded(window_id, transient_window_id);
  }
  void OnTransientWindowRemoved(uint32_t window_id,
                                uint32_t transient_window_id) override {
    tracker()->OnTransientWindowRemoved(window_id, transient_window_id);
  }
  void OnWindowViewportMetricsChanged(mojo::Array<uint32_t> window_ids,
                                      ViewportMetricsPtr old_metrics,
                                      ViewportMetricsPtr new_metrics) override {
    // Don't track the metrics as they are available at an indeterministic time
    // on Android.
  }
  void OnWindowHierarchyChanged(Id window,
                                Id new_parent,
                                Id old_parent,
                                Array<WindowDataPtr> windows) override {
    tracker()->OnWindowHierarchyChanged(window, new_parent, old_parent,
                                        std::move(windows));
  }
  void OnWindowReordered(Id window_id,
                         Id relative_window_id,
                         mojom::OrderDirection direction) override {
    tracker()->OnWindowReordered(window_id, relative_window_id, direction);
  }
  void OnWindowDeleted(Id window) override {
    tracker()->OnWindowDeleted(window);
  }
  void OnWindowVisibilityChanged(uint32_t window, bool visible) override {
    tracker()->OnWindowVisibilityChanged(window, visible);
  }
  void OnWindowDrawnStateChanged(uint32_t window, bool drawn) override {
    tracker()->OnWindowDrawnStateChanged(window, drawn);
  }
  void OnWindowInputEvent(uint32_t event_id,
                          Id window_id,
                          EventPtr event) override {
    // Don't log input events as none of the tests care about them and they
    // may come in at random points.
  }
  void OnWindowSharedPropertyChanged(uint32_t window,
                                     const String& name,
                                     Array<uint8_t> new_data) override {
    tracker_.OnWindowSharedPropertyChanged(window, name, std::move(new_data));
  }
  // TODO(sky): add testing coverage.
  void OnWindowFocused(uint32_t focused_window_id) override {}
  void OnWindowPredefinedCursorChanged(uint32_t window_id,
                                       mojom::Cursor cursor_id) override {
    tracker_.OnWindowPredefinedCursorChanged(window_id, cursor_id);
  }
  void OnChangeCompleted(uint32_t change_id, bool success) override {
    if (waiting_change_id_ == change_id && change_completed_run_loop_) {
      on_change_completed_result_ = success;
      change_completed_run_loop_->Quit();
    }
  }
  void RequestClose(uint32_t window_id) override {}
  void GetWindowManager(mojo::AssociatedInterfaceRequest<mojom::WindowManager>
                            internal) override {
    window_manager_binding_.reset(
        new mojo::AssociatedBinding<mojom::WindowManager>(this,
                                                          std::move(internal)));
    tree_->GetWindowManagerClient(
        GetProxy(&window_manager_client_, tree_.associated_group()));
  }

  // mojom::WindowManager:
  void WmSetBounds(uint32_t change_id,
                   uint32_t window_id,
                   mojo::RectPtr bounds) override {
    window_manager_client_->WmResponse(change_id, false);
  }
  void WmSetProperty(uint32_t change_id,
                     uint32_t window_id,
                     const mojo::String& name,
                     mojo::Array<uint8_t> value) override {
    window_manager_client_->WmResponse(change_id, false);
  }
  void WmCreateTopLevelWindow(
      uint32_t change_id,
      mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) override {
    NOTIMPLEMENTED();
  }
  void OnAccelerator(uint32_t id, mojom::EventPtr event) override {
    NOTIMPLEMENTED();
  }

  TestChangeTracker tracker_;

  mojom::WindowTreePtr tree_;

  // If non-null we're waiting for OnEmbed() using this RunLoop.
  scoped_ptr<base::RunLoop> embed_run_loop_;

  // If non-null we're waiting for a certain number of change notifications to
  // be encountered.
  scoped_ptr<WaitState> wait_state_;

  mojo::Binding<WindowTreeClient> binding_;
  Id connection_id_;
  Id root_window_id_;
  uint32_t next_change_id_;
  uint32_t waiting_change_id_;
  bool on_change_completed_result_;
  bool track_root_bounds_changes_;
  scoped_ptr<base::RunLoop> change_completed_run_loop_;

  scoped_ptr<mojo::AssociatedBinding<mojom::WindowManager>>
      window_manager_binding_;
  mojom::WindowManagerClientAssociatedPtr window_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeClientImpl);
};

// -----------------------------------------------------------------------------

// InterfaceFactory for vending TestWindowTreeClientImpls.
class WindowTreeClientFactory
    : public mojo::InterfaceFactory<WindowTreeClient> {
 public:
  WindowTreeClientFactory() {}
  ~WindowTreeClientFactory() override {}

  // Runs a nested MessageLoop until a new instance has been created.
  scoped_ptr<TestWindowTreeClientImpl> WaitForInstance() {
    if (!client_impl_.get()) {
      DCHECK(!run_loop_);
      run_loop_.reset(new base::RunLoop);
      run_loop_->Run();
      run_loop_.reset();
    }
    return std::move(client_impl_);
  }

 private:
  // InterfaceFactory<WindowTreeClient>:
  void Create(Connection* connection,
              InterfaceRequest<WindowTreeClient> request) override {
    client_impl_.reset(new TestWindowTreeClientImpl());
    client_impl_->Bind(std::move(request));
    if (run_loop_.get())
      run_loop_->Quit();
  }

  scoped_ptr<TestWindowTreeClientImpl> client_impl_;
  scoped_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientFactory);
};

}  // namespace

class WindowTreeAppTest : public mojo::test::ApplicationTestBase,
                          public mojo::ShellClient {
 public:
  WindowTreeAppTest()
      : connection_id_1_(0), connection_id_2_(0), root_window_id_(0) {}
  ~WindowTreeAppTest() override {}

 protected:
  // Returns the changes from the various connections.
  std::vector<Change>* changes1() { return ws_client1_->tracker()->changes(); }
  std::vector<Change>* changes2() { return ws_client2_->tracker()->changes(); }
  std::vector<Change>* changes3() { return ws_client3_->tracker()->changes(); }

  // Various connections. |ws1()|, being the first connection, has special
  // permissions (it's treated as the window manager).
  WindowTree* ws1() { return ws_client1_->tree(); }
  WindowTree* ws2() { return ws_client2_->tree(); }
  WindowTree* ws3() { return ws_client3_->tree(); }

  TestWindowTreeClientImpl* ws_client1() { return ws_client1_.get(); }
  TestWindowTreeClientImpl* ws_client2() { return ws_client2_.get(); }
  TestWindowTreeClientImpl* ws_client3() { return ws_client3_.get(); }

  Id root_window_id() const { return root_window_id_; }

  int connection_id_1() const { return connection_id_1_; }
  int connection_id_2() const { return connection_id_2_; }

  void EstablishSecondConnectionWithRoot(Id root_id) {
    ASSERT_TRUE(ws_client2_.get() == nullptr);
    ws_client2_ =
        EstablishConnectionViaEmbed(ws1(), root_id, &connection_id_2_);
    ASSERT_GT(connection_id_2_, 0);
    ASSERT_TRUE(ws_client2_.get() != nullptr);
  }

  void EstablishSecondConnection(bool create_initial_window) {
    Id window_1_1 = 0;
    if (create_initial_window) {
      window_1_1 = ws_client1()->NewWindow(1);
      ASSERT_TRUE(window_1_1);
    }
    ASSERT_NO_FATAL_FAILURE(
        EstablishSecondConnectionWithRoot(BuildWindowId(connection_id_1(), 1)));

    if (create_initial_window) {
      EXPECT_EQ("[" + WindowParentToString(window_1_1, kNullParentId) + "]",
                ChangeWindowDescription(*changes2()));
    }
  }

  void EstablishThirdConnection(WindowTree* owner, Id root_id) {
    ASSERT_TRUE(ws_client3_.get() == nullptr);
    ws_client3_ = EstablishConnectionViaEmbed(owner, root_id, nullptr);
    ASSERT_TRUE(ws_client3_.get() != nullptr);
  }

  scoped_ptr<TestWindowTreeClientImpl> WaitForWindowTreeClient() {
    return client_factory_->WaitForInstance();
  }

  // Establishes a new connection by way of Embed() on the specified
  // WindowTree.
  scoped_ptr<TestWindowTreeClientImpl> EstablishConnectionViaEmbed(
      WindowTree* owner,
      Id root_id,
      int* connection_id) {
    return EstablishConnectionViaEmbedWithPolicyBitmask(
        owner, root_id, mojom::WindowTree::kAccessPolicyDefault, connection_id);
  }

  scoped_ptr<TestWindowTreeClientImpl>
  EstablishConnectionViaEmbedWithPolicyBitmask(WindowTree* owner,
                                               Id root_id,
                                               uint32_t policy_bitmask,
                                               int* connection_id) {
    if (!EmbedUrl(shell(), owner, shell_url(), root_id)) {
      ADD_FAILURE() << "Embed() failed";
      return nullptr;
    }
    scoped_ptr<TestWindowTreeClientImpl> client =
        client_factory_->WaitForInstance();
    if (!client.get()) {
      ADD_FAILURE() << "WaitForInstance failed";
      return nullptr;
    }
    client->WaitForOnEmbed();

    EXPECT_EQ("OnEmbed",
              SingleChangeToDescription(*client->tracker()->changes()));
    if (connection_id)
      *connection_id = (*client->tracker()->changes())[0].connection_id;
    return client;
  }

  // ApplicationTestBase:
  mojo::ShellClient* GetShellClient() override { return this; }
  void SetUp() override {
    ApplicationTestBase::SetUp();
    client_factory_.reset(new WindowTreeClientFactory());

    mojom::WindowTreeHostFactoryPtr factory;
    shell()->ConnectToInterface("mojo:mus", &factory);

    mojom::WindowTreeClientPtr tree_client_ptr;
    ws_client1_.reset(new TestWindowTreeClientImpl());
    ws_client1_->Bind(GetProxy(&tree_client_ptr));

    factory->CreateWindowTreeHost(GetProxy(&host_),
                                  std::move(tree_client_ptr));

    // Next we should get an embed call on the "window manager" client.
    ws_client1_->WaitForIncomingMethodCall();

    ASSERT_EQ(1u, changes1()->size());
    EXPECT_EQ(CHANGE_TYPE_EMBED, (*changes1())[0].type);
    // All these tests assume 1 for the client id. The only real assertion here
    // is the client id is not zero, but adding this as rest of code here
    // assumes 1.
    ASSERT_GT((*changes1())[0].connection_id, 0);
    connection_id_1_ = (*changes1())[0].connection_id;
    ASSERT_FALSE((*changes1())[0].windows.empty());
    root_window_id_ = (*changes1())[0].windows[0].window_id;
    ASSERT_EQ(root_window_id_, ws_client1_->root_window_id());
    changes1()->clear();
  }

  // mojo::ShellClient implementation.
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface(client_factory_.get());
    return true;
  }

  scoped_ptr<TestWindowTreeClientImpl> ws_client1_;
  scoped_ptr<TestWindowTreeClientImpl> ws_client2_;
  scoped_ptr<TestWindowTreeClientImpl> ws_client3_;

  mojom::WindowTreeHostPtr host_;

 private:
  scoped_ptr<WindowTreeClientFactory> client_factory_;
  int connection_id_1_;
  int connection_id_2_;
  Id root_window_id_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(WindowTreeAppTest);
};

// Verifies two clients/connections get different ids.
TEST_F(WindowTreeAppTest, TwoClientsGetDifferentConnectionIds) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  ASSERT_EQ(1u, changes2()->size());
  ASSERT_NE(connection_id_1(), connection_id_2());
}

// Verifies when Embed() is invoked any child windows are removed.
TEST_F(WindowTreeAppTest, WindowsRemovedWhenEmbedding) {
  // Two windows 1 and 2. 2 is parented to 1.
  Id window_1_1 = ws_client1()->NewWindow(1);
  ASSERT_TRUE(window_1_1);
  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));

  Id window_1_2 = ws_client1()->NewWindow(2);
  ASSERT_TRUE(window_1_2);
  ASSERT_TRUE(ws_client1()->AddWindow(window_1_1, window_1_2));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));
  ASSERT_EQ(1u, changes2()->size());
  ASSERT_EQ(1u, (*changes2())[0].windows.size());
  EXPECT_EQ("[" + WindowParentToString(window_1_1, kNullParentId) + "]",
            ChangeWindowDescription(*changes2()));

  // Embed() removed window 2.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws1(), window_1_2, &windows);
    EXPECT_EQ(WindowParentToString(window_1_2, kNullParentId),
              SingleWindowDescription(windows));
  }

  // ws2 should not see window 2.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws2(), window_1_1, &windows);
    EXPECT_EQ(WindowParentToString(window_1_1, kNullParentId),
              SingleWindowDescription(windows));
  }
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws2(), window_1_2, &windows);
    EXPECT_TRUE(windows.empty());
  }

  // Windows 3 and 4 in connection 2.
  Id window_2_3 = ws_client2()->NewWindow(3);
  Id window_2_4 = ws_client2()->NewWindow(4);
  ASSERT_TRUE(window_2_3);
  ASSERT_TRUE(window_2_4);
  ASSERT_TRUE(ws_client2()->AddWindow(window_2_3, window_2_4));

  // Connection 3 rooted at 2.
  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(ws2(), window_2_3));

  // Window 4 should no longer have a parent.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws2(), window_2_3, &windows);
    EXPECT_EQ(WindowParentToString(window_2_3, kNullParentId),
              SingleWindowDescription(windows));

    windows.clear();
    GetWindowTree(ws2(), window_2_4, &windows);
    EXPECT_EQ(WindowParentToString(window_2_4, kNullParentId),
              SingleWindowDescription(windows));
  }

  // And window 4 should not be visible to connection 3.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws3(), window_2_3, &windows);
    EXPECT_EQ("no windows", SingleWindowDescription(windows));
  }
}

// Verifies once Embed() has been invoked the parent connection can't see any
// children.
TEST_F(WindowTreeAppTest, CantAccessChildrenOfEmbeddedWindow) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  Id window_1_1 = BuildWindowId(connection_id_1(), 1);
  Id window_2_2 = ws_client2()->NewWindow(2);
  ASSERT_TRUE(window_2_2);
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_2));

  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(ws2(), window_2_2));

  Id window_3_3 = ws_client3()->NewWindow(3);
  ASSERT_TRUE(window_3_3);
  ASSERT_TRUE(
      ws_client3()->AddWindow(ws_client3()->root_window_id(), window_3_3));

  // Even though 3 is a child of 2 connection 2 can't see 3 as it's from a
  // different connection.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws2(), window_2_2, &windows);
    EXPECT_EQ(WindowParentToString(window_2_2, window_1_1),
              SingleWindowDescription(windows));
  }

  // Connection 2 shouldn't be able to get window 3 at all.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws2(), window_3_3, &windows);
    EXPECT_TRUE(windows.empty());
  }

  // Connection 1 should be able to see it all (its the root).
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws1(), window_1_1, &windows);
    ASSERT_EQ(3u, windows.size());
    EXPECT_EQ(WindowParentToString(window_1_1, kNullParentId),
              windows[0].ToString());
    // NOTE: we expect a match of WindowParentToString(window_2_2, window_1_1),
    // but the ids are in the id space of client2, which is not the same as
    // the id space of ws1().
    EXPECT_EQ("window=2,1 parent=1,1", windows[1].ToString());
    // Same thing here, we really want to test for
    // WindowParentToString(window_3_3, window_2_2).
    EXPECT_EQ("window=3,1 parent=2,1", windows[2].ToString());
  }
}

// Verifies once Embed() has been invoked the parent can't mutate the children.
TEST_F(WindowTreeAppTest, CantModifyChildrenOfEmbeddedWindow) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  Id window_1_1 = BuildWindowId(connection_id_1(), 1);
  Id window_2_1 = ws_client2()->NewWindow(1);
  ASSERT_TRUE(window_2_1);
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_1));

  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(ws2(), window_2_1));

  Id window_2_2 = ws_client2()->NewWindow(2);
  ASSERT_TRUE(window_2_2);
  // Connection 2 shouldn't be able to add anything to the window anymore.
  ASSERT_FALSE(ws_client2()->AddWindow(window_2_1, window_2_2));

  // Create window 3 in connection 3 and add it to window 3.
  Id window_3_1 = ws_client3()->NewWindow(1);
  ASSERT_TRUE(window_3_1);
  ASSERT_TRUE(ws_client3()->AddWindow(window_2_1, window_3_1));

  // Connection 2 shouldn't be able to remove window 3.
  ASSERT_FALSE(ws_client2()->RemoveWindowFromParent(window_3_1));
}

// Verifies client gets a valid id.
TEST_F(WindowTreeAppTest, NewWindow) {
  Id window_1_1 = ws_client1()->NewWindow(1);
  ASSERT_TRUE(window_1_1);
  EXPECT_TRUE(changes1()->empty());

  // Can't create a window with the same id.
  ASSERT_EQ(0u, ws_client1()->NewWindowWithCompleteId(window_1_1));
  EXPECT_TRUE(changes1()->empty());

  // Can't create a window with a bogus connection id.
  ASSERT_EQ(0u, ws_client1()->NewWindowWithCompleteId(
                    BuildWindowId(connection_id_1() + 1, 1)));
  EXPECT_TRUE(changes1()->empty());
}

// Verifies AddWindow fails when window is already in position.
TEST_F(WindowTreeAppTest, AddWindowWithNoChange) {
  // Create the embed point now so that the ids line up.
  ASSERT_TRUE(ws_client1()->NewWindow(1));
  Id window_1_2 = ws_client1()->NewWindow(2);
  Id window_1_3 = ws_client1()->NewWindow(3);
  ASSERT_TRUE(window_1_2);
  ASSERT_TRUE(window_1_3);

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Make 3 a child of 2.
  ASSERT_TRUE(ws_client1()->AddWindow(window_1_2, window_1_3));

  // Try again, this should fail.
  EXPECT_FALSE(ws_client1()->AddWindow(window_1_2, window_1_3));
}

// Verifies AddWindow fails when window is already in position.
TEST_F(WindowTreeAppTest, AddAncestorFails) {
  // Create the embed point now so that the ids line up.
  ASSERT_TRUE(ws_client1()->NewWindow(1));
  Id window_1_2 = ws_client1()->NewWindow(2);
  Id window_1_3 = ws_client1()->NewWindow(3);
  ASSERT_TRUE(window_1_2);
  ASSERT_TRUE(window_1_3);

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Make 3 a child of 2.
  ASSERT_TRUE(ws_client1()->AddWindow(window_1_2, window_1_3));

  // Try to make 2 a child of 3, this should fail since 2 is an ancestor of 3.
  EXPECT_FALSE(ws_client1()->AddWindow(window_1_3, window_1_2));
}

// Verifies adding to root sends right notifications.
TEST_F(WindowTreeAppTest, AddToRoot) {
  // Create the embed point now so that the ids line up.
  Id window_1_1 = ws_client1()->NewWindow(1);
  ASSERT_TRUE(window_1_1);
  Id window_1_21 = ws_client1()->NewWindow(21);
  Id window_1_3 = ws_client1()->NewWindow(3);
  ASSERT_TRUE(window_1_21);
  ASSERT_TRUE(window_1_3);

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));
  changes2()->clear();

  // Make 3 a child of 21.
  ASSERT_TRUE(ws_client1()->AddWindow(window_1_21, window_1_3));

  // Make 21 a child of 1.
  ASSERT_TRUE(ws_client1()->AddWindow(window_1_1, window_1_21));

  // Connection 2 should not be told anything (because the window is from a
  // different connection). Create a window to ensure we got a response from
  // the server.
  ASSERT_TRUE(ws_client2()->NewWindow(100));
  EXPECT_TRUE(changes2()->empty());
}

// Verifies HierarchyChanged is correctly sent for various adds/removes.
TEST_F(WindowTreeAppTest, WindowHierarchyChangedWindows) {
  // Create the embed point now so that the ids line up.
  Id window_1_1 = ws_client1()->NewWindow(1);
  // 1,2->1,11.
  Id window_1_2 = ws_client1()->NewWindow(2);
  ASSERT_TRUE(window_1_2);
  ASSERT_TRUE(ws_client1()->SetWindowVisibility(window_1_2, true));
  Id window_1_11 = ws_client1()->NewWindow(11);
  ASSERT_TRUE(window_1_11);
  ASSERT_TRUE(ws_client1()->SetWindowVisibility(window_1_11, true));
  ASSERT_TRUE(ws_client1()->AddWindow(window_1_2, window_1_11));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));
  ASSERT_TRUE(ws_client1()->SetWindowVisibility(window_1_1, true));

  ASSERT_TRUE(ws_client2()->WaitForAllMessages());
  changes2()->clear();

  // 1,1->1,2->1,11
  {
    // Client 2 should not get anything (1,2 is from another connection).
    ASSERT_TRUE(ws_client1()->AddWindow(window_1_1, window_1_2));
    ASSERT_TRUE(ws_client2()->WaitForAllMessages());
    EXPECT_TRUE(changes2()->empty());
  }

  // 0,1->1,1->1,2->1,11.
  {
    // Client 2 is now connected to the root, so it should have gotten a drawn
    // notification.
    ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));
    ws_client2_->WaitForChangeCount(1u);
    EXPECT_EQ(
        "DrawnStateChanged window=" + IdToString(window_1_1) + " drawn=true",
        SingleChangeToDescription(*changes2()));
  }

  // 1,1->1,2->1,11.
  {
    // Client 2 is no longer connected to the root, should get drawn state
    // changed.
    changes2()->clear();
    ASSERT_TRUE(ws_client1()->RemoveWindowFromParent(window_1_1));
    ws_client2_->WaitForChangeCount(1);
    EXPECT_EQ(
        "DrawnStateChanged window=" + IdToString(window_1_1) + " drawn=false",
        SingleChangeToDescription(*changes2()));
  }

  // 1,1->1,2->1,11->1,111.
  Id window_1_111 = ws_client1()->NewWindow(111);
  ASSERT_TRUE(window_1_111);
  ASSERT_TRUE(ws_client1()->SetWindowVisibility(window_1_111, true));
  {
    changes2()->clear();
    ASSERT_TRUE(ws_client1()->AddWindow(window_1_11, window_1_111));
    ASSERT_TRUE(ws_client2()->WaitForAllMessages());
    EXPECT_TRUE(changes2()->empty());
  }

  // 0,1->1,1->1,2->1,11->1,111
  {
    changes2()->clear();
    ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));
    ws_client2_->WaitForChangeCount(1);
    EXPECT_EQ(
        "DrawnStateChanged window=" + IdToString(window_1_1) + " drawn=true",
        SingleChangeToDescription(*changes2()));
  }
}

TEST_F(WindowTreeAppTest, WindowHierarchyChangedAddingKnownToUnknown) {
  // Create the following structure: root -> 1 -> 11 and 2->21 (2 has no
  // parent).
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id window_1_1 = BuildWindowId(connection_id_1(), 1);

  Id window_2_11 = ws_client2()->NewWindow(11);
  Id window_2_2 = ws_client2()->NewWindow(2);
  Id window_2_21 = ws_client2()->NewWindow(21);
  ASSERT_TRUE(window_2_11);
  ASSERT_TRUE(window_2_2);
  ASSERT_TRUE(window_2_21);

  // Set up the hierarchy.
  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_11));
  ASSERT_TRUE(ws_client2()->AddWindow(window_2_2, window_2_21));

  // Remove 11, should result in a hierarchy change for the root.
  {
    changes1()->clear();
    ASSERT_TRUE(ws_client2()->RemoveWindowFromParent(window_2_11));

    ws_client1_->WaitForChangeCount(1);
    // 2,1 should be IdToString(window_2_11), but window_2_11 is in the id
    // space of client2, not client1.
    EXPECT_EQ("HierarchyChanged window=2,1 new_parent=null old_parent=" +
                  IdToString(window_1_1),
              SingleChangeToDescription(*changes1()));
  }

  // Add 2 to 1.
  {
    changes1()->clear();
    ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_2));
    ws_client1_->WaitForChangeCount(1);
    EXPECT_EQ("HierarchyChanged window=" + IdToString(window_2_2) +
                  " new_parent=" + IdToString(window_1_1) + " old_parent=null",
              SingleChangeToDescription(*changes1()));
    // "window=2,3 parent=2,2]" should be,
    // WindowParentToString(window_2_21, window_2_2), but isn't because of
    // differing id spaces.
    EXPECT_EQ("[" + WindowParentToString(window_2_2, window_1_1) +
                  "],[window=2,3 parent=2,2]",
              ChangeWindowDescription(*changes1()));
  }
}

TEST_F(WindowTreeAppTest, ReorderWindow) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  Id window_2_1 = ws_client2()->NewWindow(1);
  Id window_2_2 = ws_client2()->NewWindow(2);
  Id window_2_3 = ws_client2()->NewWindow(3);
  Id window_1_4 = ws_client1()->NewWindow(4);  // Peer to 1,1
  Id window_1_5 = ws_client1()->NewWindow(5);  // Peer to 1,1
  Id window_2_6 = ws_client2()->NewWindow(6);  // Child of 1,2.
  Id window_2_7 = ws_client2()->NewWindow(7);  // Unparented.
  Id window_2_8 = ws_client2()->NewWindow(8);  // Unparented.
  ASSERT_TRUE(window_2_1);
  ASSERT_TRUE(window_2_2);
  ASSERT_TRUE(window_2_3);
  ASSERT_TRUE(window_1_4);
  ASSERT_TRUE(window_1_5);
  ASSERT_TRUE(window_2_6);
  ASSERT_TRUE(window_2_7);
  ASSERT_TRUE(window_2_8);

  ASSERT_TRUE(ws_client2()->AddWindow(window_2_1, window_2_2));
  ASSERT_TRUE(ws_client2()->AddWindow(window_2_2, window_2_6));
  ASSERT_TRUE(ws_client2()->AddWindow(window_2_1, window_2_3));
  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_4));
  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_5));
  ASSERT_TRUE(
      ws_client2()->AddWindow(BuildWindowId(connection_id_1(), 1), window_2_1));

  {
    changes1()->clear();
    ASSERT_TRUE(ws_client2()->ReorderWindow(window_2_2, window_2_3,
                                            mojom::OrderDirection::ABOVE));

    ws_client1_->WaitForChangeCount(1);
    EXPECT_EQ("Reordered window=" + IdToString(window_2_2) + " relative=" +
                  IdToString(window_2_3) + " direction=above",
              SingleChangeToDescription(*changes1()));
  }

  {
    changes1()->clear();
    ASSERT_TRUE(ws_client2()->ReorderWindow(window_2_2, window_2_3,
                                            mojom::OrderDirection::BELOW));

    ws_client1_->WaitForChangeCount(1);
    EXPECT_EQ("Reordered window=" + IdToString(window_2_2) + " relative=" +
                  IdToString(window_2_3) + " direction=below",
              SingleChangeToDescription(*changes1()));
  }

  // view2 is already below view3.
  EXPECT_FALSE(ws_client2()->ReorderWindow(window_2_2, window_2_3,
                                           mojom::OrderDirection::BELOW));

  // view4 & 5 are unknown to connection2_.
  EXPECT_FALSE(ws_client2()->ReorderWindow(window_1_4, window_1_5,
                                           mojom::OrderDirection::ABOVE));

  // view6 & view3 have different parents.
  EXPECT_FALSE(ws_client1()->ReorderWindow(window_2_3, window_2_6,
                                           mojom::OrderDirection::ABOVE));

  // Non-existent window-ids
  EXPECT_FALSE(ws_client1()->ReorderWindow(BuildWindowId(connection_id_1(), 27),
                                           BuildWindowId(connection_id_1(), 28),
                                           mojom::OrderDirection::ABOVE));

  // view7 & view8 are un-parented.
  EXPECT_FALSE(ws_client1()->ReorderWindow(window_2_7, window_2_8,
                                           mojom::OrderDirection::ABOVE));
}

// Verifies DeleteWindow works.
TEST_F(WindowTreeAppTest, DeleteWindow) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id window_1_1 = BuildWindowId(connection_id_1(), 1);
  Id window_2_1 = ws_client2()->NewWindow(1);
  ASSERT_TRUE(window_2_1);

  // Make 2 a child of 1.
  {
    changes1()->clear();
    ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_1));
    ws_client1_->WaitForChangeCount(1);
    EXPECT_EQ("HierarchyChanged window=" + IdToString(window_2_1) +
                  " new_parent=" + IdToString(window_1_1) + " old_parent=null",
              SingleChangeToDescription(*changes1()));
  }

  // Delete 2.
  {
    changes1()->clear();
    changes2()->clear();
    ASSERT_TRUE(ws_client2()->DeleteWindow(window_2_1));
    EXPECT_TRUE(changes2()->empty());

    ws_client1_->WaitForChangeCount(1);
    EXPECT_EQ("WindowDeleted window=" + IdToString(window_2_1),
              SingleChangeToDescription(*changes1()));
  }
}

// Verifies DeleteWindow isn't allowed from a separate connection.
TEST_F(WindowTreeAppTest, DeleteWindowFromAnotherConnectionDisallowed) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  EXPECT_FALSE(ws_client2()->DeleteWindow(BuildWindowId(connection_id_1(), 1)));
}

// Verifies if a window was deleted and then reused that other clients are
// properly notified.
TEST_F(WindowTreeAppTest, ReuseDeletedWindowId) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id window_1_1 = BuildWindowId(connection_id_1(), 1);
  Id window_2_1 = ws_client2()->NewWindow(1);
  ASSERT_TRUE(window_2_1);

  // Add 2 to 1.
  {
    changes1()->clear();
    ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_1));
    ws_client1_->WaitForChangeCount(1);
    EXPECT_EQ("HierarchyChanged window=" + IdToString(window_2_1) +
                  " new_parent=" + IdToString(window_1_1) + " old_parent=null",
              SingleChangeToDescription(*changes1()));
    EXPECT_EQ("[" + WindowParentToString(window_2_1, window_1_1) + "]",
              ChangeWindowDescription(*changes1()));
  }

  // Delete 2.
  {
    changes1()->clear();
    ASSERT_TRUE(ws_client2()->DeleteWindow(window_2_1));

    ws_client1_->WaitForChangeCount(1);
    EXPECT_EQ("WindowDeleted window=" + IdToString(window_2_1),
              SingleChangeToDescription(*changes1()));
  }

  // Create 2 again, and add it back to 1. Should get the same notification.
  window_2_1 = ws_client2()->NewWindow(2);
  ASSERT_TRUE(window_2_1);
  {
    changes1()->clear();
    ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_1));

    ws_client1_->WaitForChangeCount(1);
    EXPECT_EQ("HierarchyChanged window=" + IdToString(window_2_1) +
                  " new_parent=" + IdToString(window_1_1) + " old_parent=null",
              SingleChangeToDescription(*changes1()));
    EXPECT_EQ("[" + WindowParentToString(window_2_1, window_1_1) + "]",
              ChangeWindowDescription(*changes1()));
  }
}

// Assertions for GetWindowTree.
TEST_F(WindowTreeAppTest, GetWindowTree) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id window_1_1 = BuildWindowId(connection_id_1(), 1);

  // Create 11 in first connection and make it a child of 1.
  Id window_1_11 = ws_client1()->NewWindow(11);
  ASSERT_TRUE(window_1_11);
  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));
  ASSERT_TRUE(ws_client1()->AddWindow(window_1_1, window_1_11));

  // Create two windows in second connection, 2 and 3, both children of 1.
  Id window_2_1 = ws_client2()->NewWindow(1);
  Id window_2_2 = ws_client2()->NewWindow(2);
  ASSERT_TRUE(window_2_1);
  ASSERT_TRUE(window_2_2);
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_1));
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_2));

  // Verifies GetWindowTree() on the root. The root connection sees all.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws1(), root_window_id(), &windows);
    ASSERT_EQ(5u, windows.size());
    EXPECT_EQ(WindowParentToString(root_window_id(), kNullParentId),
              windows[0].ToString());
    EXPECT_EQ(WindowParentToString(window_1_1, root_window_id()),
              windows[1].ToString());
    EXPECT_EQ(WindowParentToString(window_1_11, window_1_1),
              windows[2].ToString());
    EXPECT_EQ(WindowParentToString(window_2_1, window_1_1),
              windows[3].ToString());
    EXPECT_EQ(WindowParentToString(window_2_2, window_1_1),
              windows[4].ToString());
  }

  // Verifies GetWindowTree() on the window 1,1 from ws2(). ws2() sees 1,1 as
  // 1,1
  // is ws2()'s root and ws2() sees all the windows it created.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws2(), window_1_1, &windows);
    ASSERT_EQ(3u, windows.size());
    EXPECT_EQ(WindowParentToString(window_1_1, kNullParentId),
              windows[0].ToString());
    EXPECT_EQ(WindowParentToString(window_2_1, window_1_1),
              windows[1].ToString());
    EXPECT_EQ(WindowParentToString(window_2_2, window_1_1),
              windows[2].ToString());
  }

  // Connection 2 shouldn't be able to get the root tree.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws2(), root_window_id(), &windows);
    ASSERT_EQ(0u, windows.size());
  }
}

TEST_F(WindowTreeAppTest, SetWindowBounds) {
  Id window_1_1 = ws_client1()->NewWindow(1);
  ASSERT_TRUE(window_1_1);
  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  changes2()->clear();

  ws_client2_->set_track_root_bounds_changes(true);

  ws1()->SetWindowBounds(10, window_1_1,
                         mojo::Rect::From(gfx::Rect(0, 0, 100, 100)));
  ASSERT_TRUE(ws_client1()->WaitForChangeCompleted(10));

  ws_client2_->WaitForChangeCount(1);
  EXPECT_EQ("BoundsChanged window=" + IdToString(window_1_1) +
                " old_bounds=0,0 0x0 new_bounds=0,0 100x100",
            SingleChangeToDescription(*changes2()));

  // Should not be possible to change the bounds of a window created by another
  // connection.
  ws2()->SetWindowBounds(11, window_1_1,
                         mojo::Rect::From(gfx::Rect(0, 0, 0, 0)));
  ASSERT_FALSE(ws_client2()->WaitForChangeCompleted(11));
}

// Verify AddWindow fails when trying to manipulate windows in other roots.
TEST_F(WindowTreeAppTest, CantMoveWindowsFromOtherRoot) {
  // Create 1 and 2 in the first connection.
  Id window_1_1 = ws_client1()->NewWindow(1);
  Id window_1_2 = ws_client1()->NewWindow(2);
  ASSERT_TRUE(window_1_1);
  ASSERT_TRUE(window_1_2);

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Try to move 2 to be a child of 1 from connection 2. This should fail as 2
  // should not be able to access 1.
  ASSERT_FALSE(ws_client2()->AddWindow(window_1_1, window_1_2));

  // Try to reparent 1 to the root. A connection is not allowed to reparent its
  // roots.
  ASSERT_FALSE(ws_client2()->AddWindow(root_window_id(), window_1_1));
}

// Verify RemoveWindowFromParent fails for windows that are descendants of the
// roots.
TEST_F(WindowTreeAppTest, CantRemoveWindowsInOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  Id window_1_1 = ws_client1()->NewWindow(1);
  Id window_1_2 = ws_client1()->NewWindow(2);
  ASSERT_TRUE(window_1_1);
  ASSERT_TRUE(window_1_2);

  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));
  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_2));

  // Establish the second connection and give it the root 1.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  // Connection 2 should not be able to remove window 2 or 1 from its parent.
  ASSERT_FALSE(ws_client2()->RemoveWindowFromParent(window_1_2));
  ASSERT_FALSE(ws_client2()->RemoveWindowFromParent(window_1_1));

  // Create windows 10 and 11 in 2.
  Id window_2_10 = ws_client2()->NewWindow(10);
  Id window_2_11 = ws_client2()->NewWindow(11);
  ASSERT_TRUE(window_2_10);
  ASSERT_TRUE(window_2_11);

  // Parent 11 to 10.
  ASSERT_TRUE(ws_client2()->AddWindow(window_2_10, window_2_11));
  // Remove 11 from 10.
  ASSERT_TRUE(ws_client2()->RemoveWindowFromParent(window_2_11));

  // Verify nothing was actually removed.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws1(), root_window_id(), &windows);
    ASSERT_EQ(3u, windows.size());
    EXPECT_EQ(WindowParentToString(root_window_id(), kNullParentId),
              windows[0].ToString());
    EXPECT_EQ(WindowParentToString(window_1_1, root_window_id()),
              windows[1].ToString());
    EXPECT_EQ(WindowParentToString(window_1_2, root_window_id()),
              windows[2].ToString());
  }
}

// Verify GetWindowTree fails for windows that are not descendants of the roots.
TEST_F(WindowTreeAppTest, CantGetWindowTreeOfOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  Id window_1_1 = ws_client1()->NewWindow(1);
  Id window_1_2 = ws_client1()->NewWindow(2);
  ASSERT_TRUE(window_1_1);
  ASSERT_TRUE(window_1_2);

  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));
  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_2));

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));

  std::vector<TestWindow> windows;

  // Should get nothing for the root.
  GetWindowTree(ws2(), root_window_id(), &windows);
  ASSERT_TRUE(windows.empty());

  // Should get nothing for window 2.
  GetWindowTree(ws2(), window_1_2, &windows);
  ASSERT_TRUE(windows.empty());

  // Should get window 1 if asked for.
  GetWindowTree(ws2(), window_1_1, &windows);
  ASSERT_EQ(1u, windows.size());
  EXPECT_EQ(WindowParentToString(window_1_1, kNullParentId),
            windows[0].ToString());
}

TEST_F(WindowTreeAppTest, EmbedWithSameWindowId) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  changes2()->clear();

  Id window_1_1 = BuildWindowId(connection_id_1(), 1);
  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(ws1(), window_1_1));

  // Connection2 should have been told of the unembed and delete.
  {
    ws_client2_->WaitForChangeCount(2);
    EXPECT_EQ("OnUnembed window=" + IdToString(window_1_1),
              ChangesToDescription1(*changes2())[0]);
    EXPECT_EQ("WindowDeleted window=" + IdToString(window_1_1),
              ChangesToDescription1(*changes2())[1]);
  }

  // Connection2 has no root. Verify it can't see window 1,1 anymore.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws2(), window_1_1, &windows);
    EXPECT_TRUE(windows.empty());
  }
}

TEST_F(WindowTreeAppTest, EmbedWithSameWindowId2) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id window_1_1 = BuildWindowId(connection_id_1(), 1);
  changes2()->clear();

  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(ws1(), window_1_1));

  // Connection2 should have been told about the unembed and delete.
  ws_client2_->WaitForChangeCount(2);
  changes2()->clear();

  // Create a window in the third connection and parent it to the root.
  Id window_3_1 = ws_client3()->NewWindow(1);
  ASSERT_TRUE(window_3_1);
  ASSERT_TRUE(ws_client3()->AddWindow(window_1_1, window_3_1));

  // Connection 1 should have been told about the add (it owns the window).
  {
    ws_client1_->WaitForChangeCount(1);
    EXPECT_EQ("HierarchyChanged window=" + IdToString(window_3_1) +
                  " new_parent=" + IdToString(window_1_1) + " old_parent=null",
              SingleChangeToDescription(*changes1()));
  }

  // Embed 1,1 again.
  {
    changes3()->clear();

    // We should get a new connection for the new embedding.
    scoped_ptr<TestWindowTreeClientImpl> connection4(
        EstablishConnectionViaEmbed(ws1(), window_1_1, nullptr));
    ASSERT_TRUE(connection4.get());
    EXPECT_EQ("[" + WindowParentToString(window_1_1, kNullParentId) + "]",
              ChangeWindowDescription(*connection4->tracker()->changes()));

    // And 3 should get an unembed and delete.
    ws_client3_->WaitForChangeCount(2);
    EXPECT_EQ("OnUnembed window=" + IdToString(window_1_1),
              ChangesToDescription1(*changes3())[0]);
    EXPECT_EQ("WindowDeleted window=" + IdToString(window_1_1),
              ChangesToDescription1(*changes3())[1]);
  }

  // ws3() has no root. Verify it can't see window 1,1 anymore.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws3(), window_1_1, &windows);
    EXPECT_TRUE(windows.empty());
  }

  // Verify 3,1 is no longer parented to 1,1. We have to do this from 1,1 as
  // ws3() can no longer see 1,1.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws1(), window_1_1, &windows);
    ASSERT_EQ(1u, windows.size());
    EXPECT_EQ(WindowParentToString(window_1_1, kNullParentId),
              windows[0].ToString());
  }

  // Verify ws3() can still see the window it created 3,1.
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws3(), window_3_1, &windows);
    ASSERT_EQ(1u, windows.size());
    EXPECT_EQ(WindowParentToString(window_3_1, kNullParentId),
              windows[0].ToString());
  }
}

// Assertions for SetWindowVisibility.
TEST_F(WindowTreeAppTest, SetWindowVisibility) {
  // Create 1 and 2 in the first connection and parent both to the root.
  Id window_1_1 = ws_client1()->NewWindow(1);
  Id window_1_2 = ws_client1()->NewWindow(2);
  ASSERT_TRUE(window_1_1);
  ASSERT_TRUE(window_1_2);

  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws1(), root_window_id(), &windows);
    ASSERT_EQ(2u, windows.size());
    EXPECT_EQ(WindowParentToString(root_window_id(), kNullParentId) +
                  " visible=true drawn=true",
              windows[0].ToString2());
    EXPECT_EQ(WindowParentToString(window_1_1, root_window_id()) +
                  " visible=false drawn=false",
              windows[1].ToString2());
  }

  // Show all the windows.
  ASSERT_TRUE(ws_client1()->SetWindowVisibility(window_1_1, true));
  ASSERT_TRUE(ws_client1()->SetWindowVisibility(window_1_2, true));
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws1(), root_window_id(), &windows);
    ASSERT_EQ(2u, windows.size());
    EXPECT_EQ(WindowParentToString(root_window_id(), kNullParentId) +
                  " visible=true drawn=true",
              windows[0].ToString2());
    EXPECT_EQ(WindowParentToString(window_1_1, root_window_id()) +
                  " visible=true drawn=true",
              windows[1].ToString2());
  }

  // Hide 1.
  ASSERT_TRUE(ws_client1()->SetWindowVisibility(window_1_1, false));
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws1(), window_1_1, &windows);
    ASSERT_EQ(1u, windows.size());
    EXPECT_EQ(WindowParentToString(window_1_1, root_window_id()) +
                  " visible=false drawn=false",
              windows[0].ToString2());
  }

  // Attach 2 to 1.
  ASSERT_TRUE(ws_client1()->AddWindow(window_1_1, window_1_2));
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws1(), window_1_1, &windows);
    ASSERT_EQ(2u, windows.size());
    EXPECT_EQ(WindowParentToString(window_1_1, root_window_id()) +
                  " visible=false drawn=false",
              windows[0].ToString2());
    EXPECT_EQ(WindowParentToString(window_1_2, window_1_1) +
                  " visible=true drawn=false",
              windows[1].ToString2());
  }

  // Show 1.
  ASSERT_TRUE(ws_client1()->SetWindowVisibility(window_1_1, true));
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws1(), window_1_1, &windows);
    ASSERT_EQ(2u, windows.size());
    EXPECT_EQ(WindowParentToString(window_1_1, root_window_id()) +
                  " visible=true drawn=true",
              windows[0].ToString2());
    EXPECT_EQ(WindowParentToString(window_1_2, window_1_1) +
                  " visible=true drawn=true",
              windows[1].ToString2());
  }
}

// Test that we hear the cursor change in other connections.
TEST_F(WindowTreeAppTest, SetCursor) {
  // Get a second connection to listen in.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id window_1_1 = BuildWindowId(connection_id_1(), 1);
  changes2()->clear();

  ASSERT_TRUE(
      ws_client1()->SetPredefinedCursor(window_1_1, mojom::Cursor::IBEAM));
  ws_client2_->WaitForChangeCount(1u);

  EXPECT_EQ("CursorChanged id=" + IdToString(window_1_1) + " cursor_id=4",
            SingleChangeToDescription(*changes2()));
}

// Assertions for SetWindowVisibility sending notifications.
TEST_F(WindowTreeAppTest, SetWindowVisibilityNotifications) {
  // Create 1,1 and 1,2. 1,2 is made a child of 1,1 and 1,1 a child of the root.
  Id window_1_1 = ws_client1()->NewWindow(1);
  ASSERT_TRUE(window_1_1);
  ASSERT_TRUE(ws_client1()->SetWindowVisibility(window_1_1, true));
  Id window_1_2 = ws_client1()->NewWindow(2);
  ASSERT_TRUE(window_1_2);
  ASSERT_TRUE(ws_client1()->SetWindowVisibility(window_1_2, true));
  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));
  ASSERT_TRUE(ws_client1()->AddWindow(window_1_1, window_1_2));

  // Establish the second connection at 1,2.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnectionWithRoot(window_1_2));

  // Add 2,3 as a child of 1,2.
  Id window_2_1 = ws_client2()->NewWindow(1);
  ASSERT_TRUE(window_2_1);
  ASSERT_TRUE(ws_client2()->SetWindowVisibility(window_2_1, true));
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_2, window_2_1));
  ASSERT_TRUE(ws_client1()->WaitForAllMessages());

  changes2()->clear();
  // Hide 1,2 from connection 1. Connection 2 should see this.
  ASSERT_TRUE(ws_client1()->SetWindowVisibility(window_1_2, false));
  {
    ws_client2_->WaitForChangeCount(1);
    EXPECT_EQ(
        "VisibilityChanged window=" + IdToString(window_1_2) + " visible=false",
        SingleChangeToDescription(*changes2()));
  }

  changes1()->clear();
  // Show 1,2 from connection 2, connection 1 should be notified.
  ASSERT_TRUE(ws_client2()->SetWindowVisibility(window_1_2, true));
  {
    ws_client1_->WaitForChangeCount(1);
    EXPECT_EQ(
        "VisibilityChanged window=" + IdToString(window_1_2) + " visible=true",
        SingleChangeToDescription(*changes1()));
  }

  changes2()->clear();
  // Hide 1,1, connection 2 should be told the draw state changed.
  ASSERT_TRUE(ws_client1()->SetWindowVisibility(window_1_1, false));
  {
    ws_client2_->WaitForChangeCount(1);
    EXPECT_EQ(
        "DrawnStateChanged window=" + IdToString(window_1_2) + " drawn=false",
        SingleChangeToDescription(*changes2()));
  }

  changes2()->clear();
  // Show 1,1 from connection 1. Connection 2 should see this.
  ASSERT_TRUE(ws_client1()->SetWindowVisibility(window_1_1, true));
  {
    ws_client2_->WaitForChangeCount(1);
    EXPECT_EQ(
        "DrawnStateChanged window=" + IdToString(window_1_2) + " drawn=true",
        SingleChangeToDescription(*changes2()));
  }

  // Change visibility of 2,3, connection 1 should see this.
  changes1()->clear();
  ASSERT_TRUE(ws_client2()->SetWindowVisibility(window_2_1, false));
  {
    ws_client1_->WaitForChangeCount(1);
    EXPECT_EQ(
        "VisibilityChanged window=" + IdToString(window_2_1) + " visible=false",
        SingleChangeToDescription(*changes1()));
  }

  changes2()->clear();
  // Remove 1,1 from the root, connection 2 should see drawn state changed.
  ASSERT_TRUE(ws_client1()->RemoveWindowFromParent(window_1_1));
  {
    ws_client2_->WaitForChangeCount(1);
    EXPECT_EQ(
        "DrawnStateChanged window=" + IdToString(window_1_2) + " drawn=false",
        SingleChangeToDescription(*changes2()));
  }

  changes2()->clear();
  // Add 1,1 back to the root, connection 2 should see drawn state changed.
  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));
  {
    ws_client2_->WaitForChangeCount(1);
    EXPECT_EQ(
        "DrawnStateChanged window=" + IdToString(window_1_2) + " drawn=true",
        SingleChangeToDescription(*changes2()));
  }
}

TEST_F(WindowTreeAppTest, SetWindowProperty) {
  Id window_1_1 = ws_client1()->NewWindow(1);
  ASSERT_TRUE(window_1_1);

  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(false));
  changes2()->clear();

  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws1(), root_window_id(), &windows);
    ASSERT_EQ(2u, windows.size());
    EXPECT_EQ(root_window_id(), windows[0].window_id);
    EXPECT_EQ(window_1_1, windows[1].window_id);
    ASSERT_EQ(0u, windows[1].properties.size());
  }

  // Set properties on 1.
  changes2()->clear();
  std::vector<uint8_t> one(1, '1');
  ASSERT_TRUE(ws_client1()->SetWindowProperty(window_1_1, "one", &one));
  {
    ws_client2_->WaitForChangeCount(1);
    EXPECT_EQ(
        "PropertyChanged window=" + IdToString(window_1_1) + " key=one value=1",
        SingleChangeToDescription(*changes2()));
  }

  // Test that our properties exist in the window tree
  {
    std::vector<TestWindow> windows;
    GetWindowTree(ws1(), window_1_1, &windows);
    ASSERT_EQ(1u, windows.size());
    ASSERT_EQ(1u, windows[0].properties.size());
    EXPECT_EQ(one, windows[0].properties["one"]);
  }

  changes2()->clear();
  // Set back to null.
  ASSERT_TRUE(ws_client1()->SetWindowProperty(window_1_1, "one", NULL));
  {
    ws_client2_->WaitForChangeCount(1);
    EXPECT_EQ("PropertyChanged window=" + IdToString(window_1_1) +
                  " key=one value=NULL",
              SingleChangeToDescription(*changes2()));
  }
}

TEST_F(WindowTreeAppTest, OnEmbeddedAppDisconnected) {
  // Create connection 2 and 3.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id window_1_1 = BuildWindowId(connection_id_1(), 1);
  Id window_2_1 = ws_client2()->NewWindow(1);
  ASSERT_TRUE(window_2_1);
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_1));
  changes2()->clear();
  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(ws2(), window_2_1));

  // Connection 1 should get a hierarchy change for window_2_1.
  ws_client1_->WaitForChangeCount(1);
  changes1()->clear();

  // Close connection 3. Connection 2 (which had previously embedded 3) should
  // be notified of this.
  ws_client3_.reset();
  ws_client2_->WaitForChangeCount(1);
  EXPECT_EQ("OnEmbeddedAppDisconnected window=" + IdToString(window_2_1),
            SingleChangeToDescription(*changes2()));

  ws_client1_->WaitForChangeCount(1);
  EXPECT_EQ("OnEmbeddedAppDisconnected window=" + IdToString(window_2_1),
            SingleChangeToDescription(*changes1()));
}

// Verifies when the parent of an Embed() is destroyed the embedded app gets
// a WindowDeleted (and doesn't trigger a DCHECK).
TEST_F(WindowTreeAppTest, OnParentOfEmbedDisconnects) {
  // Create connection 2 and 3.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id window_1_1 = BuildWindowId(connection_id_1(), 1);
  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));
  Id window_2_1 = ws_client2()->NewWindow(1);
  Id window_2_2 = ws_client2()->NewWindow(2);
  ASSERT_TRUE(window_2_1);
  ASSERT_TRUE(window_2_2);
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_1));
  ASSERT_TRUE(ws_client2()->AddWindow(window_2_1, window_2_2));
  changes2()->clear();
  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(ws2(), window_2_2));
  changes3()->clear();

  // Close connection 2. Connection 3 should get a delete (for its root).
  ws_client2_.reset();
  ws_client3_->WaitForChangeCount(1);
  EXPECT_EQ("WindowDeleted window=" + IdToString(window_2_2),
            SingleChangeToDescription(*changes3()));
}

// Verifies WindowTreeImpl doesn't incorrectly erase from its internal
// map when a window from another connection with the same window_id is removed.
TEST_F(WindowTreeAppTest, DontCleanMapOnDestroy) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id window_1_1 = BuildWindowId(connection_id_1(), 1);
  ASSERT_TRUE(ws_client2()->NewWindow(1));
  changes1()->clear();
  ws_client2_.reset();
  ws_client1_->WaitForChangeCount(1);
  EXPECT_EQ("OnEmbeddedAppDisconnected window=" + IdToString(window_1_1),
            SingleChangeToDescription(*changes1()));
  std::vector<TestWindow> windows;
  GetWindowTree(ws1(), window_1_1, &windows);
  EXPECT_FALSE(windows.empty());
}

// Verifies Embed() works when supplying a WindowTreeClient.
TEST_F(WindowTreeAppTest, EmbedSupplyingWindowTreeClient) {
  ASSERT_TRUE(ws_client1()->NewWindow(1));

  TestWindowTreeClientImpl client2;
  mojom::WindowTreeClientPtr client2_ptr;
  mojo::Binding<WindowTreeClient> client2_binding(&client2, &client2_ptr);
  ASSERT_TRUE(Embed(ws1(), BuildWindowId(connection_id_1(), 1),
                    std::move(client2_ptr)));
  client2.WaitForOnEmbed();
  EXPECT_EQ("OnEmbed",
            SingleChangeToDescription(*client2.tracker()->changes()));
}

TEST_F(WindowTreeAppTest, EmbedFailsFromOtherConnection) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  Id window_1_1 = BuildWindowId(connection_id_1(), 1);
  Id window_2_1 = ws_client2()->NewWindow(1);
  ASSERT_TRUE(window_2_1);
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_1));
  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(ws2(), window_2_1));

  Id window_3_3 = ws_client3()->NewWindow(3);
  ASSERT_TRUE(window_3_3);
  ASSERT_TRUE(ws_client3()->AddWindow(window_2_1, window_3_3));

  // 2 should not be able to embed in window_3_3 as window_3_3 was not created
  // by
  // 2.
  EXPECT_FALSE(EmbedUrl(shell(), ws2(), shell_url(), window_3_3));
}

// Verifies Embed() from window manager on another connections window works.
TEST_F(WindowTreeAppTest, EmbedFromOtherConnection) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));

  Id window_1_1 = BuildWindowId(connection_id_1(), 1);
  Id window_2_1 = ws_client2()->NewWindow(1);
  ASSERT_TRUE(window_2_1);
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_1));

  changes2()->clear();

  // Establish a third connection in window_2_1.
  ASSERT_NO_FATAL_FAILURE(EstablishThirdConnection(ws1(), window_2_1));

  ASSERT_TRUE(ws_client2()->WaitForAllMessages());
  EXPECT_EQ(std::string(), SingleChangeToDescription(*changes2()));
}

TEST_F(WindowTreeAppTest, CantEmbedFromConnectionRoot) {
  // Shouldn't be able to embed into the root.
  ASSERT_FALSE(EmbedUrl(shell(), ws1(), shell_url(), root_window_id()));

  // Even though the call above failed a WindowTreeClient was obtained. We need
  // to
  // wait for it else we throw off the next connect.
  WaitForWindowTreeClient();

  // Don't allow a connection to embed into its own root.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  EXPECT_FALSE(EmbedUrl(shell(), ws2(), shell_url(),
                        BuildWindowId(connection_id_1(), 1)));

  // Need to wait for a WindowTreeClient for same reason as above.
  WaitForWindowTreeClient();

  Id window_1_2 = ws_client1()->NewWindow(2);
  ASSERT_TRUE(window_1_2);
  ASSERT_TRUE(
      ws_client1()->AddWindow(BuildWindowId(connection_id_1(), 1), window_1_2));
  ASSERT_TRUE(ws_client3_.get() == nullptr);
  ws_client3_ = EstablishConnectionViaEmbedWithPolicyBitmask(
      ws1(), window_1_2, mojom::WindowTree::kAccessPolicyEmbedRoot, nullptr);
  ASSERT_TRUE(ws_client3_.get() != nullptr);

  // window_1_2 is ws3's root, so even though v3 is an embed root it should not
  // be able to Embed into itself.
  ASSERT_FALSE(EmbedUrl(shell(), ws3(), shell_url(), window_1_2));
}

// Verifies that a transient window tracks its parent's lifetime.
TEST_F(WindowTreeAppTest, TransientWindowTracksTransientParentLifetime) {
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnection(true));
  Id window_1_1 = BuildWindowId(connection_id_1(), 1);

  Id window_2_1 = ws_client2()->NewWindow(1);
  Id window_2_2 = ws_client2()->NewWindow(2);
  Id window_2_3 = ws_client2()->NewWindow(3);
  ASSERT_TRUE(window_2_1);

  // root -> window_1_1 -> window_2_1
  // root -> window_1_1 -> window_2_2
  // root -> window_1_1 -> window_2_3
  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_1));
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_1));
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_2));
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_1, window_2_3));

  // window_2_2 and window_2_3 now track the lifetime of window_2_1.
  changes1()->clear();
  ws2()->AddTransientWindow(10, window_2_1, window_2_2);
  ws2()->AddTransientWindow(11, window_2_1, window_2_3);
  ws_client1()->WaitForChangeCount(2);
  EXPECT_EQ("AddTransientWindow parent = " + IdToString(window_2_1) +
                " child = " + IdToString(window_2_2),
            ChangesToDescription1(*changes1())[0]);
  EXPECT_EQ("AddTransientWindow parent = " + IdToString(window_2_1) +
                " child = " + IdToString(window_2_3),
            ChangesToDescription1(*changes1())[1]);

  changes1()->clear();
  ws2()->RemoveTransientWindowFromParent(12, window_2_3);
  ws_client1()->WaitForChangeCount(1);
  EXPECT_EQ("RemoveTransientWindowFromParent parent = " +
                IdToString(window_2_1) + " child = " + IdToString(window_2_3),
            SingleChangeToDescription(*changes1()));

  changes1()->clear();
  ASSERT_TRUE(ws_client2()->DeleteWindow(window_2_1));
  ws_client1()->WaitForChangeCount(2);
  EXPECT_EQ("WindowDeleted window=" + IdToString(window_2_2),
            ChangesToDescription1(*changes1())[0]);
  EXPECT_EQ("WindowDeleted window=" + IdToString(window_2_1),
            ChangesToDescription1(*changes1())[1]);
}

TEST_F(WindowTreeAppTest, Ids) {
  const Id window_1_100 = ws_client1()->NewWindow(100);
  ASSERT_TRUE(window_1_100);
  ASSERT_TRUE(ws_client1()->AddWindow(root_window_id(), window_1_100));

  // Establish the second connection at 1,100.
  ASSERT_NO_FATAL_FAILURE(EstablishSecondConnectionWithRoot(window_1_100));

  // 1,100 is the id in the ws_client1's id space. The new client should see
  // 2,1 (the server id).
  const Id window_1_100_in_ws2 = BuildWindowId(connection_id_1(), 1);
  EXPECT_EQ(window_1_100_in_ws2, ws_client2()->root_window_id());

  // The first window created in the second connection gets a server id of 2,1
  // regardless of the id the client uses.
  const Id window_2_101 = ws_client2()->NewWindow(101);
  ASSERT_TRUE(ws_client2()->AddWindow(window_1_100_in_ws2, window_2_101));
  const Id window_2_101_in_ws1 = BuildWindowId(connection_id_2(), 1);
  ws_client1()->WaitForChangeCount(1);
  EXPECT_EQ("HierarchyChanged window=" + IdToString(window_2_101_in_ws1) +
                " new_parent=" + IdToString(window_1_100) + " old_parent=null",
            SingleChangeToDescription(*changes1()));
  changes1()->clear();

  // Change the bounds of window_2_101 and make sure server gets it.
  ws2()->SetWindowBounds(11, window_2_101,
                         mojo::Rect::From(gfx::Rect(1, 2, 3, 4)));
  ASSERT_TRUE(ws_client2()->WaitForChangeCompleted(11));
  ws_client1()->WaitForChangeCount(1);
  EXPECT_EQ("BoundsChanged window=" + IdToString(window_2_101_in_ws1) +
                " old_bounds=0,0 0x0 new_bounds=1,2 3x4",
            SingleChangeToDescription(*changes1()));
  changes2()->clear();

  // Remove 2_101 from wm, client1 should see the change.
  ws1()->RemoveWindowFromParent(12, window_2_101_in_ws1);
  ASSERT_TRUE(ws_client1()->WaitForChangeCompleted(12));
  ws_client2()->WaitForChangeCount(1);
  EXPECT_EQ("HierarchyChanged window=" + IdToString(window_2_101) +
                " new_parent=null old_parent=" +
                IdToString(window_1_100_in_ws2),
            SingleChangeToDescription(*changes2()));
}

// TODO(sky): need to better track changes to initial connection. For example,
// that SetBounsdWindows/AddWindow and the like don't result in messages to the
// originating connection.

// TODO(sky): make sure coverage of what was
// WindowManagerTest.SecondEmbedRoot_InitService and
// WindowManagerTest.MultipleEmbedRootsBeforeWTHReady gets added to window
// manager
// tests.

}  // namespace ws
}  // namespace mus
