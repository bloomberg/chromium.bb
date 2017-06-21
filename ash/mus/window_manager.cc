// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/window_manager.h"

#include <stdint.h>

#include <limits>
#include <utility>

#include "ash/drag_drop/drag_image_view.h"
#include "ash/mus/accelerators/accelerator_handler.h"
#include "ash/mus/accelerators/accelerator_ids.h"
#include "ash/mus/bridge/shell_port_mash.h"
#include "ash/mus/move_event_handler.h"
#include "ash/mus/non_client_frame_controller.h"
#include "ash/mus/property_util.h"
#include "ash/mus/shell_delegate_mus.h"
#include "ash/mus/top_level_window_factory.h"
#include "ash/mus/window_properties.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/wm/ash_focus_rules.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/window_state.h"
#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/common/accelerator_util.h"
#include "services/ui/common/types.h"
#include "services/ui/public/cpp/input_devices/input_device_client.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/capture_synchronizer.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/display/display_observer.h"
#include "ui/events/mojo/event.mojom.h"
#include "ui/views/mus/pointer_watcher_event_router.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/wm_state.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace mus {

struct WindowManager::DragState {
  // An image representation of the contents of the current drag and drop
  // clipboard.
  std::unique_ptr<ash::DragImageView> view;

  // The cursor offset of the dragged item.
  gfx::Vector2d image_offset;
};

