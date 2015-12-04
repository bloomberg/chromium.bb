// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
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
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using mojo::Array;
using mojo::InterfaceRequest;
using mojo::String;
using mus::mojom::ERROR_CODE_NONE;
using mus::mojom::Event;
using mus::mojom::EventPtr;
using mus::mojom::LocationData;
using mus::mojom::PointerData;
using mus::mojom::WindowDataPtr;

namespace mus {

namespace ws {
namespace {

// -----------------------------------------------------------------------------

// WindowTreeClient implementation that logs all calls to a TestChangeTracker.
// TODO(sky): refactor so both this and WindowTreeAppTest share code.
class TestWindowTreeClient : public mus::mojom::WindowTreeClient {
 public:
  TestWindowTreeClient() : binding_(this) {}
  ~TestWindowTreeClient() override {}

  TestChangeTracker* tracker() { return &tracker_; }

  void Bind(mojo::InterfaceRequest<mojom::WindowTreeClient> request) {
    binding_.Bind(request.Pass());
  }

 private:
  // WindowTreeClient:
  void OnEmbed(uint16_t connection_id,
               WindowDataPtr root,
               mus::mojom::WindowTreePtr tree,
               Id focused_window_id,
               uint32_t access_policy) override {
    // TODO(sky): add test coverage of |focused_window_id|.
    tracker_.OnEmbed(connection_id, root.Pass());
  }
  void OnEmbeddedAppDisconnected(uint32_t window) override {
    tracker_.OnEmbeddedAppDisconnected(window);
  }
  void OnUnembed() override { tracker_.OnUnembed(); }
  void OnWindowBoundsChanged(uint32_t window,
                             mojo::RectPtr old_bounds,
                             mojo::RectPtr new_bounds) override {
    tracker_.OnWindowBoundsChanged(window, old_bounds.Pass(),
                                   new_bounds.Pass());
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
      mojom::ViewportMetricsPtr old_metrics,
      mojom::ViewportMetricsPtr new_metrics) override {
    tracker_.OnWindowViewportMetricsChanged(old_metrics.Pass(),
                                            new_metrics.Pass());
  }
  void OnWindowHierarchyChanged(uint32_t window,
                                uint32_t new_parent,
                                uint32_t old_parent,
                                Array<WindowDataPtr> windows) override {
    tracker_.OnWindowHierarchyChanged(window, new_parent, old_parent,
                                      windows.Pass());
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
    tracker_.OnWindowSharedPropertyChanged(window, name, new_data.Pass());
  }
  void OnWindowInputEvent(uint32_t event_id,
                          uint32_t window,
                          EventPtr event) override {
    tracker_.OnWindowInputEvent(window, event.Pass());
  }
  void OnWindowFocused(uint32_t focused_window_id) override {
    tracker_.OnWindowFocused(focused_window_id);
  }
  void OnWindowPredefinedCursorChanged(uint32 window_id,
                                       mojom::Cursor cursor_id) override {
    tracker_.OnWindowPredefinedCursorChanged(window_id, cursor_id);
  }
  void OnChangeCompleted(uint32_t change_id, bool success) override {}
  void WmSetBounds(uint32_t change_id,
                   Id window_id,
                   mojo::RectPtr bounds) override {}
  void WmSetProperty(uint32_t change_id,
                     Id window_id,
                     const mojo::String& name,
                     mojo::Array<uint8_t> transit_data) override {}

  TestChangeTracker tracker_;

  mojo::Binding<mojom::WindowTreeClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeClient);
};

// -----------------------------------------------------------------------------

// ClientConnection implementation that vends TestWindowTreeClient.
class TestClientConnection : public ClientConnection {
 public:
  explicit TestClientConnection(scoped_ptr<WindowTreeImpl> service_impl)
      : ClientConnection(service_impl.Pass(), &client_) {}

  TestWindowTreeClient* client() { return &client_; }

 private:
  ~TestClientConnection() override {}

  TestWindowTreeClient client_;

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
      ConnectionSpecificId creator_id,
      const WindowId& root_id,
      uint32_t policy_bitmask,
      mus::mojom::WindowTreeClientPtr client) override {
    // Used by ConnectionManager::AddRoot.
    scoped_ptr<WindowTreeImpl> service(new WindowTreeImpl(
        connection_manager, creator_id, root_id, policy_bitmask));
    last_connection_ = new TestClientConnection(service.Pass());
    return last_connection_;
  }

  TestClientConnection* last_connection_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectionManagerDelegate);
};

// -----------------------------------------------------------------------------

class TestWindowTreeHostConnection : public WindowTreeHostConnection {
 public:
  TestWindowTreeHostConnection(scoped_ptr<WindowTreeHostImpl> host_impl,
                               ConnectionManager* manager)
      : WindowTreeHostConnection(host_impl.Pass(), manager) {}
  ~TestWindowTreeHostConnection() override {}

