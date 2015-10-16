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
#include "components/mus/ws/server_view.h"
#include "components/mus/ws/test_change_tracker.h"
#include "components/mus/ws/view_tree_host_connection.h"
#include "components/mus/ws/view_tree_impl.h"
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
namespace {

// -----------------------------------------------------------------------------

// WindowTreeClient implementation that logs all calls to a TestChangeTracker.
// TODO(sky): refactor so both this and ViewTreeAppTest share code.
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
               Id focused_view_id,
               uint32_t access_policy) override {
    // TODO(sky): add test coverage of |focused_view_id|.
    tracker_.OnEmbed(connection_id, root.Pass());
  }
  void OnEmbeddedAppDisconnected(uint32_t view) override {
    tracker_.OnEmbeddedAppDisconnected(view);
  }
  void OnUnembed() override { tracker_.OnUnembed(); }
  void OnWindowBoundsChanged(uint32_t view,
                             mojo::RectPtr old_bounds,
                             mojo::RectPtr new_bounds) override {
    tracker_.OnWindowBoundsChanged(view, old_bounds.Pass(), new_bounds.Pass());
  }
  void OnClientAreaChanged(uint32_t window_id,
                           mojo::RectPtr old_client_area,
                           mojo::RectPtr new_client_area) override {
  }
  void OnWindowViewportMetricsChanged(
      mojom::ViewportMetricsPtr old_metrics,
      mojom::ViewportMetricsPtr new_metrics) override {
    tracker_.OnWindowViewportMetricsChanged(old_metrics.Pass(),
                                            new_metrics.Pass());
  }
  void OnWindowHierarchyChanged(uint32_t view,
                                uint32_t new_parent,
                                uint32_t old_parent,
                                Array<WindowDataPtr> views) override {
    tracker_.OnWindowHierarchyChanged(view, new_parent, old_parent,
                                      views.Pass());
  }
  void OnWindowReordered(uint32_t view_id,
                         uint32_t relative_view_id,
                         mojom::OrderDirection direction) override {
    tracker_.OnWindowReordered(view_id, relative_view_id, direction);
  }
  void OnWindowDeleted(uint32_t view) override {
    tracker_.OnWindowDeleted(view);
  }
  void OnWindowVisibilityChanged(uint32_t view, bool visible) override {
    tracker_.OnWindowVisibilityChanged(view, visible);
  }
  void OnWindowDrawnStateChanged(uint32_t view, bool drawn) override {
    tracker_.OnWindowDrawnStateChanged(view, drawn);
  }
  void OnWindowSharedPropertyChanged(uint32_t view,
                                     const String& name,
                                     Array<uint8_t> new_data) override {
    tracker_.OnWindowSharedPropertyChanged(view, name, new_data.Pass());
  }
  void OnWindowInputEvent(uint32_t view,
                          mojo::EventPtr event,
                          const mojo::Callback<void()>& callback) override {
    tracker_.OnWindowInputEvent(view, event.Pass());
  }
  void OnWindowFocused(uint32_t focused_view_id) override {
    tracker_.OnWindowFocused(focused_view_id);
  }

  TestChangeTracker tracker_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeClient);
};

// -----------------------------------------------------------------------------

// ClientConnection implementation that vends TestWindowTreeClient.
class TestClientConnection : public ClientConnection {
 public:
  explicit TestClientConnection(scoped_ptr<ViewTreeImpl> service_impl)
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

  ClientConnection* CreateClientConnectionForEmbedAtView(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mus::mojom::WindowTree> service_request,
      ConnectionSpecificId creator_id,
      mojo::URLRequestPtr request,
      const ViewId& root_id,
      uint32_t policy_bitmask) override {
    scoped_ptr<ViewTreeImpl> service(new ViewTreeImpl(
        connection_manager, creator_id, root_id, policy_bitmask));
    last_connection_ = new TestClientConnection(service.Pass());
    return last_connection_;
  }
  ClientConnection* CreateClientConnectionForEmbedAtView(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mus::mojom::WindowTree> service_request,
      ConnectionSpecificId creator_id,
      const ViewId& root_id,
      uint32_t policy_bitmask,
      mus::mojom::WindowTreeClientPtr client) override {
    // Used by ConnectionManager::AddRoot.
    scoped_ptr<ViewTreeImpl> service(new ViewTreeImpl(
        connection_manager, creator_id, root_id, policy_bitmask));
    last_connection_ = new TestClientConnection(service.Pass());
    return last_connection_;
  }

