// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_TEST_UTILS_H_
#define COMPONENTS_MUS_WS_TEST_UTILS_H_

#include <stdint.h>

#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager_delegate.h"
#include "components/mus/ws/display.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/platform_display_factory.h"
#include "components/mus/ws/test_change_tracker.h"
#include "components/mus/ws/window_manager_factory_registry.h"
#include "components/mus/ws/window_tree_impl.h"

namespace mus {
namespace ws {
namespace test {

// Collection of utilities useful in creating mus tests.

class WindowManagerFactoryRegistryTestApi {
 public:
  WindowManagerFactoryRegistryTestApi(WindowManagerFactoryRegistry* registry);
  ~WindowManagerFactoryRegistryTestApi();

  void AddService(UserId user_id, mojom::WindowManagerFactory* factory);

 private:
  WindowManagerFactoryRegistry* registry_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerFactoryRegistryTestApi);
};

// -----------------------------------------------------------------------------

class WindowTreeTestApi {
 public:
  WindowTreeTestApi(WindowTreeImpl* tree);
  ~WindowTreeTestApi();

  void set_window_manager_internal(mojom::WindowManager* wm_internal) {
    tree_->window_manager_internal_ = wm_internal;
  }

 private:
  WindowTreeImpl* tree_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeTestApi);
};

// -----------------------------------------------------------------------------

class DisplayTestApi {
 public:
  explicit DisplayTestApi(Display* display);
  ~DisplayTestApi();

  void OnEvent(const ui::Event& event) { display_->OnEvent(event); }

  mojom::WindowTree* tree_awaiting_input_ack() {
    return display_->tree_awaiting_input_ack_;
  }

 private:
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(DisplayTestApi);
};

// -----------------------------------------------------------------------------

// Factory that dispenses TestPlatformDisplays.
class TestPlatformDisplayFactory : public PlatformDisplayFactory {
 public:
  explicit TestPlatformDisplayFactory(int32_t* cursor_id_storage);
  ~TestPlatformDisplayFactory();

  // PlatformDisplayFactory:
  PlatformDisplay* CreatePlatformDisplay(
      mojo::Connector* connector,
      const scoped_refptr<GpuState>& gpu_state,
      const scoped_refptr<mus::SurfacesState>& surfaces_state) override;

 private:
  int32_t* cursor_id_storage_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatformDisplayFactory);
};

// -----------------------------------------------------------------------------

// WindowTreeClient implementation that logs all calls to a TestChangeTracker.
class TestWindowTreeClient : public mus::mojom::WindowTreeClient {
 public:
  TestWindowTreeClient();
  ~TestWindowTreeClient() override;

  TestChangeTracker* tracker() { return &tracker_; }

  void Bind(mojo::InterfaceRequest<mojom::WindowTreeClient> request);

  void set_record_on_change_completed(bool value) {
    record_on_change_completed_ = value;
  }

