// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/util.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/connection_manager_delegate.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/display_manager_factory.h"
#include "components/mus/ws/ids.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/test_change_tracker.h"
#include "components/mus/ws/window_tree_host_connection.h"
#include "components/mus/ws/window_tree_impl.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using mojo::Array;
using mojo::InterfaceRequest;
using mojo::ServiceProvider;
using mojo::ServiceProviderPtr;
using mojo::String;
using mus::mojom::ERROR_CODE_NONE;
using mus::mojom::WindowDataPtr;

namespace mus {

namespace ws {
namespace {

// -----------------------------------------------------------------------------

// WindowTreeClient implementation that logs all calls to a TestChangeTracker.
// TODO(sky): refactor so both this and WindowTreeAppTest share code.
class TestWindowTreeClient : public mus::mojom::WindowTreeClient {
 public:
  TestWindowTreeClient() {}
  ~TestWindowTreeClient() override {}

  TestChangeTracker* tracker() { return &tracker_; }

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
  void OnClientAreaChanged(uint32_t window_id,
                           mojo::RectPtr old_client_area,
                           mojo::RectPtr new_client_area) override {}
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
  void OnWindowInputEvent(uint32_t window,
                          mojo::EventPtr event,
                          const mojo::Callback<void()>& callback) override {
    tracker_.OnWindowInputEvent(window, event.Pass());
  }
  void OnWindowFocused(uint32_t focused_window_id) override {
    tracker_.OnWindowFocused(focused_window_id);
  }

  TestChangeTracker tracker_;

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
      mojo::URLRequestPtr request,
      const WindowId& root_id,
      uint32_t policy_bitmask) override {
    scoped_ptr<WindowTreeImpl> service(new WindowTreeImpl(
        connection_manager, creator_id, root_id, policy_bitmask));
    last_connection_ = new TestClientConnection(service.Pass());
    return last_connection_;
  }
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
  TestDisplayManager() {}
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
  const mojom::ViewportMetrics& GetViewportMetrics() override {
    return display_metrices_;
  }
  void UpdateTextInputState(const ui::TextInputState& state) override {}
  void SetImeVisibility(bool visible) override {}

 private:
  mojom::ViewportMetrics display_metrices_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayManager);
};

// Factory that dispenses TestDisplayManagers.
class TestDisplayManagerFactory : public DisplayManagerFactory {
 public:
  TestDisplayManagerFactory() {}
  ~TestDisplayManagerFactory() {}
  DisplayManager* CreateDisplayManager(
      mojo::ApplicationImpl* app_impl,
      const scoped_refptr<GpuState>& gpu_state,
      const scoped_refptr<mus::SurfacesState>& surfaces_state) override {
    return new TestDisplayManager();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDisplayManagerFactory);
};

mojo::EventPtr CreatePointerDownEvent(int x, int y) {
  mojo::EventPtr event(mojo::Event::New());
  event->action = mojo::EVENT_TYPE_POINTER_DOWN;
  event->pointer_data = mojo::PointerData::New();
  event->pointer_data->pointer_id = 1u;
  event->pointer_data->location = mojo::LocationData::New();
  event->pointer_data->location->x = x;
  event->pointer_data->location->y = y;
  return event.Pass();
}

mojo::EventPtr CreatePointerUpEvent(int x, int y) {
  mojo::EventPtr event(mojo::Event::New());
  event->action = mojo::EVENT_TYPE_POINTER_UP;
  event->pointer_data = mojo::PointerData::New();
  event->pointer_data->pointer_id = 1u;
  event->pointer_data->location = mojo::LocationData::New();
  event->pointer_data->location->x = x;
  event->pointer_data->location->y = y;
  return event.Pass();
}

}  // namespace

// -----------------------------------------------------------------------------

class WindowTreeTest : public testing::Test {
 public:
  WindowTreeTest() : wm_client_(nullptr) {}
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

  TestWindowTreeHostConnection* host_connection() { return host_connection_; }
  DisplayManagerDelegate* display_manager_delegate() {
    return host_connection()->window_tree_host();
  }

