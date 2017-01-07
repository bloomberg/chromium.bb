// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/window_manager.h"

#include <stdint.h>

#include <utility>

#include "ash/common/session/session_controller.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/window_state.h"
#include "ash/display/screen_position_controller.h"
#include "ash/mus/accelerators/accelerator_handler.h"
#include "ash/mus/accelerators/accelerator_ids.h"
#include "ash/mus/bridge/wm_lookup_mus.h"
#include "ash/mus/bridge/wm_root_window_controller_mus.h"
#include "ash/mus/bridge/wm_shell_mus.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/move_event_handler.h"
#include "ash/mus/non_client_frame_controller.h"
#include "ash/mus/property_util.h"
#include "ash/mus/root_window_controller.h"
#include "ash/mus/screen_mus.h"
#include "ash/mus/shadow_controller.h"
#include "ash/mus/shell_delegate_mus.h"
#include "ash/mus/window_manager_observer.h"
#include "ash/mus/window_properties.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/ash_focus_rules.h"
#include "ash/wm/event_client_impl.h"
#include "ash/wm/window_properties.h"
#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/common/accelerator_util.h"
#include "services/ui/common/types.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/base/hit_test.h"
#include "ui/display/display_observer.h"
#include "ui/events/mojo/event.mojom.h"
#include "ui/views/mus/pointer_watcher_event_router.h"
#include "ui/views/mus/screen_mus.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/wm_state.h"

namespace ash {
namespace mus {
namespace {

// TODO: this is temporary until WindowManager create the real Shell (which is
// the event target). http://crbug.com/670744.
class EventClientImplMus : public EventClientImpl {
 public:
  EventClientImplMus() = default;
  ~EventClientImplMus() override = default;

  // EventClientImpl:
  ui::EventTarget* GetToplevelEventTarget() override { return nullptr; }