// TODO: need to register OSExchangeDataProviderMus. http://crbug.com/665077.
WindowManager::WindowManager(service_manager::Connector* connector,
                             Config config,
                             bool show_primary_host_on_connect)
    : connector_(connector),
      config_(config),
      show_primary_host_on_connect_(show_primary_host_on_connect),
      wm_state_(base::MakeUnique<::wm::WMState>()),
      property_converter_(base::MakeUnique<aura::PropertyConverter>()) {
  property_converter_->RegisterPrimitiveProperty(
      kPanelAttachedKey, ui::mojom::WindowManager::kPanelAttached_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter_->RegisterPrimitiveProperty(
      kRenderTitleAreaProperty,
      ui::mojom::WindowManager::kRenderParentTitleArea_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter_->RegisterPrimitiveProperty(
      kShelfItemTypeKey, ui::mojom::WindowManager::kShelfItemType_Property,
      base::Bind(&IsValidShelfItemType));
  property_converter_->RegisterPrimitiveProperty(
      ::wm::kShadowElevationKey,
      ui::mojom::WindowManager::kShadowElevation_Property,
      base::Bind(&::wm::IsValidShadowElevation));
  property_converter_->RegisterPrimitiveProperty(
      kWindowPinTypeKey, ash::mojom::kWindowPinType_Property,
      base::Bind(&ash::IsValidWindowPinType));
  property_converter_->RegisterStringProperty(
      kShelfIDKey, ui::mojom::WindowManager::kShelfID_Property);
}

WindowManager::~WindowManager() {
  Shutdown();
  ash::Shell::set_window_tree_client(nullptr);
  ash::Shell::set_window_manager_client(nullptr);
}

void WindowManager::Init(
    std::unique_ptr<aura::WindowTreeClient> window_tree_client,
    std::unique_ptr<ShellDelegate> shell_delegate) {
  // Only create InputDeviceClient in MASH mode. For MUS mode WindowManager is
  // created by chrome, which creates InputDeviceClient.
  if (config_ == Config::MASH) {
    input_device_client_ = base::MakeUnique<ui::InputDeviceClient>();
  } else {
    // In Config::MUS ash handles all drag and drop.
    window_tree_client->DisableDragDropClient();
  }

  DCHECK(window_manager_client_);
  DCHECK(!window_tree_client_);
  window_tree_client_ = std::move(window_tree_client);

  DCHECK_EQ(nullptr, ash::Shell::window_tree_client());
  ash::Shell::set_window_tree_client(window_tree_client_.get());

  pointer_watcher_event_router_ =
      base::MakeUnique<views::PointerWatcherEventRouter>(
          window_tree_client_.get());

  // Notify PointerWatcherEventRouter and CaptureSynchronizer that the capture
  // client has been set.
  aura::client::CaptureClient* capture_client = wm_state_->capture_controller();
  pointer_watcher_event_router_->AttachToCaptureClient(capture_client);
  window_tree_client_->capture_synchronizer()->AttachToCaptureClient(
      capture_client);

  if (shell_delegate)
    shell_delegate_ = std::move(shell_delegate);
}

void WindowManager::SetLostConnectionCallback(base::OnceClosure closure) {
  lost_connection_callback_ = std::move(closure);
}

bool WindowManager::WaitForInitialDisplays() {
  return window_manager_client_->WaitForInitialDisplays();
}

bool WindowManager::GetNextAcceleratorNamespaceId(uint16_t* id) {
  if (accelerator_handlers_.size() == std::numeric_limits<uint16_t>::max())
    return false;
  while (accelerator_handlers_.count(next_accelerator_namespace_id_) > 0)
    ++next_accelerator_namespace_id_;
  *id = next_accelerator_namespace_id_;
  ++next_accelerator_namespace_id_;
  return true;
}

void WindowManager::AddAcceleratorHandler(uint16_t id_namespace,
                                          AcceleratorHandler* handler) {
  DCHECK_EQ(0u, accelerator_handlers_.count(id_namespace));
  accelerator_handlers_[id_namespace] = handler;
}

void WindowManager::RemoveAcceleratorHandler(uint16_t id_namespace) {
  accelerator_handlers_.erase(id_namespace);
}

display::mojom::DisplayController* WindowManager::GetDisplayController() {
  return display_controller_ ? display_controller_.get() : nullptr;
}

void WindowManager::CreateShell() {
  DCHECK(!created_shell_);
  created_shell_ = true;
  ShellInitParams init_params;
  ShellPortMash* shell_port =
      new ShellPortMash(this, pointer_watcher_event_router_.get());
  // Shell::CreateInstance() takes ownership of ShellDelegate.
  init_params.delegate = shell_delegate_ ? shell_delegate_.release()
                                         : new ShellDelegateMus(connector_);
  init_params.shell_port = shell_port;
  Shell::CreateInstance(init_params);
}

void WindowManager::InstallFrameDecorationValues() {
  ui::mojom::FrameDecorationValuesPtr frame_decoration_values =
      ui::mojom::FrameDecorationValues::New();
  const gfx::Insets client_area_insets =
      NonClientFrameController::GetPreferredClientAreaInsets();
  frame_decoration_values->normal_client_area_insets = client_area_insets;
  frame_decoration_values->maximized_client_area_insets = client_area_insets;
  frame_decoration_values->max_title_bar_button_width =
      NonClientFrameController::GetMaxTitleBarButtonWidth();
  window_manager_client_->SetFrameDecorationValues(
      std::move(frame_decoration_values));
}

void WindowManager::Shutdown() {
  if (!window_tree_client_)
    return;

  aura::client::CaptureClient* capture_client = wm_state_->capture_controller();
  pointer_watcher_event_router_->DetachFromCaptureClient(capture_client);
  window_tree_client_->capture_synchronizer()->DetachFromCaptureClient(
      capture_client);

  Shell::DeleteInstance();

  pointer_watcher_event_router_.reset();

  window_tree_client_.reset();
  window_manager_client_ = nullptr;
}

void WindowManager::OnEmbed(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {
  // WindowManager should never see this, instead OnWmNewDisplay() is called.
  NOTREACHED();
}

void WindowManager::OnEmbedRootDestroyed(
    aura::WindowTreeHostMus* window_tree_host) {
  // WindowManager should never see this.
  NOTREACHED();
}

void WindowManager::OnLostConnection(aura::WindowTreeClient* client) {
  DCHECK_EQ(client, window_tree_client_.get());
  if (!lost_connection_callback_.is_null()) {
    base::ResetAndReturn(&lost_connection_callback_).Run();
    return;
  }
  Shutdown();
  // TODO(sky): this case should trigger shutting down WindowManagerApplication
  // too.
}

void WindowManager::OnPointerEventObserved(const ui::PointerEvent& event,
                                           aura::Window* target) {
  pointer_watcher_event_router_->OnPointerEventObserved(event, target);
}

aura::PropertyConverter* WindowManager::GetPropertyConverter() {
  return property_converter_.get();
}

void WindowManager::SetWindowManagerClient(aura::WindowManagerClient* client) {
  window_manager_client_ = client;
  ash::Shell::set_window_manager_client(client);
}

void WindowManager::OnWmConnected() {
  CreateShell();
  if (show_primary_host_on_connect_)
    Shell::GetPrimaryRootWindow()->GetHost()->Show();
  InstallFrameDecorationValues();
}

void WindowManager::OnWmSetBounds(aura::Window* window,
                                  const gfx::Rect& bounds) {
  // TODO(sky): this indirectly sets bounds, which is against what
  // OnWmSetBounds() recommends doing. Remove that restriction, or fix this.
  window->SetBounds(bounds);
}

bool WindowManager::OnWmSetProperty(
    aura::Window* window,
    const std::string& name,
    std::unique_ptr<std::vector<uint8_t>>* new_data) {
  if (property_converter_->IsTransportNameRegistered(name))
    return true;
  DVLOG(1) << "unknown property changed, ignoring " << name;
  return false;
}

void WindowManager::OnWmSetModalType(aura::Window* window, ui::ModalType type) {
  ui::ModalType old_type = window->GetProperty(aura::client::kModalKey);
  if (type == old_type)
    return;

  window->SetProperty(aura::client::kModalKey, type);
  if (type != ui::MODAL_TYPE_SYSTEM && old_type != ui::MODAL_TYPE_SYSTEM)
    return;

  aura::Window* new_parent = wm::GetDefaultParent(window, window->bounds());
  DCHECK(new_parent);
  if (window->parent())
    window->parent()->RemoveChild(window);
  new_parent->AddChild(window);
}

void WindowManager::OnWmSetCanFocus(aura::Window* window, bool can_focus) {
  NonClientFrameController* non_client_frame_controller =
      NonClientFrameController::Get(window);
  if (non_client_frame_controller)
    non_client_frame_controller->set_can_activate(can_focus);
}

aura::Window* WindowManager::OnWmCreateTopLevelWindow(
    ui::mojom::WindowType window_type,
    std::map<std::string, std::vector<uint8_t>>* properties) {
  if (window_type == ui::mojom::WindowType::UNKNOWN) {
    LOG(WARNING) << "Request to create top level of unknown type, failing";
    return nullptr;
  }

  return CreateAndParentTopLevelWindow(this, window_type, properties);
}

void WindowManager::OnWmClientJankinessChanged(
    const std::set<aura::Window*>& client_windows,
    bool janky) {
  for (auto* window : client_windows)
    window->SetProperty(kWindowIsJanky, janky);
}

void WindowManager::OnWmBuildDragImage(const gfx::Point& screen_location,
                                       const SkBitmap& drag_image,
                                       const gfx::Vector2d& drag_image_offset,
                                       ui::mojom::PointerKind source) {
  if (drag_image.isNull())
    return;

  // TODO(erg): Get the right display for this drag image. Right now, none of
  // the drag drop code is multidisplay aware.
  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  // TODO(erg): SkBitmap is the wrong data type for the drag image; we should
  // be passing ImageSkias once http://crbug.com/655874 is implemented.

  ui::DragDropTypes::DragEventSource ui_source =
      source == ui::mojom::PointerKind::MOUSE
          ? ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE
          : ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH;
  std::unique_ptr<DragImageView> drag_view =
      base::MakeUnique<DragImageView>(root_window, ui_source);
  drag_view->SetImage(gfx::ImageSkia::CreateFrom1xBitmap(drag_image));
  gfx::Size size = drag_view->GetPreferredSize();
  gfx::Rect drag_image_bounds(screen_location - drag_image_offset, size);
  drag_view->SetBoundsInScreen(drag_image_bounds);
  drag_view->SetWidgetVisible(true);

  drag_state_ = base::MakeUnique<DragState>();
  drag_state_->view = std::move(drag_view);
  drag_state_->image_offset = drag_image_offset;
}

void WindowManager::OnWmMoveDragImage(const gfx::Point& screen_location) {
  if (drag_state_) {
    drag_state_->view->SetScreenPosition(screen_location -
                                         drag_state_->image_offset);
  }
}

void WindowManager::OnWmDestroyDragImage() {
  drag_state_.reset();
}

void WindowManager::OnWmWillCreateDisplay(const display::Display& display) {
  // Ash connects such that |automatically_create_display_roots| is false, which
  // means this should never be hit.
  NOTREACHED();
}

void WindowManager::OnWmNewDisplay(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
    const display::Display& display) {
  // Ash connects such that |automatically_create_display_roots| is false, which
  // means this should never be hit.
  NOTREACHED();
}

void WindowManager::OnWmDisplayRemoved(
    aura::WindowTreeHostMus* window_tree_host) {
  // Ash connects such that |automatically_create_display_roots| is false, which
  // means this should never be hit.
  NOTREACHED();
}

void WindowManager::OnWmDisplayModified(const display::Display& display) {
  // TODO(sky): this shouldn't be called as we're passing false for
  // |automatically_create_display_roots|, but it currently is. Update mus.
}

void WindowManager::OnWmPerformMoveLoop(
    aura::Window* window,
    ui::mojom::MoveLoopSource source,
    const gfx::Point& cursor_location,
    const base::Callback<void(bool)>& on_done) {
  MoveEventHandler* handler = MoveEventHandler::GetForWindow(window);
  if (!handler) {
    on_done.Run(false);
    return;
  }

  DCHECK(!handler->IsDragInProgress());
  ::wm::WindowMoveSource aura_source =
      source == ui::mojom::MoveLoopSource::MOUSE
          ? ::wm::WINDOW_MOVE_SOURCE_MOUSE
          : ::wm::WINDOW_MOVE_SOURCE_TOUCH;
  handler->AttemptToStartDrag(cursor_location, HTCAPTION, aura_source, on_done);
}

void WindowManager::OnWmCancelMoveLoop(aura::Window* window) {
  MoveEventHandler* handler = MoveEventHandler::GetForWindow(window);
  if (handler)
    handler->RevertDrag();
}

ui::mojom::EventResult WindowManager::OnAccelerator(
    uint32_t id,
    const ui::Event& event,
    std::unordered_map<std::string, std::vector<uint8_t>>* properties) {
  auto iter = accelerator_handlers_.find(GetAcceleratorNamespaceId(id));
  if (iter == accelerator_handlers_.end())
    return ui::mojom::EventResult::HANDLED;

  return iter->second->OnAccelerator(id, event, properties);
}

void WindowManager::OnWmSetClientArea(
    aura::Window* window,
    const gfx::Insets& insets,
    const std::vector<gfx::Rect>& additional_client_areas) {
  NonClientFrameController* non_client_frame_controller =
      NonClientFrameController::Get(window);
  if (!non_client_frame_controller)
    return;
  non_client_frame_controller->SetClientArea(insets, additional_client_areas);
}

bool WindowManager::IsWindowActive(aura::Window* window) {
  return Shell::Get()->activation_client()->GetActiveWindow() == window;
}

void WindowManager::OnWmDeactivateWindow(aura::Window* window) {
  Shell::Get()->activation_client()->DeactivateWindow(window);
}

}  // namespace mus
}  // namespace ash
