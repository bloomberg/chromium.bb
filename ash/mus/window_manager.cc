// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/window_manager.h"

#include <stdint.h>

#include <utility>

#include "ash/common/session/session_controller.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_window.h"
#include "ash/mus/accelerators/accelerator_handler.h"
#include "ash/mus/accelerators/accelerator_ids.h"
#include "ash/mus/bridge/wm_lookup_mus.h"
#include "ash/mus/bridge/wm_shell_mus.h"
#include "ash/mus/move_event_handler.h"
#include "ash/mus/non_client_frame_controller.h"
#include "ash/mus/property_util.h"
#include "ash/mus/screen_mus.h"
#include "ash/mus/shell_delegate_mus.h"
#include "ash/mus/top_level_window_factory.h"
#include "ash/mus/window_properties.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/wm/ash_focus_rules.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/common/accelerator_util.h"
#include "services/ui/common/types.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
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
#include "ui/views/mus/screen_mus.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/wm_state.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace mus {

// TODO: need to register OSExchangeDataProviderMus. http://crbug.com/665077.
WindowManager::WindowManager(service_manager::Connector* connector)
    : connector_(connector),
      wm_state_(base::MakeUnique<::wm::WMState>()),
      property_converter_(base::MakeUnique<aura::PropertyConverter>()) {
  property_converter_->RegisterProperty(
      kPanelAttachedKey, ui::mojom::WindowManager::kPanelAttached_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter_->RegisterProperty(
      kRenderTitleAreaProperty,
      ui::mojom::WindowManager::kRenderParentTitleArea_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
  property_converter_->RegisterProperty(
      kShelfItemTypeKey, ui::mojom::WindowManager::kShelfItemType_Property,
      base::Bind(&IsValidShelfItemType));
  property_converter_->RegisterProperty(
      ::wm::kShadowElevationKey,
      ui::mojom::WindowManager::kShadowElevation_Property,
      base::Bind(&::wm::IsValidShadowElevation));
}

WindowManager::~WindowManager() {
  Shutdown();
  ash::Shell::set_window_tree_client(nullptr);
  ash::Shell::set_window_manager_client(nullptr);
}

void WindowManager::Init(
    std::unique_ptr<aura::WindowTreeClient> window_tree_client,
    const scoped_refptr<base::SequencedWorkerPool>& blocking_pool) {
  blocking_pool_ = blocking_pool;
  DCHECK(window_manager_client_);
  DCHECK(!window_tree_client_);
  window_tree_client_ = std::move(window_tree_client);

  DCHECK_EQ(nullptr, ash::Shell::window_tree_client());
  ash::Shell::set_window_tree_client(window_tree_client_.get());

  // |connector_| will be null in some tests.
  if (connector_)
    connector_->BindInterface(ui::mojom::kServiceName, &display_controller_);

  screen_ = base::MakeUnique<ScreenMus>(display_controller_.get());
  display::Screen::SetScreenInstance(screen_.get());

  pointer_watcher_event_router_ =
      base::MakeUnique<views::PointerWatcherEventRouter>(
          window_tree_client_.get());

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

  lookup_.reset(new WmLookupMus);

  // Notify PointerWatcherEventRouter and CaptureSynchronizer that the capture
  // client has been set.
  aura::client::CaptureClient* capture_client = wm_state_->capture_controller();
  pointer_watcher_event_router_->AttachToCaptureClient(capture_client);
  window_tree_client_->capture_synchronizer()->AttachToCaptureClient(
      capture_client);
}

void WindowManager::DeleteAllRootWindowControllers() {
  // Primary RootWindowController must be destroyed last.
  RootWindowController* primary_root_window_controller =
      GetPrimaryRootWindowController();
  std::set<RootWindowController*> secondary_root_window_controllers;
  for (auto& root_window_controller_ptr : root_window_controllers_) {
    if (root_window_controller_ptr.get() != primary_root_window_controller) {
      secondary_root_window_controllers.insert(
          root_window_controller_ptr.get());
    }
  }
  const bool in_shutdown = true;
  for (RootWindowController* root_window_controller :
       secondary_root_window_controllers) {
    DestroyRootWindowController(root_window_controller, in_shutdown);
  }
  if (primary_root_window_controller)
    DestroyRootWindowController(primary_root_window_controller, in_shutdown);
  DCHECK(root_window_controllers_.empty());
}

std::set<RootWindowController*> WindowManager::GetRootWindowControllers() {
  std::set<RootWindowController*> result;
  for (auto& root_window_controller : root_window_controllers_)
    result.insert(root_window_controller.get());
  return result;
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

void WindowManager::CreatePrimaryRootWindowController(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {
  // See comment in CreateRootWindowController().
  DCHECK(created_shell_);
  CreateAndRegisterRootWindowController(
      std::move(window_tree_host), screen_->GetAllDisplays()[0],
      RootWindowController::RootWindowType::PRIMARY);
}

void WindowManager::CreateShell(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {
  DCHECK(!created_shell_);
  created_shell_ = true;
  ShellInitParams init_params;
  WmShellMus* wm_shell = new WmShellMus(
      WmWindow::Get(window_tree_host->window()),
      shell_delegate_for_test_ ? std::move(shell_delegate_for_test_)
                               : base::MakeUnique<ShellDelegateMus>(connector_),
      this, pointer_watcher_event_router_.get(),
      create_session_state_delegate_stub_for_test_);
  init_params.primary_window_tree_host = window_tree_host.release();
  init_params.wm_shell = wm_shell;
  init_params.blocking_pool = blocking_pool_.get();
  Shell::CreateInstance(init_params);
}

void WindowManager::CreateAndRegisterRootWindowController(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
    const display::Display& display,
    RootWindowController::RootWindowType root_window_type) {
  RootWindowSettings* root_window_settings =
      InitRootWindowSettings(window_tree_host->window());
  root_window_settings->display_id = display.id();
  std::unique_ptr<RootWindowController> root_window_controller(
      new RootWindowController(nullptr, window_tree_host.release()));
  root_window_controller->Init(root_window_type);
  // TODO: To avoid lots of IPC AddActivationParent() should take an array.
  // http://crbug.com/682048.
  WmWindow* root_window = root_window_controller->GetWindow();
  for (size_t i = 0; i < kNumActivatableShellWindowIds; ++i) {
    window_manager_client_->AddActivationParent(
        root_window->GetChildByShellWindowId(kActivatableShellWindowIds[i])
            ->aura_window());
  }
  root_window_controllers_.insert(std::move(root_window_controller));
}

void WindowManager::DestroyRootWindowController(
    RootWindowController* root_window_controller,
    bool in_shutdown) {
  if (!in_shutdown && root_window_controllers_.size() > 1) {
    DCHECK_NE(root_window_controller, GetPrimaryRootWindowController());
    root_window_controller->MoveWindowsTo(
        GetPrimaryRootWindowController()->GetRootWindow());
  }

  root_window_controller->Shutdown();

  for (auto iter = root_window_controllers_.begin();
       iter != root_window_controllers_.end(); ++iter) {
    if (iter->get() == root_window_controller) {
      root_window_controllers_.erase(iter);
      break;
    }
  }
}

void WindowManager::Shutdown() {
  if (!window_tree_client_)
    return;

  aura::client::CaptureClient* capture_client = wm_state_->capture_controller();
  pointer_watcher_event_router_->DetachFromCaptureClient(capture_client);
  window_tree_client_->capture_synchronizer()->DetachFromCaptureClient(
      capture_client);

  Shell::DeleteInstance();

  lookup_.reset();

  pointer_watcher_event_router_.reset();

  window_tree_client_.reset();
  window_manager_client_ = nullptr;
}

RootWindowController* WindowManager::GetPrimaryRootWindowController() {
  return RootWindowController::ForWindow(WmShell::Get()
                                             ->GetPrimaryRootWindowController()
                                             ->GetWindow()
                                             ->aura_window());
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

bool WindowManager::OnWmSetBounds(aura::Window* window, gfx::Rect* bounds) {
  // TODO(sky): this indirectly sets bounds, which is against what
  // OnWmSetBounds() recommends doing. Remove that restriction, or fix this.
  WmWindow::Get(window)->SetBounds(*bounds);
  *bounds = window->bounds();
  return true;
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

void WindowManager::OnWmWillCreateDisplay(const display::Display& display) {
  // A call to this function means a new display is being added, so the
  // DisplayList needs to be updated. Calling AddDisplay() results in
  // notifying DisplayObservers. Ash code assumes when this happens there is
  // a valid RootWindowController for the new display. Suspend notifying
  // observers, add the Display. The RootWindowController is created in
  // OnWmNewDisplay(), which is called immediately after this function.
  std::unique_ptr<display::DisplayListObserverLock> display_lock =
      screen_->display_list().SuspendObserverUpdates();
  const bool is_first_display = screen_->display_list().displays().empty();
  // TODO(sky): should be passed whether display is primary.
  screen_->display_list().AddDisplay(
      display, is_first_display ? display::DisplayList::Type::PRIMARY
                                : display::DisplayList::Type::NOT_PRIMARY);
}

void WindowManager::OnWmNewDisplay(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
    const display::Display& display) {
  RootWindowController::RootWindowType root_window_type =
      screen_->display_list().displays().size() == 1
          ? RootWindowController::RootWindowType::PRIMARY
          : RootWindowController::RootWindowType::SECONDARY;
  // The ash startup sequence creates the Shell, which creates
  // WindowTreeHostManager, which creates the WindowTreeHosts and
  // RootWindowControllers. For mash we are supplied the WindowTreeHost when
  // a display is added (and WindowTreeHostManager is not used in mash). Mash
  // waits for the first WindowTreeHost, then creates the shell. As there are
  // order dependencies we have to create the RootWindowController at a similar
  // time as cash, to do that we inject the WindowTreeHost into ShellInitParams.
  // Shell calls to WmShell::InitHosts(), which calls back to
  // CreatePrimaryRootWindowController().
  if (!created_shell_) {
    CreateShell(std::move(window_tree_host));
    return;
  }
  CreateAndRegisterRootWindowController(std::move(window_tree_host), display,
                                        root_window_type);

  for (auto& observer : *screen_->display_list().observers())
    observer.OnDisplayAdded(display);
}

void WindowManager::OnWmDisplayRemoved(
    aura::WindowTreeHostMus* window_tree_host) {
  for (auto& root_window_controller_ptr : root_window_controllers_) {
    if (root_window_controller_ptr->GetHost() == window_tree_host) {
      const bool in_shutdown = false;
      DestroyRootWindowController(root_window_controller_ptr.get(),
                                  in_shutdown);
      break;
    }
  }
}

void WindowManager::OnWmDisplayModified(const display::Display& display) {
  screen_->display_list().UpdateDisplay(display);
}

void WindowManager::OnWmPerformMoveLoop(
    aura::Window* window,
    ui::mojom::MoveLoopSource source,
    const gfx::Point& cursor_location,
    const base::Callback<void(bool)>& on_done) {
  WmWindow* child_window = WmWindow::Get(window);
  MoveEventHandler* handler = MoveEventHandler::GetForWindow(child_window);
  if (!handler) {
    on_done.Run(false);
    return;
  }

  DCHECK(!handler->IsDragInProgress());
  aura::client::WindowMoveSource aura_source =
      source == ui::mojom::MoveLoopSource::MOUSE
          ? aura::client::WINDOW_MOVE_SOURCE_MOUSE
          : aura::client::WINDOW_MOVE_SOURCE_TOUCH;
  handler->AttemptToStartDrag(cursor_location, HTCAPTION, aura_source, on_done);
}

void WindowManager::OnWmCancelMoveLoop(aura::Window* window) {
  WmWindow* child_window = WmWindow::Get(window);
  MoveEventHandler* handler = MoveEventHandler::GetForWindow(child_window);
  if (handler)
    handler->RevertDrag();
}

ui::mojom::EventResult WindowManager::OnAccelerator(uint32_t id,
                                                    const ui::Event& event) {
  auto iter = accelerator_handlers_.find(GetAcceleratorNamespaceId(id));
  if (iter == accelerator_handlers_.end())
    return ui::mojom::EventResult::HANDLED;

  return iter->second->OnAccelerator(id, event);
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
  return Shell::GetInstance()->activation_client()->GetActiveWindow() == window;
}

void WindowManager::OnWmDeactivateWindow(aura::Window* window) {
  Shell::GetInstance()->activation_client()->DeactivateWindow(window);
}

}  // namespace mus
}  // namespace ash
