// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "components/mus/common/types.h"
#include "components/mus/common/util.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/connection_manager_delegate.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/display_manager_factory.h"
#include "components/mus/ws/ids.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_surface_manager_test_api.h"
#include "components/mus/ws/test_change_tracker.h"
#include "components/mus/ws/window_tree_host_connection.h"
#include "components/mus/ws/window_tree_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"

using mojo::Array;
using mojo::InterfaceRequest;
using mojo::String;
using mus::mojom::Event;
using mus::mojom::EventPtr;
using mus::mojom::LocationData;
using mus::mojom::PointerData;
using mus::mojom::WindowDataPtr;

namespace mus {
namespace ws {
namespace {

std::string WindowIdToString(const WindowId& id) {
  return base::StringPrintf("%d,%d", id.connection_id, id.window_id);
}

ClientWindowId BuildClientWindowId(WindowTreeImpl* tree,
                                   ConnectionSpecificId window_id) {
  return ClientWindowId(WindowIdToTransportId(WindowId(tree->id(), window_id)));
}

ClientWindowId ClientWindowIdForWindow(WindowTreeImpl* tree,
                                       const ServerWindow* window) {
  ClientWindowId client_window_id;
  // If window isn't known we'll return 0, which should then error out.
  tree->IsWindowKnown(window, &client_window_id);
  return client_window_id;
}

class TestWindowManager : public mojom::WindowManager {
 public:
  TestWindowManager() : got_create_top_level_window_(false), change_id_(0u) {}
  ~TestWindowManager() override {}

  bool did_call_create_top_level_window(uint32_t* change_id) {
    if (!got_create_top_level_window_)
      return false;

    got_create_top_level_window_ = false;
    *change_id = change_id_;
    return true;
  }

 private:
  // WindowManager:
  void WmSetBounds(uint32_t change_id,
                   uint32_t window_id,
                   mojo::RectPtr bounds) override {}
  void WmSetProperty(uint32_t change_id,
                     uint32_t window_id,
                     const mojo::String& name,
                     mojo::Array<uint8_t> value) override {}
  void WmCreateTopLevelWindow(
      uint32_t change_id,
      mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) override {
    got_create_top_level_window_ = true;
    change_id_ = change_id;
  }
  void OnAccelerator(uint32_t id, mojom::EventPtr event) override {}

  bool got_create_top_level_window_;
  uint32_t change_id_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowManager);
};

// -----------------------------------------------------------------------------

// WindowTreeClient implementation that logs all calls to a TestChangeTracker.
// TODO(sky): refactor so both this and WindowTreeAppTest share code.
class TestWindowTreeClient : public mus::mojom::WindowTreeClient {
 public:
  TestWindowTreeClient() : binding_(this), record_on_change_completed_(false) {}
  ~TestWindowTreeClient() override {}

  TestChangeTracker* tracker() { return &tracker_; }

  void Bind(mojo::InterfaceRequest<mojom::WindowTreeClient> request) {
    binding_.Bind(std::move(request));
  }

  void set_record_on_change_completed(bool value) {
    record_on_change_completed_ = value;
  }

