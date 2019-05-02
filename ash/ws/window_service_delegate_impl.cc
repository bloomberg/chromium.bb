// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/window_service_delegate_impl.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/ime/ime_engine_factory_registry.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/non_client_frame_controller.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/top_level_window_factory.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_finder.h"
#include "ash/wm/window_util.h"
#include "ash/ws/ash_window_manager.h"
#include "base/bind.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/hit_test.h"
#include "ui/events/system_input_injector.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// Function supplied to ToplevelWindowEventHandler::AttemptToStartDrag().
// |end_closure| is the callback that was supplied to RunWindowMoveLoop().
void OnMoveLoopCompleted(base::OnceCallback<void(bool success)> end_closure,
                         ToplevelWindowEventHandler::DragResult result) {
  std::move(end_closure)
      .Run(result == ToplevelWindowEventHandler::DragResult::SUCCESS);
}

// Returns true if there is a drag and drop in progress.
bool InDragLoop(aura::Window* window) {
  aura::client::DragDropClient* drag_drop_client =
      aura::client::GetDragDropClient(window->GetRootWindow());
  return drag_drop_client && drag_drop_client->IsDragDropInProgress();
}

// Returns true if there is a window move loop in progress.
bool InWindowMoveLoop() {
  return Shell::Get()
      ->toplevel_window_event_handler()
      ->is_drag_in_progress();
}

// Returns true if a window move loop can be started.
bool ShouldStartMoveLoop(aura::Window* window) {
  // A window move can only be started when there is no in progress drag loop or
  // window move loop.
  return !InDragLoop(window) && !InWindowMoveLoop();
}

// Returns true if a drag loop can be started.
bool ShouldStartDragLoop(aura::Window* window) {
  // A drag loop can only be started when there is no in progress drag loop or
  // window move loop.
  return !InDragLoop(window) && !InWindowMoveLoop();
}

}  // namespace

WindowServiceDelegateImpl::WindowServiceDelegateImpl() = default;

WindowServiceDelegateImpl::~WindowServiceDelegateImpl() = default;

std::unique_ptr<aura::Window> WindowServiceDelegateImpl::NewTopLevel(
    ws::TopLevelProxyWindow* top_level_proxy_window,
    aura::PropertyConverter* property_converter,
    const base::flat_map<std::string, std::vector<uint8_t>>& properties) {
  std::map<std::string, std::vector<uint8_t>> property_map =
      mojo::FlatMapToMap(properties);
  ws::mojom::WindowType window_type =
      aura::GetWindowTypeFromProperties(property_map);

  auto* window = CreateAndParentTopLevelWindow(
      top_level_proxy_window, window_type, property_converter, &property_map);
  return base::WrapUnique<aura::Window>(window);
}

void WindowServiceDelegateImpl::OnUnhandledKeyEvent(
    const ui::KeyEvent& key_event) {
  Shell::Get()->accelerator_controller()->Process(ui::Accelerator(key_event));
}

bool WindowServiceDelegateImpl::StoreAndSetCursor(aura::Window* window,
                                                  ui::Cursor cursor) {
  auto* frame = NonClientFrameController::Get(window);
  if (frame)
    frame->StoreCursor(cursor);

  ash::Shell::Get()->env_filter()->SetCursorForWindow(window, cursor);

  return !!frame;
}

void WindowServiceDelegateImpl::RunWindowMoveLoop(
    aura::Window* window,
    ws::mojom::MoveLoopSource source,
    const gfx::Point& cursor,
    int window_component,
    DoneCallback callback) {
  if (!ShouldStartMoveLoop(window)) {
    std::move(callback).Run(false);
    return;
  }

  if (source == ws::mojom::MoveLoopSource::MOUSE)
    window->SetCapture();

  const ::wm::WindowMoveSource aura_source =
      source == ws::mojom::MoveLoopSource::MOUSE
          ? ::wm::WINDOW_MOVE_SOURCE_MOUSE
          : ::wm::WINDOW_MOVE_SOURCE_TOUCH;

  gfx::Point location_in_parent = cursor;
  ::wm::ConvertPointFromScreen(window->parent(), &location_in_parent);

  Shell::Get()
      ->toplevel_window_event_handler()
      ->AttemptToStartDrag(
          window, location_in_parent, window_component, aura_source,
          base::BindOnce(&OnMoveLoopCompleted, std::move(callback)),
          /*update_gesture_target=*/false);
}

