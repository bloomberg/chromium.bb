// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_TEST_UTILS_H_
#define COMPONENTS_MUS_WS_TEST_UTILS_H_

#include <stdint.h>

#include <vector>

#include "components/mus/public/interfaces/display.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/ws/display.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/event_dispatcher.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/platform_display_factory.h"
#include "components/mus/ws/test_change_tracker.h"
#include "components/mus/ws/user_display_manager.h"
#include "components/mus/ws/window_manager_factory_registry.h"
#include "components/mus/ws/window_manager_state.h"
#include "components/mus/ws/window_server_delegate.h"
#include "components/mus/ws/window_tree.h"
#include "components/mus/ws/window_tree_binding.h"

namespace mus {
namespace ws {
namespace test {

// Collection of utilities useful in creating mus tests.

class WindowManagerFactoryRegistryTestApi {
 public:
  WindowManagerFactoryRegistryTestApi(WindowManagerFactoryRegistry* registry);
  ~WindowManagerFactoryRegistryTestApi();

  void AddService(const UserId& user_id, mojom::WindowManagerFactory* factory);

 private:
  WindowManagerFactoryRegistry* registry_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerFactoryRegistryTestApi);
};

// -----------------------------------------------------------------------------

class UserDisplayManagerTestApi {
 public:
  explicit UserDisplayManagerTestApi(UserDisplayManager* udm) : udm_(udm) {}
  ~UserDisplayManagerTestApi() {}

  void SetTestObserver(mojom::DisplayManagerObserver* observer) {
    udm_->test_observer_ = observer;
    if (observer)
      udm_->OnObserverAdded(observer);
  }

 private:
  UserDisplayManager* udm_;

  DISALLOW_COPY_AND_ASSIGN(UserDisplayManagerTestApi);
};

// -----------------------------------------------------------------------------

class WindowTreeTestApi {
 public:
  WindowTreeTestApi(WindowTree* tree);
  ~WindowTreeTestApi();

  void set_window_manager_internal(mojom::WindowManager* wm_internal) {
    tree_->window_manager_internal_ = wm_internal;
  }

  void ClearAck() { tree_->event_ack_id_ = 0; }
  void EnableCapture() { tree_->event_ack_id_ = 1u; }

 private:
  WindowTree* tree_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeTestApi);
};

// -----------------------------------------------------------------------------

class DisplayTestApi {
 public:
  explicit DisplayTestApi(Display* display);
  ~DisplayTestApi();

  void OnEvent(const ui::Event& event) { display_->OnEvent(event); }

 private:
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(DisplayTestApi);
};

// -----------------------------------------------------------------------------

class EventDispatcherTestApi {
 public:
  explicit EventDispatcherTestApi(EventDispatcher* ed) : ed_(ed) {}
  ~EventDispatcherTestApi() {}

  bool AreAnyPointersDown() const { return ed_->AreAnyPointersDown(); }
  bool is_mouse_button_down() const { return ed_->mouse_button_down_; }
  bool IsObservingWindow(ServerWindow* window) {
    return ed_->IsObservingWindow(window);
  }
  int NumberPointerTargetsForWindow(ServerWindow* window);

 private:
  EventDispatcher* ed_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcherTestApi);
};

// -----------------------------------------------------------------------------

class WindowManagerStateTestApi {
 public:
  explicit WindowManagerStateTestApi(WindowManagerState* wms) : wms_(wms) {}
  ~WindowManagerStateTestApi() {}

  void DispatchInputEventToWindow(ServerWindow* target,
                                  bool in_nonclient_area,
                                  const ui::Event& event,
                                  Accelerator* accelerator) {
    wms_->DispatchInputEventToWindow(target, in_nonclient_area, event,
                                     accelerator);
  }

  void OnEventAckTimeout() { wms_->OnEventAckTimeout(); }

  mojom::WindowTree* tree_awaiting_input_ack() {
    return wms_->tree_awaiting_input_ack_;
  }

 private:
  WindowManagerState* wms_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerStateTestApi);
};

// -----------------------------------------------------------------------------

// Factory that always embeds the new WindowTree as the root user id.
class TestDisplayBinding : public DisplayBinding {
 public:
  TestDisplayBinding(Display* display, WindowServer* window_server)
      : display_(display), window_server_(window_server) {}
  ~TestDisplayBinding() override {}

 private:
  // DisplayBinding:
  WindowTree* CreateWindowTree(ServerWindow* root) override;

  Display* display_;
  WindowServer* window_server_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayBinding);
};

// -----------------------------------------------------------------------------

// Factory that dispenses TestPlatformDisplays.
class TestPlatformDisplayFactory : public PlatformDisplayFactory {
 public:
  explicit TestPlatformDisplayFactory(int32_t* cursor_id_storage);
  ~TestPlatformDisplayFactory();

  // PlatformDisplayFactory:
  PlatformDisplay* CreatePlatformDisplay() override;

 private:
  int32_t* cursor_id_storage_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatformDisplayFactory);
};

// -----------------------------------------------------------------------------