 private:
  // WindowTreeClient:
  void OnEmbed(uint16_t connection_id,
               WindowDataPtr root,
               mus::mojom::WindowTreePtr tree,
               Id focused_window_id,
               uint32_t access_policy) override {
    // TODO(sky): add test coverage of |focused_window_id|.
    tracker_.OnEmbed(connection_id, std::move(root));
  }
  void OnEmbeddedAppDisconnected(uint32_t window) override {
    tracker_.OnEmbeddedAppDisconnected(window);
  }
  void OnUnembed(Id window_id) override { tracker_.OnUnembed(window_id); }
  void OnTopLevelCreated(uint32_t change_id,
                         mojom::WindowDataPtr data) override {
    tracker_.OnTopLevelCreated(change_id, std::move(data));
  }
  void OnWindowBoundsChanged(uint32_t window,
                             mojo::RectPtr old_bounds,
                             mojo::RectPtr new_bounds) override {
    tracker_.OnWindowBoundsChanged(window, std::move(old_bounds),
                                   std::move(new_bounds));
  }
  void OnClientAreaChanged(
      uint32_t window_id,
      mojo::InsetsPtr new_client_area,
      mojo::Array<mojo::RectPtr> new_additional_client_areas) override {}
  void OnTransientWindowAdded(uint32_t window_id,
                              uint32_t transient_window_id) override {}
  void OnTransientWindowRemoved(uint32_t window_id,
                                uint32_t transient_window_id) override {}
  void OnWindowViewportMetricsChanged(
      mojo::Array<uint32_t> window_ids,
      mojom::ViewportMetricsPtr old_metrics,
      mojom::ViewportMetricsPtr new_metrics) override {
    tracker_.OnWindowViewportMetricsChanged(std::move(old_metrics),
                                            std::move(new_metrics));
  }
  void OnWindowHierarchyChanged(uint32_t window,
                                uint32_t new_parent,
                                uint32_t old_parent,
                                Array<WindowDataPtr> windows) override {
    tracker_.OnWindowHierarchyChanged(window, new_parent, old_parent,
                                      std::move(windows));
  }
  void OnWindowReordered(uint32_t window_id,
                         uint32_t relative_window_id,
                         mojom::OrderDirection direction) override {
    tracker_.OnWindowReordered(window_id, relative_window_id, direction);
  }
  void OnWindowDeleted(uint32_t window) override {
    tracker_.OnWindowDeleted(window);
  }
  void OnWindowVisibilityChanged(uint32_t window, bool visible) override {
    tracker_.OnWindowVisibilityChanged(window, visible);
  }
  void OnWindowDrawnStateChanged(uint32_t window, bool drawn) override {
    tracker_.OnWindowDrawnStateChanged(window, drawn);
  }
  void OnWindowSharedPropertyChanged(uint32_t window,
                                     const String& name,
                                     Array<uint8_t> new_data) override {
    tracker_.OnWindowSharedPropertyChanged(window, name, std::move(new_data));
  }
  void OnWindowInputEvent(uint32_t event_id,
                          uint32_t window,
                          EventPtr event) override {
    tracker_.OnWindowInputEvent(window, std::move(event));
  }
  void OnWindowFocused(uint32_t focused_window_id) override {
    tracker_.OnWindowFocused(focused_window_id);
  }
  void OnWindowPredefinedCursorChanged(uint32_t window_id,
                                       mojom::Cursor cursor_id) override {
    tracker_.OnWindowPredefinedCursorChanged(window_id, cursor_id);
  }
  void OnChangeCompleted(uint32_t change_id, bool success) override {
    if (record_on_change_completed_)
      tracker_.OnChangeCompleted(change_id, success);
  }
  void RequestClose(uint32_t window_id) override {}
  void GetWindowManager(mojo::AssociatedInterfaceRequest<mojom::WindowManager>
                            internal) override {}

  TestChangeTracker tracker_;

  mojo::Binding<mojom::WindowTreeClient> binding_;
  bool record_on_change_completed_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeClient);
};

// -----------------------------------------------------------------------------

// ClientConnection implementation that vends TestWindowTreeClient.
class TestClientConnection : public ClientConnection {
 public:
  explicit TestClientConnection(scoped_ptr<WindowTreeImpl> service_impl)
      : ClientConnection(std::move(service_impl), &client_),
        is_paused_(false) {}

  TestWindowTreeClient* client() { return &client_; }

  bool is_paused() const { return is_paused_; }

  // ClientConnection:
  mojom::WindowManager* GetWindowManager() override {
    NOTREACHED();
    return nullptr;
  }
  void SetIncomingMethodCallProcessingPaused(bool paused) override {
    is_paused_ = paused;
  }

 private:
  ~TestClientConnection() override {}

  TestWindowTreeClient client_;
  bool is_paused_;

  DISALLOW_COPY_AND_ASSIGN(TestClientConnection);
};

// -----------------------------------------------------------------------------

// Empty implementation of ConnectionManagerDelegate.
class TestConnectionManagerDelegate : public ConnectionManagerDelegate {
 public:
  TestConnectionManagerDelegate() : last_connection_(nullptr) {}
  ~TestConnectionManagerDelegate() override {}