 private:
  DISALLOW_COPY_AND_ASSIGN(EventClientImplMus);
};

}  // namespace

// TODO: need to register OSExchangeDataProviderMus. http://crbug.com/665077.
WindowManager::WindowManager(service_manager::Connector* connector)
    : connector_(connector),
      focus_controller_(base::MakeUnique<::wm::FocusController>(
          new ::ash::wm::AshFocusRules())),
      wm_state_(base::MakeUnique<::wm::WMState>()),
      property_converter_(base::MakeUnique<aura::PropertyConverter>()),
      event_client_(base::MakeUnique<EventClientImplMus>()),
      screen_position_controller_(
          base::MakeUnique<ScreenPositionController>()) {
  property_converter_->RegisterProperty(
      kPanelAttachedKey, ui::mojom::WindowManager::kPanelAttached_Property);
  property_converter_->RegisterProperty(
      kRenderTitleAreaProperty,
      ui::mojom::WindowManager::kRenderParentTitleArea_Property);
  property_converter_->RegisterProperty(
      kShelfItemTypeKey, ui::mojom::WindowManager::kShelfItemType_Property);
}

WindowManager::~WindowManager() {
  Shutdown();
  aura::Env::GetInstance()->RemoveObserver(this);
}

void WindowManager::Init(
    std::unique_ptr<aura::WindowTreeClient> window_tree_client,
    const scoped_refptr<base::SequencedWorkerPool>& blocking_pool) {
  DCHECK(window_manager_client_);
  DCHECK(!window_tree_client_);
  window_tree_client_ = std::move(window_tree_client);

  aura::Env::GetInstance()->AddObserver(this);

  // |connector_| will be null in some tests.
  if (connector_)
    connector_->BindInterface(ui::mojom::kServiceName, &display_controller_);

  screen_ = base::MakeUnique<ScreenMus>();
  display::Screen::SetScreenInstance(screen_.get());

  pointer_watcher_event_router_ =
      base::MakeUnique<views::PointerWatcherEventRouter>(
          window_tree_client_.get());

  shadow_controller_ = base::MakeUnique<ShadowController>();

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

  shell_.reset(new WmShellMus(base::MakeUnique<ShellDelegateMus>(connector_),
                              this, pointer_watcher_event_router_.get()));
  shell_->Initialize(blocking_pool);
  lookup_.reset(new WmLookupMus);
}

aura::Window* WindowManager::NewTopLevelWindow(
    ui::mojom::WindowType window_type,
    std::map<std::string, std::vector<uint8_t>>* properties) {
  RootWindowController* root_window_controller =
      GetRootWindowControllerForNewTopLevelWindow(properties);
  aura::Window* window =
      root_window_controller->NewTopLevelWindow(window_type, properties);
  if (properties->count(
          ui::mojom::WindowManager::kWindowIgnoredByShelf_Property)) {
    wm::WindowState* window_state =
        static_cast<WmWindow*>(WmWindowMus::Get(window))->GetWindowState();
    window_state->set_ignored_by_shelf(mojo::ConvertTo<bool>(
        (*properties)
            [ui::mojom::WindowManager::kWindowIgnoredByShelf_Property]));
    // No need to persist this value.
    properties->erase(ui::mojom::WindowManager::kWindowIgnoredByShelf_Property);
  }
  return window;
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

void WindowManager::AddObserver(WindowManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowManager::RemoveObserver(WindowManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

display::mojom::DisplayController* WindowManager::GetDisplayController() {
  return display_controller_ ? display_controller_.get() : nullptr;
}

RootWindowController* WindowManager::CreateRootWindowController(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
    const display::Display& display) {
  // TODO(sky): this is temporary, should use RootWindowController directly.
  aura::client::SetCaptureClient(window_tree_host->window(),
                                 wm_state_->capture_controller());
  aura::client::SetFocusClient(window_tree_host->window(),
                               focus_controller_.get());
  aura::client::SetActivationClient(window_tree_host->window(),
                                    focus_controller_.get());
  aura::client::SetEventClient(window_tree_host->window(), event_client_.get());
  aura::client::SetScreenPositionClient(window_tree_host->window(),
                                        screen_position_controller_.get());

  std::unique_ptr<RootWindowController> root_window_controller_ptr(
      new RootWindowController(this, std::move(window_tree_host), display));
  RootWindowController* root_window_controller =
      root_window_controller_ptr.get();
  root_window_controllers_.insert(std::move(root_window_controller_ptr));

  // Create a shelf if a user is already logged in.
  if (shell_->session_controller()->NumberOfLoggedInUsers())
    root_window_controller->wm_root_window_controller()->CreateShelf();

  for (auto& observer : observers_)
    observer.OnRootWindowControllerAdded(root_window_controller);

  for (auto& observer : *screen_->display_list().observers())
    observer.OnDisplayAdded(root_window_controller->display());

  return root_window_controller;
}

void WindowManager::DestroyRootWindowController(
    RootWindowController* root_window_controller) {
  if (root_window_controllers_.size() > 1) {
    DCHECK_NE(root_window_controller, GetPrimaryRootWindowController());
    root_window_controller->wm_root_window_controller()->MoveWindowsTo(
        WmWindowMus::Get(GetPrimaryRootWindowController()->root()));
  }

  root_window_controller->Shutdown();

  // NOTE: classic ash deleted RootWindowController after a delay (DeleteSoon())
  // this may need to change to mirror that.
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

  // Remove the focus from any window. This will prevent overhead and side
  // effects (e.g. crashes) from changing focus during shutdown.
  // See bug crbug.com/134502.
  static_cast<aura::client::FocusClient*>(focus_controller_.get())
      ->FocusWindow(nullptr);

  // Observers can rely on WmShell from the callback. So notify the observers
  // before destroying it.
  for (auto& observer : observers_)
    observer.OnWindowTreeClientDestroyed();

  // Primary RootWindowController must be destroyed last.
  RootWindowController* primary_root_window_controller =
      GetPrimaryRootWindowController();
  std::set<RootWindowController*> secondary_root_window_controllers;
  for (auto& root_window_controller_ptr : root_window_controllers_) {
    if (root_window_controller_ptr.get() != primary_root_window_controller)
      secondary_root_window_controllers.insert(
          root_window_controller_ptr.get());
  }
  for (RootWindowController* root_window_controller :
       secondary_root_window_controllers) {
    DestroyRootWindowController(root_window_controller);
  }
  if (primary_root_window_controller)
    DestroyRootWindowController(primary_root_window_controller);

  DCHECK(root_window_controllers_.empty());

  lookup_.reset();
  shell_->Shutdown();
  shell_.reset();
  shadow_controller_.reset();

  pointer_watcher_event_router_.reset();

  window_tree_client_.reset();
  window_manager_client_ = nullptr;

  DCHECK_EQ(screen_.get(), display::Screen::GetScreen());
  display::Screen::SetScreenInstance(nullptr);
}

RootWindowController* WindowManager::GetPrimaryRootWindowController() {
  return static_cast<WmRootWindowControllerMus*>(
             WmShell::Get()->GetPrimaryRootWindowController())
      ->root_window_controller();
}

RootWindowController*
WindowManager::GetRootWindowControllerForNewTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  // If a specific display was requested, use it.
  const int64_t display_id = GetInitialDisplayId(*properties);
  for (auto& root_window_controller_ptr : root_window_controllers_) {
    if (root_window_controller_ptr->display().id() == display_id)
      return root_window_controller_ptr.get();
  }

  return static_cast<WmRootWindowControllerMus*>(
             WmShellMus::Get()
                 ->GetRootWindowForNewWindows()
                 ->GetRootWindowController())
      ->root_window_controller();
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

aura::client::CaptureClient* WindowManager::GetCaptureClient() {
  return wm_state_->capture_controller();
}

aura::PropertyConverter* WindowManager::GetPropertyConverter() {
  return property_converter_.get();
}

void WindowManager::SetWindowManagerClient(aura::WindowManagerClient* client) {
  window_manager_client_ = client;
}

bool WindowManager::OnWmSetBounds(aura::Window* window, gfx::Rect* bounds) {
  // TODO(sky): this indirectly sets bounds, which is against what
  // OnWmSetBounds() recommends doing. Remove that restriction, or fix this.
  WmWindowMus::Get(window)->SetBounds(*bounds);
  *bounds = window->bounds();
  return true;
}

bool WindowManager::OnWmSetProperty(
    aura::Window* window,
    const std::string& name,
    std::unique_ptr<std::vector<uint8_t>>* new_data) {
  // TODO(sky): constrain this to set of keys we know about, and allowed values.
  if (name == ui::mojom::WindowManager::kWindowIgnoredByShelf_Property) {
    wm::WindowState* window_state =
        static_cast<WmWindow*>(WmWindowMus::Get(window))->GetWindowState();
    window_state->set_ignored_by_shelf(
        new_data ? mojo::ConvertTo<bool>(**new_data) : false);
    return false;  // Won't attempt to map through property converter.
  }
  return name == ui::mojom::WindowManager::kAppIcon_Property ||
         name == ui::mojom::WindowManager::kShowState_Property ||
         name == ui::mojom::WindowManager::kPreferredSize_Property ||
         name == ui::mojom::WindowManager::kResizeBehavior_Property ||
         name == ui::mojom::WindowManager::kShelfItemType_Property ||
         name == ui::mojom::WindowManager::kWindowIcon_Property ||
         name == ui::mojom::WindowManager::kWindowTitle_Property;
}

aura::Window* WindowManager::OnWmCreateTopLevelWindow(
    ui::mojom::WindowType window_type,
    std::map<std::string, std::vector<uint8_t>>* properties) {
  if (window_type == ui::mojom::WindowType::UNKNOWN) {
    LOG(WARNING) << "Request to create top level of unknown type, failing";
    return nullptr;
  }

  return NewTopLevelWindow(window_type, properties);
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
  CreateRootWindowController(std::move(window_tree_host), display);
}

void WindowManager::OnWmDisplayRemoved(
    aura::WindowTreeHostMus* window_tree_host) {
  for (auto& root_window_controller_ptr : root_window_controllers_) {
    if (root_window_controller_ptr->window_tree_host() == window_tree_host) {
      DestroyRootWindowController(root_window_controller_ptr.get());
      break;
    }
  }
}

void WindowManager::OnWmDisplayModified(const display::Display& display) {
  for (auto& controller : root_window_controllers_) {
    if (controller->display().id() == display.id()) {
      controller->SetDisplay(display);
      // The root window will be resized by the window server.
      return;
    }
  }

  NOTREACHED();
}

void WindowManager::OnWmPerformMoveLoop(
    aura::Window* window,
    ui::mojom::MoveLoopSource source,
    const gfx::Point& cursor_location,
    const base::Callback<void(bool)>& on_done) {
  WmWindowMus* child_window = WmWindowMus::Get(window);
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
  WmWindowMus* child_window = WmWindowMus::Get(window);
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

void WindowManager::OnWindowInitialized(aura::Window* window) {
  // This ensures WmWindowAura won't be called before WmWindowMus. This is
  // important as if WmWindowAura::Get() is called first, then WmWindowAura
  // would be created, not WmWindowMus.
  WmWindowMus::Get(window);
}

}  // namespace mus
}  // namespace ash