class TestWindowManager : public mojom::WindowManager {
 public:
  TestWindowManager()
      : got_create_top_level_window_(false),
        change_id_(0u),
        on_accelerator_called_(false),
        on_accelerator_id_(0u) {}
  ~TestWindowManager() override {}

  bool did_call_create_top_level_window(uint32_t* change_id) {
    if (!got_create_top_level_window_)
      return false;

    got_create_top_level_window_ = false;
    *change_id = change_id_;
    return true;
  }

  bool on_accelerator_called() { return on_accelerator_called_; }
  uint32_t on_accelerator_id() { return on_accelerator_id_; }

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
      mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) override;
  void OnAccelerator(uint32_t id, mojom::EventPtr event) override;

  bool got_create_top_level_window_;
  uint32_t change_id_;

  bool on_accelerator_called_;
  uint32_t on_accelerator_id_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowManager);
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
               bool drawn) override;
  void OnEmbeddedAppDisconnected(uint32_t window) override;
  void OnUnembed(Id window_id) override;
  void OnLostCapture(Id window_id) override;
  void OnTopLevelCreated(uint32_t change_id,
                         mojom::WindowDataPtr data,
                         bool drawn) override;
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
      uint32_t old_parent,
      uint32_t new_parent,
      mojo::Array<mojom::WindowDataPtr> windows) override;
  void OnWindowReordered(uint32_t window_id,
                         uint32_t relative_window_id,
                         mojom::OrderDirection direction) override;
  void OnWindowDeleted(uint32_t window) override;
  void OnWindowVisibilityChanged(uint32_t window, bool visible) override;
  void OnWindowOpacityChanged(uint32_t window,
                              float old_opacity,
                              float new_opacity) override;
  void OnWindowParentDrawnStateChanged(uint32_t window, bool drawn) override;
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

// WindowTreeBinding implementation that vends TestWindowTreeBinding.
class TestWindowTreeBinding : public WindowTreeBinding {
 public:
  explicit TestWindowTreeBinding(WindowTree* tree);
  ~TestWindowTreeBinding() override;

  WindowTree* tree() { return tree_; }
  TestWindowTreeClient* client() { return &client_; }

  bool is_paused() const { return is_paused_; }

  // WindowTreeBinding:
  mojom::WindowManager* GetWindowManager() override;
  void SetIncomingMethodCallProcessingPaused(bool paused) override;

 private:
  WindowTree* tree_;
  TestWindowTreeClient client_;
  bool is_paused_ = false;
  scoped_ptr<TestWindowManager> window_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeBinding);
};

// -----------------------------------------------------------------------------

// WindowServerDelegate that creates TestWindowTreeClients.
class TestWindowServerDelegate : public WindowServerDelegate {
 public:
  TestWindowServerDelegate();
  ~TestWindowServerDelegate() override;

  void set_window_server(WindowServer* window_server) {
    window_server_ = window_server;
  }

  void set_num_displays_to_create(int count) {
    num_displays_to_create_ = count;
  }

  TestWindowTreeClient* last_client() {
    return last_binding() ? last_binding()->client() : nullptr;
  }
  TestWindowTreeBinding* last_binding() {
    return bindings_.empty() ? nullptr : bindings_.back();
  }

  std::vector<TestWindowTreeBinding*>* bindings() { return &bindings_; }

  bool got_on_no_more_displays() const { return got_on_no_more_displays_; }

  Display* AddDisplay();

  // WindowServerDelegate:
  void OnNoMoreDisplays() override;
  scoped_ptr<WindowTreeBinding> CreateWindowTreeBinding(
      BindingType type,
      ws::WindowServer* window_server,
      ws::WindowTree* tree,
      mojom::WindowTreeRequest* tree_request,
      mojom::WindowTreeClientPtr* client) override;
  void CreateDefaultDisplays() override;
  bool IsTestConfig() const override;

 private:
  // If CreateDefaultDisplays() this is the number of Displays that are
  // created. The default is 0, which results in a DCHECK.
  int num_displays_to_create_ = 0;
  Display* display_ = nullptr;
  WindowServer* window_server_ = nullptr;
  bool got_on_no_more_displays_ = false;
  // All TestWindowTreeBinding objects created via CreateWindowTreeBinding.
  // These are owned by the corresponding WindowTree.
  std::vector<TestWindowTreeBinding*> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowServerDelegate);
};

// -----------------------------------------------------------------------------

// Returns the first and only root of |tree|. If |tree| has zero or more than
// one root returns null.
ServerWindow* FirstRoot(WindowTree* tree);

// Returns the ClientWindowId of the first root of |tree|, or an empty
// ClientWindowId if |tree| has zero or more than one root.
ClientWindowId FirstRootId(WindowTree* tree);

// Returns |tree|s ClientWindowId for |window|.
ClientWindowId ClientWindowIdForWindow(WindowTree* tree,
                                       const ServerWindow* window);

// Creates a new visible window as a child of the single root of |tree|.
// |client_id| set to the ClientWindowId of the new window.
ServerWindow* NewWindowInTree(WindowTree* tree, ClientWindowId* client_id);

}  // namespace test
}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_TEST_UTILS_H_