  TestWindowTreeClient* last_client() {
    return last_connection_ ? last_connection_->client() : nullptr;
  }

  TestClientConnection* last_connection() { return last_connection_; }

 private:
  // ConnectionManagerDelegate:
  void OnNoMoreRootConnections() override {}

  ClientConnection* CreateClientConnectionForEmbedAtWindow(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mus::mojom::WindowTree> service_request,
      ServerWindow* root,
      uint32_t policy_bitmask,
      mus::mojom::WindowTreeClientPtr client) override {
    // Used by ConnectionManager::AddRoot.
    scoped_ptr<WindowTreeImpl> service(
        new WindowTreeImpl(connection_manager, root, policy_bitmask));
    last_connection_ = new TestClientConnection(std::move(service));
    return last_connection_;
  }

  TestClientConnection* last_connection_;
  WindowTreeHostImpl* window_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectionManagerDelegate);
};

// -----------------------------------------------------------------------------

class TestWindowTreeHostConnection : public WindowTreeHostConnection {
 public:
  TestWindowTreeHostConnection(scoped_ptr<WindowTreeHostImpl> host_impl,
                               ConnectionManager* manager)
      : WindowTreeHostConnection(std::move(host_impl), manager) {}
  ~TestWindowTreeHostConnection() override {}

 private:
  // WindowTreeHostDelegate:
  void OnDisplayInitialized() override {
    connection_manager()->AddHost(this);
    set_window_tree(connection_manager()->EmbedAtWindow(
        window_tree_host()->root_window(),
        mus::mojom::WindowTree::kAccessPolicyEmbedRoot,
        mus::mojom::WindowTreeClientPtr()));
  }
  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeHostConnection);
};

// -----------------------------------------------------------------------------
// Empty implementation of DisplayManager.
class TestDisplayManager : public DisplayManager {
 public:
  explicit TestDisplayManager(int32_t* cursor_id_storage)
      : cursor_id_storage_(cursor_id_storage) {}
  ~TestDisplayManager() override {}

  // DisplayManager:
  void Init(DisplayManagerDelegate* delegate) override {
    // It is necessary to tell the delegate about the ViewportMetrics to make
    // sure that the WindowTreeHostConnection is correctly initialized (and a
    // root-window is created).
    mojom::ViewportMetrics metrics;
    metrics.size_in_pixels = mojo::Size::From(gfx::Size(400, 300));
    metrics.device_pixel_ratio = 1.f;
    delegate->OnViewportMetricsChanged(mojom::ViewportMetrics(), metrics);
  }
  void SchedulePaint(const ServerWindow* window,
                     const gfx::Rect& bounds) override {}
  void SetViewportSize(const gfx::Size& size) override {}
  void SetTitle(const base::string16& title) override {}
  void SetCursorById(int32_t cursor) override { *cursor_id_storage_ = cursor; }
  mojom::Rotation GetRotation() override { return mojom::Rotation::VALUE_0; }
  const mojom::ViewportMetrics& GetViewportMetrics() override {
    return display_metrices_;
  }
  void UpdateTextInputState(const ui::TextInputState& state) override {}
  void SetImeVisibility(bool visible) override {}
  bool IsFramePending() const override { return false; }

 private:
  mojom::ViewportMetrics display_metrices_;

  int32_t* cursor_id_storage_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayManager);
};

// Factory that dispenses TestDisplayManagers.
class TestDisplayManagerFactory : public DisplayManagerFactory {
 public:
  explicit TestDisplayManagerFactory(int32_t* cursor_id_storage)
      : cursor_id_storage_(cursor_id_storage) {}
  ~TestDisplayManagerFactory() {}
  DisplayManager* CreateDisplayManager(
      mojo::Shell* shell,
      const scoped_refptr<GpuState>& gpu_state,
      const scoped_refptr<mus::SurfacesState>& surfaces_state) override {
    return new TestDisplayManager(cursor_id_storage_);
  }

 private:
  int32_t* cursor_id_storage_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayManagerFactory);
};

