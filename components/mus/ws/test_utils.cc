// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/test_utils.h"

#include "cc/output/copy_output_request.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/window_manager_access_policy.h"
#include "components/mus/ws/window_manager_factory_service.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "services/shell/public/interfaces/connector.mojom.h"

namespace mus {
namespace ws {
namespace test {
namespace {

// -----------------------------------------------------------------------------
// Empty implementation of PlatformDisplay.
class TestPlatformDisplay : public PlatformDisplay {
 public:
  explicit TestPlatformDisplay(int32_t* cursor_id_storage)
      : cursor_id_storage_(cursor_id_storage) {
    display_metrics_.size_in_pixels = mojo::Size::From(gfx::Size(400, 300));
    display_metrics_.device_pixel_ratio = 1.f;
  }
  ~TestPlatformDisplay() override {}

  // PlatformDisplay:
  void Init(PlatformDisplayDelegate* delegate) override {
    // It is necessary to tell the delegate about the ViewportMetrics to make
    // sure that the DisplayBinding is correctly initialized (and a root-window
    // is created).
    delegate->OnViewportMetricsChanged(mojom::ViewportMetrics(),
                                       display_metrics_);
  }
  void SchedulePaint(const ServerWindow* window,
                     const gfx::Rect& bounds) override {}
  void SetViewportSize(const gfx::Size& size) override {}
  void SetTitle(const base::string16& title) override {}
  void SetCapture() override {}
  void ReleaseCapture() override {}
  void SetCursorById(int32_t cursor) override { *cursor_id_storage_ = cursor; }
  mojom::Rotation GetRotation() override { return mojom::Rotation::VALUE_0; }
  const mojom::ViewportMetrics& GetViewportMetrics() override {
    return display_metrics_;
  }
  void UpdateTextInputState(const ui::TextInputState& state) override {}
  void SetImeVisibility(bool visible) override {}
  bool IsFramePending() const override { return false; }
  void RequestCopyOfOutput(
      scoped_ptr<cc::CopyOutputRequest> output_request) override {}

 private:
  mojom::ViewportMetrics display_metrics_;