 private:
  // WindowTreeClient:
  void OnEmbed(uint16_t connection_id,
               mojom::WindowDataPtr root,
               mus::mojom::WindowTreePtr tree,
               Id focused_window_id,
               uint32_t access_policy) override;
  void OnEmbeddedAppDisconnected(uint32_t window) override;
  void OnUnembed(Id window_id) override;
  void OnLostCapture(Id window_id) override;
  void OnTopLevelCreated(uint32_t change_id,
                         mojom::WindowDataPtr data) override;
  void OnWindowBoundsChanged(uint32_t window,
                             mojo::RectPtr old_bounds,
                             mojo::RectPtr new_bounds) override;
  void OnClientAreaChanged(
      uint32_t window_id,
      mojo::InsetsPtr new_client_area,
      mojo::Array<mojo::RectPtr> new_additional_client_areas) override;
  void OnTransientWindowAdded(uint32_t window_id,
                              uint32_t transient_window_id) override;
  void OnTransientWindowRemoved(uint32_t window_id,
                                uint32_t transient_window_id) override;
  void OnWindowViewportMetricsChanged(
      mojo::Array<uint32_t> window_ids,
      mojom::ViewportMetricsPtr old_metrics,
      mojom::ViewportMetricsPtr new_metrics) override;
  void OnWindowHierarchyChanged(
      uint32_t window,
      uint32_t new_parent,
      uint32_t old_parent,
      mojo::Array<mojom::WindowDataPtr> windows) override;
  void OnWindowReordered(uint32_t window_id,
                         uint32_t relative_window_id,
                         mojom::OrderDirection direction) override;
  void OnWindowDeleted(uint32_t window) override;
  void OnWindowVisibilityChanged(uint32_t window, bool visible) override;
  void OnWindowDrawnStateChanged(uint32_t window, bool drawn) override;
  void OnWindowSharedPropertyChanged(uint32_t window,
                                     const mojo::String& name,
                                     mojo::Array<uint8_t> new_data) override;
  void OnWindowInputEvent(uint32_t event_id,
                          uint32_t window,
                          mojom::EventPtr event) override;
  void OnWindowFocused(uint32_t focused_window_id) override;
  void OnWindowPredefinedCursorChanged(uint32_t window_id,
                                       mojom::Cursor cursor_id) override;
  void OnChangeCompleted(uint32_t change_id, bool success) override;
  void RequestClose(uint32_t window_id) override;
  void GetWindowManager(
      mojo::AssociatedInterfaceRequest<mojom::WindowManager> internal) override;

  TestChangeTracker tracker_;
  mojo::Binding<mojom::WindowTreeClient> binding_;
  bool record_on_change_completed_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeClient);
};

// -----------------------------------------------------------------------------

// ClientConnection implementation that vends TestWindowTreeClient.
class TestClientConnection : public ClientConnection {
 public:
  TestClientConnection();
  ~TestClientConnection() override;

  TestWindowTreeClient* client() { return &client_; }

  bool is_paused() const { return is_paused_; }

  // ClientConnection:
  mojom::WindowManager* GetWindowManager() override;
  void SetIncomingMethodCallProcessingPaused(bool paused) override;

 private:
  TestWindowTreeClient client_;
  bool is_paused_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestClientConnection);
};

// -----------------------------------------------------------------------------

// ConnectionManagerDelegate that creates TestWindowTreeClients.
class TestConnectionManagerDelegate : public ConnectionManagerDelegate {
 public:
  TestConnectionManagerDelegate();
  ~TestConnectionManagerDelegate() override;

  void set_connection_manager(ConnectionManager* connection_manager) {
    connection_manager_ = connection_manager;
  }

  void set_num_displays_to_create(int count) {
    num_displays_to_create_ = count;
  }

  TestWindowTreeClient* last_client() {
    return last_connection_ ? last_connection_->client() : nullptr;
  }
  TestClientConnection* last_connection() { return last_connection_; }

  bool got_on_no_more_connections() const {
    return got_on_no_more_connections_;
  }

  // ConnectionManagerDelegate:
  void OnNoMoreRootConnections() override;
  scoped_ptr<ClientConnection> CreateClientConnectionForEmbedAtWindow(
      ws::ConnectionManager* connection_manager,
      ws::WindowTreeImpl* tree,
      mojom::WindowTreeRequest tree_request,
      mojom::WindowTreeClientPtr client) override;
  void CreateDefaultDisplays() override;

 private:
  // If CreateDefaultDisplays() this is the number of Displays that are
  // created. The default is 0, which results in a DCHECK.
  int num_displays_to_create_ = 0;
  TestClientConnection* last_connection_ = nullptr;
  Display* display_ = nullptr;
  ConnectionManager* connection_manager_ = nullptr;
  bool got_on_no_more_connections_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestConnectionManagerDelegate);
};

}  // namespace test
}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_TEST_UTILS_H_