 private:
  // WindowTreeHostDelegate:
  void OnDisplayInitialized() override {
    connection_manager()->AddHost(this);
    set_window_tree(connection_manager()->EmbedAtWindow(
        kInvalidConnectionId, window_tree_host()->root_window()->id(),
        mus::mojom::WindowTree::ACCESS_POLICY_EMBED_ROOT,
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
      mojo::ApplicationImpl* app_impl,
      const scoped_refptr<GpuState>& gpu_state,
      const scoped_refptr<mus::SurfacesState>& surfaces_state) override {
    return new TestDisplayManager(cursor_id_storage_);
  }

 private:
  int32_t* cursor_id_storage_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayManagerFactory);
};

EventPtr CreatePointerDownEvent(int x, int y) {
  EventPtr event(Event::New());
  event->action = mus::mojom::EVENT_TYPE_POINTER_DOWN;
  event->pointer_data = PointerData::New();
  event->pointer_data->pointer_id = 1u;
  event->pointer_data->location = LocationData::New();
  event->pointer_data->location->x = x;
  event->pointer_data->location->y = y;
  event->pointer_data->kind = mojom::POINTER_KIND_TOUCH;
  return event.Pass();
}

EventPtr CreatePointerUpEvent(int x, int y) {
  EventPtr event(Event::New());
  event->action = mus::mojom::EVENT_TYPE_POINTER_UP;
  event->pointer_data = PointerData::New();
  event->pointer_data->pointer_id = 1u;
  event->pointer_data->location = LocationData::New();
  event->pointer_data->location->x = x;
  event->pointer_data->location->y = y;
  event->pointer_data->kind = mojom::POINTER_KIND_TOUCH;
  return event.Pass();
}

EventPtr CreateMouseMoveEvent(int x, int y) {
  EventPtr event(Event::New());
  event->action = mus::mojom::EVENT_TYPE_POINTER_MOVE;
  event->pointer_data = PointerData::New();
  event->pointer_data->pointer_id = std::numeric_limits<uint32_t>::max();
  event->pointer_data->location = LocationData::New();
  event->pointer_data->location->x = x;
  event->pointer_data->location->y = y;
  event->pointer_data->kind = mojom::POINTER_KIND_MOUSE;
  return event.Pass();
}

EventPtr CreateMouseDownEvent(int x, int y) {
  EventPtr event = CreatePointerDownEvent(x, y);
  event->flags = static_cast<mus::mojom::EventFlags>(
      event->flags | mojom::EVENT_FLAGS_LEFT_MOUSE_BUTTON);
  event->pointer_data->kind = mojom::POINTER_KIND_MOUSE;
  return event.Pass();
}

EventPtr CreateMouseUpEvent(int x, int y) {
  EventPtr event = CreatePointerUpEvent(x, y);
  event->flags = static_cast<mus::mojom::EventFlags>(
      event->flags | mojom::EVENT_FLAGS_LEFT_MOUSE_BUTTON);
  event->pointer_data->kind = mojom::POINTER_KIND_MOUSE;
  return event.Pass();
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

  TestWindowTreeClient* wm_client() { return wm_client_; }
  int32_t cursor_id() { return cursor_id_; }

  TestWindowTreeHostConnection* host_connection() { return host_connection_; }

  void DispatchEventWithoutAck(mojom::EventPtr event) {
    host_connection()->window_tree_host()->OnEvent(event.Pass());
  }

  void AckPreviousEvent() {
    host_connection()
        ->window_tree_host()
        ->tree_awaiting_input_ack_->OnWindowInputEventAck(0);
  }

  void DispatchEventAndAckImmediately(mojom::EventPtr event) {
    DispatchEventWithoutAck(event.Pass());
    AckPreviousEvent();
  }

  // Embeds a child window to the root window. Shared setup between several of
  // the unit tests.
  void SetupEventTargeting(TestWindowTreeClient** out_client,
                           ServerWindow** out_window);

 protected:
  // testing::Test:
  void SetUp() override {
    DisplayManager::set_factory_for_testing(&display_manager_factory_);
    // TODO(fsamuel): This is probably broken. We need a root.
    connection_manager_.reset(
        new ConnectionManager(&delegate_, scoped_refptr<SurfacesState>()));
    WindowTreeHostImpl* host = new WindowTreeHostImpl(
        mus::mojom::WindowTreeHostClientPtr(), connection_manager_.get(),
        nullptr, scoped_refptr<GpuState>(), scoped_refptr<mus::SurfacesState>(),
        nullptr);
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

void WindowTreeTest::SetupEventTargeting(TestWindowTreeClient** out_client,
                                         ServerWindow** out_window) {
  const WindowId embed_window_id(wm_connection()->id(), 1);
  EXPECT_TRUE(
      wm_connection()->NewWindow(embed_window_id, ServerWindow::Properties()));
  EXPECT_TRUE(wm_connection()->SetWindowVisibility(embed_window_id, true));
  EXPECT_TRUE(
      wm_connection()->AddWindow(*(wm_connection()->root()), embed_window_id));
  host_connection()->window_tree_host()->root_window()->SetBounds(
      gfx::Rect(0, 0, 100, 100));
  mojom::WindowTreeClientPtr client;
  mojo::InterfaceRequest<mojom::WindowTreeClient> client_request =
      GetProxy(&client);
  wm_client()->Bind(client_request.Pass());
  ConnectionSpecificId connection_id = 0;
  wm_connection()->Embed(embed_window_id, client.Pass(),
                         mojom::WindowTree::ACCESS_POLICY_DEFAULT,
                         &connection_id);
  WindowTreeImpl* connection1 =
      connection_manager()->GetConnectionWithRoot(embed_window_id);
  ASSERT_TRUE(connection1 != nullptr);
  ASSERT_NE(connection1, wm_connection());

  connection_manager()
      ->GetWindow(embed_window_id)
      ->SetBounds(gfx::Rect(0, 0, 50, 50));

  const WindowId child1(connection1->id(), 1);
  EXPECT_TRUE(connection1->NewWindow(child1, ServerWindow::Properties()));
  EXPECT_TRUE(connection1->AddWindow(embed_window_id, child1));
  connection1->GetHost()->AddActivationParent(
      WindowIdToTransportId(embed_window_id));

  ServerWindow* v1 = connection1->GetWindow(child1);
  v1->SetVisible(true);
  v1->SetBounds(gfx::Rect(20, 20, 20, 20));
  EnableHitTest(v1);

  TestWindowTreeClient* embed_connection = last_window_tree_client();
  embed_connection->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  *out_client = embed_connection;
  *out_window = v1;
}

// Verifies focus correctly changes on pointer events.
TEST_F(WindowTreeTest, FocusOnPointer) {
  const WindowId embed_window_id(wm_connection()->id(), 1);
  EXPECT_TRUE(
      wm_connection()->NewWindow(embed_window_id, ServerWindow::Properties()));
  EXPECT_TRUE(wm_connection()->SetWindowVisibility(embed_window_id, true));
  EXPECT_TRUE(
      wm_connection()->AddWindow(*(wm_connection()->root()), embed_window_id));
  host_connection()->window_tree_host()->root_window()->SetBounds(
      gfx::Rect(0, 0, 100, 100));
  mojom::WindowTreeClientPtr client;
  mojo::InterfaceRequest<mojom::WindowTreeClient> client_request =
      GetProxy(&client);
  wm_client()->Bind(client_request.Pass());
  ConnectionSpecificId connection_id = 0;
  wm_connection()->Embed(embed_window_id,
                         client.Pass(),
                         mojom::WindowTree::ACCESS_POLICY_DEFAULT,
                         &connection_id);
  WindowTreeImpl* connection1 =
      connection_manager()->GetConnectionWithRoot(embed_window_id);
  ASSERT_TRUE(connection1 != nullptr);
  ASSERT_NE(connection1, wm_connection());

  connection_manager()
      ->GetWindow(embed_window_id)
      ->SetBounds(gfx::Rect(0, 0, 50, 50));

  const WindowId child1(connection1->id(), 1);
  EXPECT_TRUE(connection1->NewWindow(child1, ServerWindow::Properties()));
  EXPECT_TRUE(connection1->AddWindow(embed_window_id, child1));
  ServerWindow* v1 = connection1->GetWindow(child1);
  v1->SetVisible(true);
  v1->SetBounds(gfx::Rect(20, 20, 20, 20));
  EnableHitTest(v1);

  TestWindowTreeClient* connection1_client = last_window_tree_client();
  connection1_client->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  // Focus should not go to |child1| yet, since the parent still doesn't allow
  // active children.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(21, 22));
  EXPECT_EQ(nullptr, connection1->GetHost()->GetFocusedWindow());
  DispatchEventAndAckImmediately(CreatePointerUpEvent(21, 22));
  connection1_client->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  connection1->GetHost()->AddActivationParent(
      WindowIdToTransportId(embed_window_id));

  // Focus should go to child1. This result in notifying both the window
  // manager and client connection being notified.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(21, 22));
  EXPECT_EQ(v1, connection1->GetHost()->GetFocusedWindow());
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
  EXPECT_EQ(v1, host_connection()->window_tree_host()->GetFocusedWindow());

  DispatchEventAndAckImmediately(CreatePointerUpEvent(21, 22));
  wm_client()->tracker()->changes()->clear();
  connection1_client->tracker()->changes()->clear();

  // Press in the same location. Should not get a focus change event (only input
  // event).
  DispatchEventAndAckImmediately(CreatePointerDownEvent(61, 22));
  EXPECT_EQ(v1, host_connection()->window_tree_host()->GetFocusedWindow());
  ASSERT_EQ(wm_client()->tracker()->changes()->size(), 1u)
      << SingleChangeToDescription(*wm_client()->tracker()->changes());
  EXPECT_EQ("InputEvent window=0,2 event_action=4",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  EXPECT_TRUE(connection1_client->tracker()->changes()->empty());
}

TEST_F(WindowTreeTest, BasicInputEventTarget) {
  TestWindowTreeClient* embed_connection = nullptr;
  ServerWindow* out_window = nullptr;
  EXPECT_NO_FATAL_FAILURE(SetupEventTargeting(&embed_connection, &out_window));

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
  ServerWindow* out_window = nullptr;
  EXPECT_NO_FATAL_FAILURE(SetupEventTargeting(&embed_connection, &out_window));

  // Like in BasicInputEventTarget, we send a pointer down event to be
  // dispatched. This is only to place the mouse cursor over that window though.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(21, 22));

  out_window->SetPredefinedCursor(mojom::Cursor::CURSOR_IBEAM);

  // Because the cursor is over the window when SetCursor was called, we should
  // have immediately changed the cursor.
  EXPECT_EQ(mojom::Cursor::CURSOR_IBEAM, cursor_id());
}

TEST_F(WindowTreeTest, CursorChangesWhenEnteringWindowWithDifferentCursor) {
  TestWindowTreeClient* embed_connection = nullptr;
  ServerWindow* out_window = nullptr;
  EXPECT_NO_FATAL_FAILURE(SetupEventTargeting(&embed_connection, &out_window));

  // Let's create a pointer event outside the window and then move the pointer
  // inside.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(5, 5));
  out_window->SetPredefinedCursor(mojom::Cursor::CURSOR_IBEAM);
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());

  DispatchEventAndAckImmediately(CreateMouseMoveEvent(21, 22));
  EXPECT_EQ(mojom::Cursor::CURSOR_IBEAM, cursor_id());
}

TEST_F(WindowTreeTest, TouchesDontChangeCursor) {
  TestWindowTreeClient* embed_connection = nullptr;
  ServerWindow* out_window = nullptr;
  EXPECT_NO_FATAL_FAILURE(SetupEventTargeting(&embed_connection, &out_window));

  // Let's create a pointer event outside the window and then move the pointer
  // inside.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(5, 5));
  out_window->SetPredefinedCursor(mojom::Cursor::CURSOR_IBEAM);
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());

  // With a touch event, we shouldn't update the cursor.
  DispatchEventAndAckImmediately(CreatePointerDownEvent(21, 22));
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());
}

TEST_F(WindowTreeTest, DragOutsideWindow) {
  TestWindowTreeClient* embed_connection = nullptr;
  ServerWindow* out_window = nullptr;
  EXPECT_NO_FATAL_FAILURE(SetupEventTargeting(&embed_connection, &out_window));

  // Start with the cursor outside the window. Setting the cursor shouldn't
  // change the cursor.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(5, 5));
  out_window->SetPredefinedCursor(mojom::Cursor::CURSOR_IBEAM);
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());