  int32_t* cursor_id_storage_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatformDisplay);
};

ClientWindowId NextUnusedClientWindowId(WindowTree* tree) {
  ClientWindowId client_id;
  for (ConnectionSpecificId id = 1;; ++id) {
    // Used the id of the connection in the upper bits to simplify things.
    const ClientWindowId client_id =
        ClientWindowId(WindowIdToTransportId(WindowId(tree->id(), id)));
    if (!tree->GetWindowByClientId(client_id))
      return client_id;
  }
}

}  // namespace

// WindowManagerFactoryRegistryTestApi ----------------------------------------

WindowManagerFactoryRegistryTestApi::WindowManagerFactoryRegistryTestApi(
    WindowManagerFactoryRegistry* registry)
    : registry_(registry) {}

WindowManagerFactoryRegistryTestApi::~WindowManagerFactoryRegistryTestApi() {}

void WindowManagerFactoryRegistryTestApi::AddService(
    const UserId& user_id,
    mojom::WindowManagerFactory* factory) {
  scoped_ptr<WindowManagerFactoryService> service_ptr(
      new WindowManagerFactoryService(registry_, user_id));
  WindowManagerFactoryService* service = service_ptr.get();
  registry_->AddServiceImpl(std::move(service_ptr));
  service->SetWindowManagerFactoryImpl(factory);
}

// TestPlatformDisplayFactory  -------------------------------------------------

TestPlatformDisplayFactory::TestPlatformDisplayFactory(
    int32_t* cursor_id_storage)
    : cursor_id_storage_(cursor_id_storage) {}

TestPlatformDisplayFactory::~TestPlatformDisplayFactory() {}

PlatformDisplay* TestPlatformDisplayFactory::CreatePlatformDisplay() {
  return new TestPlatformDisplay(cursor_id_storage_);
}

// WindowTreeTestApi  ---------------------------------------------------------

WindowTreeTestApi::WindowTreeTestApi(WindowTree* tree) : tree_(tree) {}
WindowTreeTestApi::~WindowTreeTestApi() {}

// DisplayTestApi  ------------------------------------------------------------

DisplayTestApi::DisplayTestApi(Display* display) : display_(display) {}
DisplayTestApi::~DisplayTestApi() {}

// EventDispatcherTestApi  ----------------------------------------------------

int EventDispatcherTestApi::NumberPointerTargetsForWindow(
    ServerWindow* window) {
  int count = 0;
  for (const auto& pair : ed_->pointer_targets_)
    if (pair.second.window == window)
      count++;
  return count;
}

// TestDisplayBinding ---------------------------------------------------------

WindowTree* TestDisplayBinding::CreateWindowTree(ServerWindow* root) {
  return window_server_->EmbedAtWindow(
      root, shell::mojom::kRootUserID, mus::mojom::WindowTreeClientPtr(),
      make_scoped_ptr(new WindowManagerAccessPolicy));
}

// TestWindowManager ----------------------------------------------------------

void TestWindowManager::WmCreateTopLevelWindow(
    uint32_t change_id,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) {
  got_create_top_level_window_ = true;
  change_id_ = change_id;
}

void TestWindowManager::OnAccelerator(uint32_t id, mojom::EventPtr event) {
  on_accelerator_called_ = true;
  on_accelerator_id_ = id;
}

// TestWindowTreeClient -------------------------------------------------------

TestWindowTreeClient::TestWindowTreeClient()
    : binding_(this), record_on_change_completed_(false) {}
TestWindowTreeClient::~TestWindowTreeClient() {}

void TestWindowTreeClient::Bind(
    mojo::InterfaceRequest<mojom::WindowTreeClient> request) {
  binding_.Bind(std::move(request));
}

void TestWindowTreeClient::OnEmbed(uint16_t connection_id,
                                   mojom::WindowDataPtr root,
                                   mus::mojom::WindowTreePtr tree,
                                   Id focused_window_id,
                                   bool drawn) {
  // TODO(sky): add test coverage of |focused_window_id|.
  tracker_.OnEmbed(connection_id, std::move(root), drawn);
}

void TestWindowTreeClient::OnEmbeddedAppDisconnected(uint32_t window) {
  tracker_.OnEmbeddedAppDisconnected(window);
}

void TestWindowTreeClient::OnUnembed(Id window_id) {
  tracker_.OnUnembed(window_id);
}

void TestWindowTreeClient::OnLostCapture(Id window_id) {}

void TestWindowTreeClient::OnTopLevelCreated(uint32_t change_id,
                                             mojom::WindowDataPtr data,
                                             bool drawn) {
  tracker_.OnTopLevelCreated(change_id, std::move(data), drawn);
}

void TestWindowTreeClient::OnWindowBoundsChanged(uint32_t window,
                                                 mojo::RectPtr old_bounds,
                                                 mojo::RectPtr new_bounds) {
  tracker_.OnWindowBoundsChanged(window, std::move(old_bounds),
                                 std::move(new_bounds));
}

void TestWindowTreeClient::OnClientAreaChanged(
    uint32_t window_id,
    mojo::InsetsPtr new_client_area,
    mojo::Array<mojo::RectPtr> new_additional_client_areas) {}

void TestWindowTreeClient::OnTransientWindowAdded(
    uint32_t window_id,
    uint32_t transient_window_id) {}

void TestWindowTreeClient::OnTransientWindowRemoved(
    uint32_t window_id,
    uint32_t transient_window_id) {}

void TestWindowTreeClient::OnWindowViewportMetricsChanged(
    mojo::Array<uint32_t> window_ids,
    mojom::ViewportMetricsPtr old_metrics,
    mojom::ViewportMetricsPtr new_metrics) {
  tracker_.OnWindowViewportMetricsChanged(std::move(old_metrics),
                                          std::move(new_metrics));
}

void TestWindowTreeClient::OnWindowHierarchyChanged(
    uint32_t window,
    uint32_t old_parent,
    uint32_t new_parent,
    mojo::Array<mojom::WindowDataPtr> windows) {
  tracker_.OnWindowHierarchyChanged(window, old_parent, new_parent,
                                    std::move(windows));
}

void TestWindowTreeClient::OnWindowReordered(uint32_t window_id,
                                             uint32_t relative_window_id,
                                             mojom::OrderDirection direction) {
  tracker_.OnWindowReordered(window_id, relative_window_id, direction);
}

void TestWindowTreeClient::OnWindowDeleted(uint32_t window) {
  tracker_.OnWindowDeleted(window);
}

void TestWindowTreeClient::OnWindowVisibilityChanged(uint32_t window,
                                                     bool visible) {
  tracker_.OnWindowVisibilityChanged(window, visible);
}

void TestWindowTreeClient::OnWindowOpacityChanged(uint32_t window,
                                                  float old_opacity,
                                                  float new_opacity) {
  tracker_.OnWindowOpacityChanged(window, new_opacity);
}

void TestWindowTreeClient::OnWindowParentDrawnStateChanged(uint32_t window,
                                                           bool drawn) {
  tracker_.OnWindowParentDrawnStateChanged(window, drawn);
}

void TestWindowTreeClient::OnWindowSharedPropertyChanged(
    uint32_t window,
    const mojo::String& name,
    mojo::Array<uint8_t> new_data) {
  tracker_.OnWindowSharedPropertyChanged(window, name, std::move(new_data));
}

void TestWindowTreeClient::OnWindowInputEvent(uint32_t event_id,
                                              uint32_t window,
                                              mojom::EventPtr event) {
  tracker_.OnWindowInputEvent(window, std::move(event));
}

void TestWindowTreeClient::OnWindowFocused(uint32_t focused_window_id) {
  tracker_.OnWindowFocused(focused_window_id);
}

void TestWindowTreeClient::OnWindowPredefinedCursorChanged(
    uint32_t window_id,
    mojom::Cursor cursor_id) {
  tracker_.OnWindowPredefinedCursorChanged(window_id, cursor_id);
}

void TestWindowTreeClient::OnChangeCompleted(uint32_t change_id, bool success) {
  if (record_on_change_completed_)
    tracker_.OnChangeCompleted(change_id, success);
}

void TestWindowTreeClient::RequestClose(uint32_t window_id) {}

void TestWindowTreeClient::GetWindowManager(
    mojo::AssociatedInterfaceRequest<mojom::WindowManager> internal) {}

// TestWindowTreeBinding ------------------------------------------------------

TestWindowTreeBinding::TestWindowTreeBinding(WindowTree* tree)
    : WindowTreeBinding(&client_), tree_(tree) {}
TestWindowTreeBinding::~TestWindowTreeBinding() {}

mojom::WindowManager* TestWindowTreeBinding::GetWindowManager() {
  if (!window_manager_.get())
    window_manager_.reset(new TestWindowManager);
  return window_manager_.get();
}
void TestWindowTreeBinding::SetIncomingMethodCallProcessingPaused(bool paused) {
  is_paused_ = paused;
}

// TestWindowServerDelegate ----------------------------------------------

TestWindowServerDelegate::TestWindowServerDelegate() {}
TestWindowServerDelegate::~TestWindowServerDelegate() {}

Display* TestWindowServerDelegate::AddDisplay() {
  // Display manages its own lifetime.
  Display* display = new Display(window_server_, PlatformDisplayInitParams());
  display->Init(nullptr);
  return display;
}

void TestWindowServerDelegate::OnNoMoreDisplays() {
  got_on_no_more_displays_ = true;
}

scoped_ptr<WindowTreeBinding> TestWindowServerDelegate::CreateWindowTreeBinding(
    BindingType type,
    ws::WindowServer* window_server,
    ws::WindowTree* tree,
    mojom::WindowTreeRequest* tree_request,
    mojom::WindowTreeClientPtr* client) {
  scoped_ptr<TestWindowTreeBinding> binding(new TestWindowTreeBinding(tree));
  bindings_.push_back(binding.get());
  return std::move(binding);
}

void TestWindowServerDelegate::CreateDefaultDisplays() {
  DCHECK(num_displays_to_create_);
  DCHECK(window_server_);

  for (int i = 0; i < num_displays_to_create_; ++i)
    AddDisplay();
}

bool TestWindowServerDelegate::IsTestConfig() const {
  return true;
}

ServerWindow* FirstRoot(WindowTree* tree) {
  return tree->roots().size() == 1u
             ? tree->GetWindow((*tree->roots().begin())->id())
             : nullptr;
}

ClientWindowId FirstRootId(WindowTree* tree) {
  ServerWindow* first_root = FirstRoot(tree);
  return first_root ? ClientWindowIdForWindow(tree, first_root)
                    : ClientWindowId();
}

ClientWindowId ClientWindowIdForWindow(WindowTree* tree,
                                       const ServerWindow* window) {
  ClientWindowId client_window_id;
  // If window isn't known we'll return 0, which should then error out.
  tree->IsWindowKnown(window, &client_window_id);
  return client_window_id;
}

ServerWindow* NewWindowInTree(WindowTree* tree, ClientWindowId* client_id) {
  ServerWindow* parent = FirstRoot(tree);
  if (!parent)
    return nullptr;
  ClientWindowId parent_client_id;
  if (!tree->IsWindowKnown(parent, &parent_client_id))
    return nullptr;
  ClientWindowId client_window_id = NextUnusedClientWindowId(tree);
  if (!tree->NewWindow(client_window_id, ServerWindow::Properties()))
    return nullptr;
  if (!tree->SetWindowVisibility(client_window_id, true))
    return nullptr;
  if (!tree->AddWindow(parent_client_id, client_window_id))
    return nullptr;
  *client_id = client_window_id;
  return tree->GetWindowByClientId(client_window_id);
}

}  // namespace test
}  // namespace ws
}  // namespace mus