ui::TouchEvent CreatePointerDownEvent(int x, int y) {
  return ui::TouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(x, y), 1,
                        ui::EventTimeForNow());
}

ui::TouchEvent CreatePointerUpEvent(int x, int y) {
  return ui::TouchEvent(ui::ET_TOUCH_RELEASED, gfx::Point(x, y), 1,
                        ui::EventTimeForNow());
}

ui::MouseEvent CreateMouseMoveEvent(int x, int y) {
  return ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(x, y), gfx::Point(x, y),
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
}

ui::MouseEvent CreateMouseDownEvent(int x, int y) {
  return ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(x, y),
                        gfx::Point(x, y), ui::EventTimeForNow(),
                        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
}

ui::MouseEvent CreateMouseUpEvent(int x, int y) {
  return ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(x, y),
                        gfx::Point(x, y), ui::EventTimeForNow(),
                        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
}

const ServerWindow* FirstRoot(WindowTreeImpl* connection) {
  return connection->roots().size() == 1u ? *(connection->roots().begin())
                                          : nullptr;
}

ClientWindowId FirstRootId(WindowTreeImpl* connection) {
  return connection->roots().size() == 1u
             ? ClientWindowIdForWindow(connection,
                                       *(connection->roots().begin()))
             : ClientWindowId();
}

}  // namespace

// -----------------------------------------------------------------------------

class WindowTreeTest : public testing::Test {
 public:
  WindowTreeTest()
      : wm_client_(nullptr),
        cursor_id_(0),
        display_manager_factory_(&cursor_id_) {}
  ~WindowTreeTest() override {}

  // WindowTreeImpl for the window manager.
  WindowTreeImpl* wm_connection() {
    return connection_manager_->GetConnection(1);
  }

  TestWindowTreeClient* last_window_tree_client() {
    return delegate_.last_client();
  }

  TestClientConnection* last_client_connection() {
    return delegate_.last_connection();
  }

  ConnectionManager* connection_manager() { return connection_manager_.get(); }

  ServerWindow* GetWindowById(const WindowId& id) {
    return connection_manager_->GetWindow(id);
  }

  TestWindowTreeClient* wm_client() { return wm_client_; }
  mus::mojom::Cursor cursor_id() {
    return static_cast<mus::mojom::Cursor>(cursor_id_);
  }

  TestWindowTreeHostConnection* host_connection() { return host_connection_; }

  void DispatchEventWithoutAck(const ui::Event& event) {
    host_connection()->window_tree_host()->OnEvent(event);
  }

  void set_window_manager_internal(WindowTreeImpl* connection,
                                   mojom::WindowManager* wm_internal) {
    connection->window_manager_internal_ = wm_internal;
  }

  void AckPreviousEvent() {
    while (host_connection()->window_tree_host()->tree_awaiting_input_ack_) {
      host_connection()
          ->window_tree_host()
          ->tree_awaiting_input_ack_->OnWindowInputEventAck(0);
    }
  }

  void DispatchEventAndAckImmediately(const ui::Event& event) {
    DispatchEventWithoutAck(event);
    AckPreviousEvent();
  }

  // Creates a new window from wm_connection() and embeds a new connection in
  // it.
  void SetupEventTargeting(TestWindowTreeClient** out_client,
                           WindowTreeImpl** window_tree_connection,
                           ServerWindow** window);

 protected:
  // testing::Test:
  void SetUp() override {
    DisplayManager::set_factory_for_testing(&display_manager_factory_);
    // TODO(fsamuel): This is probably broken. We need a root.
    connection_manager_.reset(
        new ConnectionManager(&delegate_, scoped_refptr<SurfacesState>()));
    WindowTreeHostImpl* host = new WindowTreeHostImpl(
        connection_manager_.get(), nullptr, scoped_refptr<GpuState>(),
        scoped_refptr<mus::SurfacesState>());
    // TODO(fsamuel): This is way too magical. We need to find a better way to
    // manage lifetime.
    host_connection_ = new TestWindowTreeHostConnection(
        make_scoped_ptr(host), connection_manager_.get());
    host->Init(host_connection_);
    wm_client_ = delegate_.last_client();
  }