void WindowServiceDelegateImpl::CancelWindowMoveLoop() {
  Shell::Get()
      ->toplevel_window_event_handler()
      ->RevertDrag();
}

void WindowServiceDelegateImpl::RunDragLoop(
    aura::Window* window,
    const ui::OSExchangeData& data,
    const gfx::Point& screen_location,
    uint32_t drag_operation,
    ui::DragDropTypes::DragEventSource source,
    DragDropCompletedCallback callback) {
  if (!ShouldStartDragLoop(window)) {
    std::move(callback).Run(ui::DragDropTypes::DRAG_NONE);
    return;
  }

  aura::Window* const root_window = window->GetRootWindow();
  aura::client::DragDropClient* drag_drop_client =
      aura::client::GetDragDropClient(root_window);
  DCHECK(drag_drop_client);

  std::move(callback).Run(drag_drop_client->StartDragAndDrop(
      data, root_window, window, screen_location, drag_operation, source));
}

void WindowServiceDelegateImpl::CancelDragLoop(aura::Window* window) {
  if (!InDragLoop(window))
    return;

  aura::client::GetDragDropClient(window->GetRootWindow())->DragCancel();
}

void WindowServiceDelegateImpl::SetWindowResizeShadow(aura::Window* window,
                                                      int hit_test) {
  ResizeShadowController* controller = Shell::Get()->resize_shadow_controller();
  if (hit_test == HTNOWHERE)
    controller->HideShadow(window);
  else
    controller->ShowShadow(window, hit_test);
}

void WindowServiceDelegateImpl::UpdateTextInputState(
    aura::Window* window,
    ui::mojom::TextInputStatePtr state) {
  if (!wm::IsActiveWindow(window))
    return;

  RootWindowController::ForWindow(window)->ash_host()->UpdateTextInputState(
      std::move(state));
}

void WindowServiceDelegateImpl::UpdateImeVisibility(
    aura::Window* window,
    bool visible,
    ui::mojom::TextInputStatePtr state) {
  if (!wm::IsActiveWindow(window))
    return;

  RootWindowController::ForWindow(window)->ash_host()->UpdateImeVisibility(
      visible, std::move(state));
}

void WindowServiceDelegateImpl::SetModalType(aura::Window* window,
                                             ui::ModalType type) {
  const ui::ModalType old_type = window->GetProperty(aura::client::kModalKey);
  if (old_type == type)
    return;

  window->SetProperty(aura::client::kModalKey, type);

  // Reparent the window if it will become, or will no longer be, system modal.
  if (type == ui::MODAL_TYPE_SYSTEM || old_type == ui::MODAL_TYPE_SYSTEM) {
    aura::Window* const parent =
        wm::GetDefaultParent(window, window->GetBoundsInScreen());
    if (parent != window->parent())
      parent->AddChild(window);
  }
}

ui::SystemInputInjector* WindowServiceDelegateImpl::GetSystemInputInjector() {
  if (!system_input_injector_) {
    system_input_injector_ =
        ui::OzonePlatform::GetInstance()->CreateSystemInputInjector();
  }
  return system_input_injector_.get();
}

ui::EventTarget* WindowServiceDelegateImpl::GetGlobalEventTarget() {
  return Shell::Get();
}

aura::Window* WindowServiceDelegateImpl::GetRootWindowForDisplayId(
    int64_t display_id) {
  return Shell::Get()->GetRootWindowForDisplayId(display_id);
}

aura::Window* WindowServiceDelegateImpl::GetTopmostWindowAtPoint(
    const gfx::Point& location_in_screen,
    const std::set<aura::Window*>& ignore,
    aura::Window** real_topmost) {
  return wm::GetTopmostWindowAtPoint(location_in_screen, ignore, real_topmost);
}

std::unique_ptr<ws::WindowManagerInterface>
WindowServiceDelegateImpl::CreateWindowManagerInterface(
    ws::WindowTree* tree,
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (name == mojom::AshWindowManager::Name_)
    return std::make_unique<AshWindowManager>(tree, std::move(handle));
  return nullptr;
}

void WindowServiceDelegateImpl::ConnectToImeEngine(
    ime::mojom::ImeEngineRequest engine_request,
    ime::mojom::ImeEngineClientPtr client) {
  Shell::Get()->ime_engine_factory_registry()->ConnectToImeEngine(
      std::move(engine_request), std::move(client));
}

}  // namespace ash