  TestClientConnection* last_connection_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectionManagerDelegate);
};

// -----------------------------------------------------------------------------

class TestViewTreeHostConnection : public ViewTreeHostConnection {
 public:
  TestViewTreeHostConnection(scoped_ptr<ViewTreeHostImpl> host_impl,
                             ConnectionManager* manager)
      : ViewTreeHostConnection(host_impl.Pass(), manager) {}
  ~TestViewTreeHostConnection() override {}

 private:
  // WindowTreeHostDelegate:
  void OnDisplayInitialized() override {
    connection_manager()->AddHost(this);
    set_view_tree(connection_manager()->EmbedAtView(
        kInvalidConnectionId, view_tree_host()->root_view()->id(),
        mus::mojom::WindowTree::ACCESS_POLICY_EMBED_ROOT,
        mus::mojom::WindowTreeClientPtr()));
  }
  DISALLOW_COPY_AND_ASSIGN(TestViewTreeHostConnection);
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
    // sure that the ViewTreeHostConnection is correctly initialized (and a
    // root-view is created).
    mojom::ViewportMetrics metrics;
    metrics.size_in_pixels = mojo::Size::From(gfx::Size(400, 300));
    metrics.device_pixel_ratio = 1.f;
    delegate->OnViewportMetricsChanged(mojom::ViewportMetrics(), metrics);
  }
  void SchedulePaint(const ServerView* view, const gfx::Rect& bounds) override {
  }
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
      const scoped_refptr<SurfacesState>& surfaces_state) override {
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

class ViewTreeTest : public testing::Test {
 public:
  ViewTreeTest() : wm_client_(nullptr) {}
  ~ViewTreeTest() override {}

  // ViewTreeImpl for the window manager.
  ViewTreeImpl* wm_connection() {
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

  TestViewTreeHostConnection* host_connection() { return host_connection_; }
  DisplayManagerDelegate* display_manager_delegate() {
    return host_connection()->view_tree_host();
  }

 protected:
  // testing::Test:
  void SetUp() override {
    DisplayManager::set_factory_for_testing(&display_manager_factory_);
    // TODO(fsamuel): This is probably broken. We need a root.
    connection_manager_.reset(
        new ConnectionManager(&delegate_, scoped_refptr<SurfacesState>()));
    ViewTreeHostImpl* host = new ViewTreeHostImpl(
        mus::mojom::WindowTreeHostClientPtr(), connection_manager_.get(),
        nullptr, scoped_refptr<GpuState>(), scoped_refptr<SurfacesState>());
    // TODO(fsamuel): This is way too magical. We need to find a better way to
    // manage lifetime.
    host_connection_ = new TestViewTreeHostConnection(
        make_scoped_ptr(host), connection_manager_.get());
    host->Init(host_connection_);
    wm_client_ = delegate_.last_client();
  }

 private:
  // TestWindowTreeClient that is used for the WM connection.
  TestWindowTreeClient* wm_client_;
  TestDisplayManagerFactory display_manager_factory_;
  TestConnectionManagerDelegate delegate_;
  TestViewTreeHostConnection* host_connection_;
  scoped_ptr<ConnectionManager> connection_manager_;
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ViewTreeTest);
};

// Verifies focus correctly changes on pointer events.
TEST_F(ViewTreeTest, FocusOnPointer) {
  const ViewId embed_view_id(wm_connection()->id(), 1);
  EXPECT_EQ(ERROR_CODE_NONE, wm_connection()->NewWindow(embed_view_id));
  EXPECT_TRUE(wm_connection()->SetWindowVisibility(embed_view_id, true));
  EXPECT_TRUE(
      wm_connection()->AddWindow(*(wm_connection()->root()), embed_view_id));
  host_connection()->view_tree_host()->root_view()->SetBounds(
      gfx::Rect(0, 0, 100, 100));
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  wm_connection()->Embed(embed_view_id, request.Pass());
  ViewTreeImpl* connection1 =
      connection_manager()->GetConnectionWithRoot(embed_view_id);
  ASSERT_TRUE(connection1 != nullptr);
  ASSERT_NE(connection1, wm_connection());

  connection_manager()
      ->GetView(embed_view_id)
      ->SetBounds(gfx::Rect(0, 0, 50, 50));

  const ViewId child1(connection1->id(), 1);
  EXPECT_EQ(ERROR_CODE_NONE, connection1->NewWindow(child1));
  EXPECT_TRUE(connection1->AddWindow(embed_view_id, child1));
  ServerView* v1 = connection1->GetView(child1);
  v1->SetVisible(true);
  v1->SetBounds(gfx::Rect(20, 20, 20, 20));

  TestWindowTreeClient* connection1_client = last_window_tree_client();
  connection1_client->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  display_manager_delegate()->OnEvent(CreatePointerDownEvent(21, 22));
  // Focus should go to child1. This result in notifying both the window
  // manager and client connection being notified.
  EXPECT_EQ(v1, connection1->GetHost()->GetFocusedView());
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

  // Press outside of the embedded view. Focus should go to the root. Notice
  // the client1 doesn't see who has focus as the focused view (root) isn't
  // visible to it.
  display_manager_delegate()->OnEvent(CreatePointerDownEvent(61, 22));
  EXPECT_EQ(host_connection()->view_tree_host()->root_view(),
            host_connection()->view_tree_host()->GetFocusedView());
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
  EXPECT_EQ(host_connection()->view_tree_host()->root_view(),
            host_connection()->view_tree_host()->GetFocusedView());
  ASSERT_EQ(wm_client()->tracker()->changes()->size(), 1u);
  EXPECT_EQ("InputEvent view=0,2 event_action=4",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  EXPECT_TRUE(connection1_client->tracker()->changes()->empty());
}

TEST_F(ViewTreeTest, BasicInputEventTarget) {
  const ViewId embed_view_id(wm_connection()->id(), 1);
  EXPECT_EQ(ERROR_CODE_NONE, wm_connection()->NewWindow(embed_view_id));
  EXPECT_TRUE(wm_connection()->SetWindowVisibility(embed_view_id, true));
  EXPECT_TRUE(
      wm_connection()->AddWindow(*(wm_connection()->root()), embed_view_id));
  host_connection()->view_tree_host()->root_view()->SetBounds(
      gfx::Rect(0, 0, 100, 100));
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  wm_connection()->Embed(embed_view_id, request.Pass());
  ViewTreeImpl* connection1 =
      connection_manager()->GetConnectionWithRoot(embed_view_id);
  ASSERT_TRUE(connection1 != nullptr);
  ASSERT_NE(connection1, wm_connection());

  connection_manager()
      ->GetView(embed_view_id)
      ->SetBounds(gfx::Rect(0, 0, 50, 50));

  const ViewId child1(connection1->id(), 1);
  EXPECT_EQ(ERROR_CODE_NONE, connection1->NewWindow(child1));
  EXPECT_TRUE(connection1->AddWindow(embed_view_id, child1));
  ServerView* v1 = connection1->GetView(child1);
  v1->SetVisible(true);
  v1->SetBounds(gfx::Rect(20, 20, 20, 20));

  TestWindowTreeClient* embed_connection = last_window_tree_client();
  embed_connection->tracker()->changes()->clear();
  wm_client()->tracker()->changes()->clear();

  // Send an event to |v1|. |embed_connection| should get the event, not
  // |wm_client|, since |v1| lives inside an embedded view.
  display_manager_delegate()->OnEvent(CreatePointerDownEvent(21, 22));
  ASSERT_EQ(1u, wm_client()->tracker()->changes()->size());
  EXPECT_EQ("Focused id=2,1",
            ChangesToDescription1(*wm_client()->tracker()->changes())[0]);
  ASSERT_EQ(2u, embed_connection->tracker()->changes()->size());
  EXPECT_EQ("Focused id=2,1",
            ChangesToDescription1(*embed_connection->tracker()->changes())[0]);
  EXPECT_EQ("InputEvent view=2,1 event_action=4",
            ChangesToDescription1(*embed_connection->tracker()->changes())[1]);
}

}  // namespace mus