 private:
  // TestWindowTreeClient that is used for the WM connection.
  TestWindowTreeClient* wm_client_;
  int32_t cursor_id_;
  TestDisplayManagerFactory display_manager_factory_;
  TestConnectionManagerDelegate delegate_;
  TestWindowTreeHostConnection* host_connection_;
  scoped_ptr<ConnectionManager> connection_manager_;
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeTest);
};

// Creates a new window in wm_connection(), adds it to the root, embeds a
// new client in the window and creates a child of said window. |window| is
// set to the child of |window_tree_connection| that is created.
void WindowTreeTest::SetupEventTargeting(
    TestWindowTreeClient** out_client,
    WindowTreeImpl** window_tree_connection,
    ServerWindow** window) {
  const ClientWindowId embed_window_id =
      BuildClientWindowId(wm_connection(), 1);
  EXPECT_TRUE(
      wm_connection()->NewWindow(embed_window_id, ServerWindow::Properties()));
  EXPECT_TRUE(wm_connection()->SetWindowVisibility(embed_window_id, true));
  EXPECT_TRUE(wm_connection()->AddWindow(FirstRootId(wm_connection()),
                                         embed_window_id));
  host_connection()->window_tree_host()->root_window()->SetBounds(
      gfx::Rect(0, 0, 100, 100));
  mojom::WindowTreeClientPtr client;
  mojo::InterfaceRequest<mojom::WindowTreeClient> client_request =
      GetProxy(&client);
  wm_client()->Bind(std::move(client_request));
  ConnectionSpecificId connection_id = 0;
  wm_connection()->Embed(embed_window_id, std::move(client),
                         mojom::WindowTree::kAccessPolicyDefault,
                         &connection_id);
  ServerWindow* embed_window =
      wm_connection()->GetWindowByClientId(embed_window_id);
  WindowTreeImpl* connection1 =
      connection_manager()->GetConnectionWithRoot(embed_window);
  ASSERT_TRUE(connection1 != nullptr);
  ASSERT_NE(connection1, wm_connection());

  embed_window->SetBounds(gfx::Rect(0, 0, 50, 50));

  const ClientWindowId child1_id(BuildClientWindowId(connection1, 1));
  EXPECT_TRUE(connection1->NewWindow(child1_id, ServerWindow::Properties()));
  ServerWindow* child1 = connection1->GetWindowByClientId(child1_id);
  ASSERT_TRUE(child1);
  EXPECT_TRUE(connection1->AddWindow(
      ClientWindowIdForWindow(connection1, embed_window), child1_id));
  connection1->GetHost(embed_window)->AddActivationParent(embed_window);

  child1->SetVisible(true);
  child1->SetBounds(gfx::Rect(20, 20, 20, 20));
  EnableHitTest(child1);

  TestWindowTreeClient* embed_connection = last_window_tree_client();
  embed_connection->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  *out_client = embed_connection;
  *window_tree_connection = connection1;
  *window = child1;
}

