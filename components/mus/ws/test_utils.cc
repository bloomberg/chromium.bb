// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/test_utils.h"

#include "cc/output/copy_output_request.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/window_manager_factory_service.h"
#include "mojo/converters/geometry/geometry_type_converters.h"

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

PlatformDisplay* TestPlatformDisplayFactory::CreatePlatformDisplay(
    mojo::Connector* connector,
    const scoped_refptr<GpuState>& gpu_state,
    const scoped_refptr<mus::SurfacesState>& surfaces_state) {
  return new TestPlatformDisplay(cursor_id_storage_);
}

// WindowTreeTestApi  ---------------------------------------------------------

WindowTreeTestApi::WindowTreeTestApi(WindowTree* tree) : tree_(tree) {}
WindowTreeTestApi::~WindowTreeTestApi() {}

// DisplayTestApi  ------------------------------------------------------------

DisplayTestApi::DisplayTestApi(Display* display) : display_(display) {}
DisplayTestApi::~DisplayTestApi() {}

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
                                   uint32_t access_policy) {
  // TODO(sky): add test coverage of |focused_window_id|.
  tracker_.OnEmbed(connection_id, std::move(root));
}

void TestWindowTreeClient::OnEmbeddedAppDisconnected(uint32_t window) {
  tracker_.OnEmbeddedAppDisconnected(window);
}

void TestWindowTreeClient::OnUnembed(Id window_id) {
  tracker_.OnUnembed(window_id);
}

void TestWindowTreeClient::OnLostCapture(Id window_id) {}

void TestWindowTreeClient::OnTopLevelCreated(uint32_t change_id,
                                             mojom::WindowDataPtr data) {
  tracker_.OnTopLevelCreated(change_id, std::move(data));
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
    uint32_t new_parent,
    uint32_t old_parent,
    mojo::Array<mojom::WindowDataPtr> windows) {
  tracker_.OnWindowHierarchyChanged(window, new_parent, old_parent,
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

void TestWindowTreeClient::OnWindowDrawnStateChanged(uint32_t window,
                                                     bool drawn) {
  tracker_.OnWindowDrawnStateChanged(window, drawn);
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

TestWindowTreeBinding::TestWindowTreeBinding() : WindowTreeBinding(&client_) {}
TestWindowTreeBinding::~TestWindowTreeBinding() {}

mojom::WindowManager* TestWindowTreeBinding::GetWindowManager() {
  NOTREACHED();
  return nullptr;
}
void TestWindowTreeBinding::SetIncomingMethodCallProcessingPaused(bool paused) {
  is_paused_ = paused;
}

// TestConnectionManagerDelegate ----------------------------------------------

TestConnectionManagerDelegate::TestConnectionManagerDelegate() {}
TestConnectionManagerDelegate::~TestConnectionManagerDelegate() {}

void TestConnectionManagerDelegate::OnNoMoreRootConnections() {
  got_on_no_more_connections_ = true;
}

scoped_ptr<WindowTreeBinding>
TestConnectionManagerDelegate::CreateWindowTreeBindingForEmbedAtWindow(
    ws::ConnectionManager* connection_manager,
    ws::WindowTree* tree,
    mojom::WindowTreeRequest tree_request,
    mojom::WindowTreeClientPtr client) {
  scoped_ptr<TestWindowTreeBinding> binding(new TestWindowTreeBinding);
  last_binding_ = binding.get();
  return std::move(binding);
}

void TestConnectionManagerDelegate::CreateDefaultDisplays() {
  DCHECK(num_displays_to_create_);
  DCHECK(connection_manager_);

  for (int i = 0; i < num_displays_to_create_; ++i) {
    // Display manages its own lifetime.
    Display* display =
        new Display(connection_manager_, nullptr, scoped_refptr<GpuState>(),
                    scoped_refptr<mus::SurfacesState>());
    display->Init(nullptr);
  }
}

}  // namespace test
}  // namespace ws
}  // namespace mus