  // Move the pointer to the inside of the window
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(21, 22));
  EXPECT_EQ(mojom::Cursor::CURSOR_IBEAM, cursor_id());

  // Start the drag.
  DispatchEventAndAckImmediately(CreateMouseDownEvent(21, 22));
  EXPECT_EQ(mojom::Cursor::CURSOR_IBEAM, cursor_id());

  // Move the cursor (mouse is still down) outside the window.
  DispatchEventAndAckImmediately(CreateMouseMoveEvent(5, 5));
  EXPECT_EQ(mojom::Cursor::CURSOR_IBEAM, cursor_id());

  // Release the cursor. We should now adapt the cursor of the window
  // underneath the pointer.
  DispatchEventAndAckImmediately(CreateMouseUpEvent(5, 5));
  EXPECT_EQ(mojom::Cursor::CURSOR_NULL, cursor_id());
}

// TODO(erg): Add tests for when programmatic changes to the window hierarchy
// would cause the window that supplies the cursor to change.

TEST_F(WindowTreeTest, EventAck) {
  const WindowId embed_window_id(wm_connection()->id(), 1);
  EXPECT_TRUE(
      wm_connection()->NewWindow(embed_window_id, ServerWindow::Properties()));
  EXPECT_TRUE(wm_connection()->SetWindowVisibility(embed_window_id, true));
  EXPECT_TRUE(
      wm_connection()->AddWindow(*(wm_connection()->root()), embed_window_id));
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

}  // namespace ws

}  // namespace mus