// Verifies focus correctly changes on pointer events.
TEST_F(WindowTreeTest, FocusOnPointer) {
  const ClientWindowId embed_window_id =
      BuildClientWindowId(wm_connection(), 1);
  EXPECT_TRUE(
      wm_connection()->NewWindow(embed_window_id, ServerWindow::Properties()));
  ServerWindow* embed_window =
      wm_connection()->GetWindowByClientId(embed_window_id);
  ASSERT_TRUE(embed_window);
  EXPECT_TRUE(wm_connection()->SetWindowVisibility(embed_window_id, true));
  ASSERT_TRUE(FirstRoot(wm_connection()));
  const ClientWindowId wm_root_id = FirstRootId(wm_connection());
  EXPECT_TRUE(wm_connection()->AddWindow(wm_root_id, embed_window_id));
  host_connection()->window_tree_host()->root_window()->SetBounds(
      gfx::Rect(0, 0, 100, 100));
  mojom::WindowTreeClientPtr client;
  mojo::InterfaceRequest<mojom::WindowTreeClient> client_request =
      GetProxy(&client);
  wm_client()->Bind(std::move(client_request));
  ConnectionSpecificId connection_id = 0;
  wm_connection()->Embed(embed_window_id, std::move(client),
                         mojom::WindowTree::kAccessPolicyDefault,
                         &connection_id);
  WindowTreeImpl* connection1 =
      connection_manager()->GetConnectionWithRoot(embed_window);
  ASSERT_TRUE(connection1 != nullptr);
  ASSERT_NE(connection1, wm_connection());

  embed_window->SetBounds(gfx::Rect(0, 0, 50, 50));

  const ClientWindowId child1_id(BuildClientWindowId(connection1, 1));
  EXPECT_TRUE(connection1->NewWindow(child1_id, ServerWindow::Properties()));
  EXPECT_TRUE(connection1->AddWindow(
      ClientWindowIdForWindow(connection1, embed_window), child1_id));
  ServerWindow* child1 = connection1->GetWindowByClientId(child1_id);
  ASSERT_TRUE(child1);
  child1->SetVisible(true);
  child1->SetBounds(gfx::Rect(20, 20, 20, 20));
  EnableHitTest(child1);

  TestWindowTreeClient* connection1_client = last_window_tree_client();
  connection1_client->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  // Focus should not go to |child1| yet, since the parent still doesn't allow
  // active children.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(21, 22));
  WindowTreeHostImpl* host1 = connection1->GetHost(embed_window);
  EXPECT_EQ(nullptr, host1->GetFocusedWindow());
  DispatchEventAndAckImmediately(CreatePointerUpEvent(21, 22));
  connection1_client->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  host1->AddActivationParent(embed_window);

  // Focus should go to child1. This result in notifying both the window
  // manager and client connection being notified.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(21, 22));
  EXPECT_EQ(child1, host1->GetFocusedWindow());
  ASSERT_GE(wm_client()->tracker()->changes()->size(), 1u);
  EXPECT_EQ("Focused id=2,1",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  ASSERT_GE(connection1_client->tracker()->changes()->size(), 1u);
  EXPECT_EQ(
      "Focused id=2,1",
      ChangesToDescription1(*connection1_client->tracker()->changes())[0]);

  DispatchEventAndAckImmediately(CreatePointerUpEvent(21, 22));
  wm_client()->tracker()->changes()->clear();
  connection1_client->tracker()->changes()->clear();

  // Press outside of the embedded window. Note that root cannot be focused
  // (because it cannot be activated). So the focus would not move in this case.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(61, 22));
  EXPECT_EQ(child1, host_connection()->window_tree_host()->GetFocusedWindow());

  DispatchEventAndAckImmediately(CreatePointerUpEvent(21, 22));
  wm_client()->tracker()->changes()->clear();
  connection1_client->tracker()->changes()->clear();

  // Press in the same location. Should not get a focus change event (only input
  // event).
  DispatchEventAndAckImmediately(CreatePointerDownEvent(61, 22));
  EXPECT_EQ(child1, host_connection()->window_tree_host()->GetFocusedWindow());
  ASSERT_EQ(wm_client()->tracker()->changes()->size(), 1u)
      << SingleChangeToDescription(*wm_client()->tracker()->changes());
  EXPECT_EQ("InputEvent window=0,2 event_action=4",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  EXPECT_TRUE(connection1_client->tracker()->changes()->empty());
}

TEST_F(WindowTreeTest, BasicInputEventTarget) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));

  // Send an event to |v1|. |embed_connection| should get the event, not
  // |wm_client|, since |v1| lives inside an embedded window.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(21, 22));
  ASSERT_EQ(1u, wm_client()->tracker()->changes()->size());
  EXPECT_EQ("Focused id=2,1",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  ASSERT_EQ(2u, embed_connection->tracker()->changes()->size());
  EXPECT_EQ("Focused id=2,1",
            ChangesToDescription1(*embed_connection->tracker()->changes())[0]);
  EXPECT_EQ("InputEvent window=2,1 event_action=4",
            ChangesToDescription1(*embed_connection->tracker()->changes())[1]);
}