 protected:
  // testing::Test:
  void SetUp() override {
    DisplayManager::set_factory_for_testing(&display_manager_factory_);
    // TODO(fsamuel): This is probably broken. We need a root.
    connection_manager_.reset(
        new ConnectionManager(&delegate_, scoped_refptr<SurfacesState>()));
    WindowTreeHostImpl* host = new WindowTreeHostImpl(
        mus::mojom::WindowTreeHostClientPtr(), connection_manager_.get(),
        nullptr, scoped_refptr<GpuState>(),
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
  TestDisplayManagerFactory display_manager_factory_;
  TestConnectionManagerDelegate delegate_;
  TestWindowTreeHostConnection* host_connection_;
  scoped_ptr<ConnectionManager> connection_manager_;
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeTest);
};

// Verifies focus correctly changes on pointer events.
TEST_F(WindowTreeTest, FocusOnPointer) {
  const WindowId embed_window_id(wm_connection()->id(), 1);
  EXPECT_EQ(ERROR_CODE_NONE, wm_connection()->NewWindow(embed_window_id));
  EXPECT_TRUE(wm_connection()->SetWindowVisibility(embed_window_id, true));
  EXPECT_TRUE(
      wm_connection()->AddWindow(*(wm_connection()->root()), embed_window_id));
  host_connection()->window_tree_host()->root_window()->SetBounds(
      gfx::Rect(0, 0, 100, 100));
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  wm_connection()->Embed(embed_window_id, request.Pass());
  WindowTreeImpl* connection1 =
      connection_manager()->GetConnectionWithRoot(embed_window_id);
  ASSERT_TRUE(connection1 != nullptr);
  ASSERT_NE(connection1, wm_connection());

  connection_manager()
      ->GetWindow(embed_window_id)
      ->SetBounds(gfx::Rect(0, 0, 50, 50));

  const WindowId child1(connection1->id(), 1);
  EXPECT_EQ(ERROR_CODE_NONE, connection1->NewWindow(child1));
  EXPECT_TRUE(connection1->AddWindow(embed_window_id, child1));
  ServerWindow* v1 = connection1->GetWindow(child1);
  v1->SetVisible(true);
  v1->SetBounds(gfx::Rect(20, 20, 20, 20));

  TestWindowTreeClient* connection1_client = last_window_tree_client();
  connection1_client->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  display_manager_delegate()->OnEvent(CreatePointerDownEvent(21, 22));
  // Focus should go to child1. This result in notifying both the window
  // manager and client connection being notified.
  EXPECT_EQ(v1, connection1->GetHost()->GetFocusedWindow());
  ASSERT_GE(wm_client()->tracker()->changes()->size(), 1u);
  EXPECT_EQ("Focused id=2,1",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  ASSERT_GE(connection1_client->tracker()->changes()->size(), 1u);
  EXPECT_EQ(
      "Focused id=2,1",
      ChangesToDescription1(*connection1_client->tracker()->changes())[0]);

  display_manager_delegate()->OnEvent(CreatePointerUpEvent(21, 22));
  wm_client()->tracker()->changes()->clear();
  connection1_client->tracker()->changes()->clear();

  // Press outside of the embedded window. Focus should go to the root. Notice
  // the client1 doesn't see who has focus as the focused window (root) isn't
  // visible to it.
  display_manager_delegate()->OnEvent(CreatePointerDownEvent(61, 22));
  EXPECT_EQ(host_connection()->window_tree_host()->root_window(),
            host_connection()->window_tree_host()->GetFocusedWindow());
  ASSERT_GE(wm_client()->tracker()->changes()->size(), 1u);
  EXPECT_EQ("Focused id=0,2",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  ASSERT_GE(connection1_client->tracker()->changes()->size(), 1u);
  EXPECT_EQ(
      "Focused id=null",
      ChangesToDescription1(*connection1_client->tracker()->changes())[0]);

  display_manager_delegate()->OnEvent(CreatePointerUpEvent(21, 22));
  wm_client()->tracker()->changes()->clear();
  connection1_client->tracker()->changes()->clear();

  // Press in the same location. Should not get a focus change event (only input
  // event).
  display_manager_delegate()->OnEvent(CreatePointerDownEvent(61, 22));
  EXPECT_EQ(host_connection()->window_tree_host()->root_window(),
            host_connection()->window_tree_host()->GetFocusedWindow());
  ASSERT_EQ(wm_client()->tracker()->changes()->size(), 1u);
  EXPECT_EQ("InputEvent window=0,2 event_action=4",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  EXPECT_TRUE(connection1_client->tracker()->changes()->empty());
}

TEST_F(WindowTreeTest, BasicInputEventTarget) {
  const WindowId embed_window_id(wm_connection()->id(), 1);
  EXPECT_EQ(ERROR_CODE_NONE, wm_connection()->NewWindow(embed_window_id));
  EXPECT_TRUE(wm_connection()->SetWindowVisibility(embed_window_id, true));
  EXPECT_TRUE(
      wm_connection()->AddWindow(*(wm_connection()->root()), embed_window_id));
  host_connection()->window_tree_host()->root_window()->SetBounds(
      gfx::Rect(0, 0, 100, 100));
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  wm_connection()->Embed(embed_window_id, request.Pass());
  WindowTreeImpl* connection1 =
      connection_manager()->GetConnectionWithRoot(embed_window_id);
  ASSERT_TRUE(connection1 != nullptr);
  ASSERT_NE(connection1, wm_connection());

  connection_manager()
      ->GetWindow(embed_window_id)
      ->SetBounds(gfx::Rect(0, 0, 50, 50));

  const WindowId child1(connection1->id(), 1);
  EXPECT_EQ(ERROR_CODE_NONE, connection1->NewWindow(child1));
  EXPECT_TRUE(connection1->AddWindow(embed_window_id, child1));
  ServerWindow* v1 = connection1->GetWindow(child1);
  v1->SetVisible(true);
  v1->SetBounds(gfx::Rect(20, 20, 20, 20));

  TestWindowTreeClient* embed_connection = last_window_tree_client();
  embed_connection->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  // Send an event to |v1|. |embed_connection| should get the event, not
  // |wm_client|, since |v1| lives inside an embedded window.
  display_manager_delegate()->OnEvent(CreatePointerDownEvent(21, 22));
  ASSERT_EQ(1u, wm_client()->tracker()->changes()->size());
  EXPECT_EQ("Focused id=2,1",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  ASSERT_EQ(2u, embed_connection->tracker()->changes()->size());
  EXPECT_EQ("Focused id=2,1",
            ChangesToDescription1(*embed_connection->tracker()->changes())[0]);
  EXPECT_EQ("InputEvent window=2,1 event_action=4",
            ChangesToDescription1(*embed_connection->tracker()->changes())[1]);
}

}  // namespace ws

}  // namespace mus