TEST_F(WindowTreeTest, CursorChangesWhenMouseOverWindowAndWindowSetsCursor) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));

  // Like in BasicInputEventTarget, we send a pointer down event to be
  // dispatched. This is only to place the mouse cursor over that window though.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(21, 22));

  window->SetPredefinedCursor(mojom::Cursor::IBEAM);

  // Because the cursor is over the window when SetCursor was called, we should
  // have immediately changed the cursor.
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());
}

TEST_F(WindowTreeTest, CursorChangesWhenEnteringWindowWithDifferentCursor) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));

  // Let's create a pointer event outside the window and then move the pointer
  // inside.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(5, 5));
  window->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());

  DispatchEventAndAckImmediately(CreateMouseMoveEvent(21, 22));
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());
}

TEST_F(WindowTreeTest, TouchesDontChangeCursor) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));

  // Let's create a pointer event outside the window and then move the pointer
  // inside.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(5, 5));
  window->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());

  // With a touch event, we shouldn't update the cursor.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(21, 22));
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());
}

TEST_F(WindowTreeTest, DragOutsideWindow) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));

  // Start with the cursor outside the window. Setting the cursor shouldn't
  // change the cursor.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(5, 5));
  window->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());

  // Move the pointer to the inside of the window
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(21, 22));
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());

  // Start the drag.
  DispatchEventAndAckImmediately(CreateMouseDownEvent(21, 22));
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());

  // Move the cursor (mouse is still down) outside the window.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(5, 5));
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());

  // Release the cursor. We should now adapt the cursor of the window
  // underneath the pointer.
  DispatchEventAndAckImmediately(CreateMouseUpEvent(5, 5));
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());
}

TEST_F(WindowTreeTest, ChangingWindowBoundsChangesCursor) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  EXPECT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));

  // Put the cursor just outside the bounds of the window.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(41, 41));
  window->SetPredefinedCursor(mojom::Cursor::IBEAM);
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());

  // Expand the bounds of the window so they now include where the cursor now
  // is.
  window->SetBounds(gfx::Rect(20, 20, 25, 25));
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());

  // Contract the bounds again.
  window->SetBounds(gfx::Rect(20, 20, 20, 20));
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());
}

TEST_F(WindowTreeTest, WindowReorderingChangesCursor) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window1 = nullptr;
  EXPECT_NO_FATAL_FAILURE(SetupEventTargeting(
      &embed_connection, &window_tree_connection, &window1));

  // Create a second window right over the first.
  const ClientWindowId embed_window_id(FirstRootId(window_tree_connection));
  const ClientWindowId child2_id(
      BuildClientWindowId(window_tree_connection, 2));
  EXPECT_TRUE(
      window_tree_connection->NewWindow(child2_id, ServerWindow::Properties()));
  ServerWindow* child2 = window_tree_connection->GetWindowByClientId(child2_id);
  ASSERT_TRUE(child2);
  EXPECT_TRUE(window_tree_connection->AddWindow(embed_window_id, child2_id));
  child2->SetVisible(true);
  child2->SetBounds(gfx::Rect(20, 20, 20, 20));
  EnableHitTest(child2);

  // Give each window a different cursor.
  window1->SetPredefinedCursor(mojom::Cursor::IBEAM);
  child2->SetPredefinedCursor(mojom::Cursor::HAND);

  // We expect window2 to be over window1 now.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(22, 22));
  EXPECT_EQ(mojom::Cursor::HAND, cursor_id());

  // But when we put window2 at the bottom, we should adapt window1's cursor.
  child2->parent()->StackChildAtBottom(child2);
  EXPECT_EQ(mojom::Cursor::IBEAM, cursor_id());
}

TEST_F(WindowTreeTest, EventAck) {
  const ClientWindowId embed_window_id =
      BuildClientWindowId(wm_connection(), 1);
  EXPECT_TRUE(
      wm_connection()->NewWindow(embed_window_id, ServerWindow::Properties()));
  EXPECT_TRUE(wm_connection()->SetWindowVisibility(embed_window_id, true));
  ASSERT_TRUE(FirstRoot(wm_connection()));
  EXPECT_TRUE(wm_connection()->AddWindow(FirstRootId(wm_connection()),
                                         embed_window_id));
  host_connection()->window_tree_host()->root_window()->SetBounds(
      gfx::Rect(0, 0, 100, 100));

  wm_client()->tracker()->changes()->clear();
  DispatchEventWithoutAck(CreateMouseMoveEvent(21, 22));
  ASSERT_EQ(1u, wm_client()->tracker()->changes()->size());
  EXPECT_EQ("InputEvent window=0,2 event_action=5",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  wm_client()->tracker()->changes()->clear();

  // Send another event. This event shouldn't reach the client.
  DispatchEventWithoutAck(CreateMouseMoveEvent(21, 22));
  ASSERT_EQ(0u, wm_client()->tracker()->changes()->size());

  // Ack the first event. That should trigger the dispatch of the second event.
  AckPreviousEvent();
  ASSERT_EQ(1u, wm_client()->tracker()->changes()->size());
  EXPECT_EQ("InputEvent window=0,2 event_action=5",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
}

// Establish connection, call NewTopLevelWindow(), make sure get id, and make
// sure client paused.
TEST_F(WindowTreeTest, NewTopLevelWindow) {
  TestWindowManager wm_internal;
  set_window_manager_internal(wm_connection(), &wm_internal);
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTreeImpl* window_tree_connection = nullptr;
  ServerWindow* window = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      SetupEventTargeting(&embed_connection, &window_tree_connection, &window));
  embed_connection->tracker()->changes()->clear();
  embed_connection->set_record_on_change_completed(true);

  // Create a new top level window.
  mojo::Map<mojo::String, mojo::Array<uint8_t>> properties;
  properties.mark_non_null();
  const uint32_t initial_change_id = 17;
  // Explicitly use an id that does not contain the connection id.
  const ClientWindowId embed_window_id2_in_child(45 << 16 | 27);
  static_cast<mojom::WindowTree*>(window_tree_connection)
      ->NewTopLevelWindow(initial_change_id, embed_window_id2_in_child.id,
                          std::move(properties));

  // The binding should be paused until the wm acks the change.
  uint32_t wm_change_id = 0u;
  ASSERT_TRUE(wm_internal.did_call_create_top_level_window(&wm_change_id));
  EXPECT_TRUE(last_client_connection()->is_paused());

  // Create the window for |embed_window_id2_in_child|.
  const ClientWindowId embed_window_id2 =
      BuildClientWindowId(wm_connection(), 2);
  EXPECT_TRUE(
      wm_connection()->NewWindow(embed_window_id2, ServerWindow::Properties()));
  EXPECT_TRUE(wm_connection()->AddWindow(FirstRootId(wm_connection()),
                                         embed_window_id2));

  // Ack the change, which should resume the binding.
  static_cast<mojom::WindowManagerClient*>(wm_connection())
      ->OnWmCreatedTopLevelWindow(wm_change_id, embed_window_id2.id);
  EXPECT_FALSE(last_client_connection()->is_paused());
  EXPECT_EQ("TopLevelCreated id=17 window_id=" +
                WindowIdToString(
                    WindowIdFromTransportId(embed_window_id2_in_child.id)),
            SingleChangeToDescription(*embed_connection->tracker()->changes()));
  embed_connection->tracker()->changes()->clear();

  // Change the visibility of the window from the owner and make sure the
  // client sees the right id.
  ServerWindow* embed_window =
      wm_connection()->GetWindowByClientId(embed_window_id2);
  ASSERT_TRUE(embed_window);
  EXPECT_FALSE(embed_window->visible());
  ASSERT_TRUE(wm_connection()->SetWindowVisibility(
      ClientWindowIdForWindow(wm_connection(), embed_window), true));
  EXPECT_TRUE(embed_window->visible());
  EXPECT_EQ("VisibilityChanged window=" +
                WindowIdToString(
                    WindowIdFromTransportId(embed_window_id2_in_child.id)) +
                " visible=true",
            SingleChangeToDescription(*embed_connection->tracker()->changes()));

  // Set the visibility from the child using the client assigned id.
  ASSERT_TRUE(window_tree_connection->SetWindowVisibility(
      embed_window_id2_in_child, false));
  EXPECT_FALSE(embed_window->visible());
}

}  // namespace ws
}  // namespace mus
