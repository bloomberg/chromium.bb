// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_filter.h"
#include "ash/app_list/app_list.h"
#include "ash/ash_switches.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/ime/input_method_event_filter.h"
#include "ash/launcher/launcher.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/tooltips/tooltip_controller.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/compact_layout_manager.h"
#include "ash/wm/compact_status_area_layout_manager.h"
#include "ash/wm/default_container_event_filter.h"
#include "ash/wm/default_container_layout_manager.h"
#include "ash/wm/dialog_frame_view.h"
#include "ash/wm/modal_container_layout_manager.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/root_window_event_filter.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/shadow_controller.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/stacking_controller.h"
#include "ash/wm/status_area_layout_manager.h"
#include "ash/wm/toplevel_layout_manager.h"
#include "ash/wm/toplevel_window_event_filter.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_modality_controller.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using views::Widget;

// Screen width at or below which we automatically start in compact window mode,
// in pixels. Should be at least 1366 pixels, the resolution of ChromeOS ZGB
// device displays, as we traditionally used a single window on those devices.
const int kCompactWindowModeWidthThreshold = 1366;

// Creates each of the special window containers that holds windows of various
// types in the shell UI. They are added to |containers| from back to front in
// the z-index.
void CreateSpecialContainers(aura::Window::Windows* containers,
                             bool is_window_mode_compact) {
  aura::Window* background_container = new aura::Window(NULL);
  background_container->set_id(
      internal::kShellWindowId_DesktopBackgroundContainer);
  containers->push_back(background_container);

  aura::Window* default_container = new aura::Window(NULL);
  // Primary windows in compact mode don't allow drag, so don't use the filter.
  if (!is_window_mode_compact) {
    default_container->SetEventFilter(
        new ToplevelWindowEventFilter(default_container));
  }
  default_container->set_id(internal::kShellWindowId_DefaultContainer);
  containers->push_back(default_container);

  aura::Window* always_on_top_container = new aura::Window(NULL);
  always_on_top_container->SetEventFilter(
      new ToplevelWindowEventFilter(always_on_top_container));
  always_on_top_container->set_id(
      internal::kShellWindowId_AlwaysOnTopContainer);
  containers->push_back(always_on_top_container);

  aura::Window* panel_container = new aura::Window(NULL);
  panel_container->set_id(internal::kShellWindowId_PanelContainer);
  containers->push_back(panel_container);

  aura::Window* launcher_container = new aura::Window(NULL);
  launcher_container->set_id(internal::kShellWindowId_LauncherContainer);
  containers->push_back(launcher_container);

  aura::Window* modal_container = new aura::Window(NULL);
  modal_container->SetEventFilter(
      new ToplevelWindowEventFilter(modal_container));
  modal_container->SetLayoutManager(
      new internal::ModalContainerLayoutManager(modal_container));
  modal_container->set_id(internal::kShellWindowId_ModalContainer);
  containers->push_back(modal_container);

  // TODO(beng): Figure out if we can make this use ModalityEventFilter instead
  //             of stops_event_propagation.
  aura::Window* lock_container = new aura::Window(NULL);
  lock_container->set_stops_event_propagation(true);
  lock_container->set_id(internal::kShellWindowId_LockScreenContainer);
  containers->push_back(lock_container);

  aura::Window* lock_modal_container = new aura::Window(NULL);
  lock_modal_container->SetEventFilter(
      new ToplevelWindowEventFilter(lock_modal_container));
  lock_modal_container->SetLayoutManager(
      new internal::ModalContainerLayoutManager(lock_modal_container));
  lock_modal_container->set_id(internal::kShellWindowId_LockModalContainer);
  containers->push_back(lock_modal_container);

  aura::Window* status_container = new aura::Window(NULL);
  status_container->set_id(internal::kShellWindowId_StatusContainer);
  containers->push_back(status_container);

  aura::Window* menu_container = new aura::Window(NULL);
  menu_container->set_id(internal::kShellWindowId_MenuAndTooltipContainer);
  containers->push_back(menu_container);

  aura::Window* setting_bubble_container = new aura::Window(NULL);
  setting_bubble_container->set_id(
      internal::kShellWindowId_SettingBubbleContainer);
  containers->push_back(setting_bubble_container);
}

}  // namespace

// static
Shell* Shell::instance_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

Shell::Shell(ShellDelegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      accelerator_controller_(new AcceleratorController),
      delegate_(delegate),
      window_mode_(NORMAL_MODE) {
  aura::RootWindow::GetInstance()->SetEventFilter(
      new internal::RootWindowEventFilter);
}

Shell::~Shell() {
  RemoveRootWindowEventFilter(input_method_filter_.get());
  RemoveRootWindowEventFilter(window_modality_controller_.get());
  RemoveRootWindowEventFilter(accelerator_filter_.get());

  // TooltipController needs a valid shell instance. We delete it before
  // deleting the shell |instance_|.
  RemoveRootWindowEventFilter(tooltip_controller_.get());
  aura::client::SetTooltipClient(NULL);

  // The LayoutManagers for the default and status containers talk to
  // ShelfLayoutManager (LayoutManager installed on the launcher container).
  // ShelfLayoutManager has a reference to the launcher widget. To avoid any of
  // these trying to reference launcher after it's deleted we delete them all,
  // then the launcher.
  if (!CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kAuraWorkspaceManager)) {
    ResetLayoutManager(internal::kShellWindowId_DefaultContainer);
  }
  ResetLayoutManager(internal::kShellWindowId_StatusContainer);
  ResetLayoutManager(internal::kShellWindowId_LauncherContainer);
  // Make sure we delete WorkspaceController before launcher is
  // deleted as it has a reference to launcher model.
  workspace_controller_.reset();
  launcher_.reset();

  // Delete containers now so that child windows does not access
  // observers when they are destructed. This has to be after launcher
  // is destructed because launcher closes the widget in its destructor.
  aura::RootWindow* root_window = aura::RootWindow::GetInstance();
  while (!root_window->children().empty()) {
    aura::Window* child = root_window->children()[0];
    delete child;
  }

  tooltip_controller_.reset();

  // These need a valid Shell instance to clean up properly, so explicitly
  // delete them before invalidating the instance.
  drag_drop_controller_.reset();
  window_cycle_controller_.reset();

  DCHECK(instance_ == this);
  instance_ = NULL;
}

// static
Shell* Shell::CreateInstance(ShellDelegate* delegate) {
  CHECK(!instance_);
  instance_ = new Shell(delegate);
  instance_->Init();
  return instance_;
}

// static
Shell* Shell::GetInstance() {
  DCHECK(instance_);
  return instance_;
}

// static
void Shell::DeleteInstance() {
  delete instance_;
  instance_ = NULL;
}

void Shell::Init() {
  // On small screens we automatically enable --aura-window-mode=compact if the
  // user has not explicitly set a window mode flag. This must happen before
  // we create containers or layout managers.
  gfx::Size monitor_size = gfx::Screen::GetPrimaryMonitorSize();
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  window_mode_ = ComputeWindowMode(monitor_size, command_line);

  aura::RootWindow* root_window = aura::RootWindow::GetInstance();
  root_window->SetCursor(aura::kCursorPointer);

  activation_controller_.reset(new internal::ActivationController);

  aura::Window::Windows containers;
  CreateSpecialContainers(&containers, IsWindowModeCompact());
  aura::Window::Windows::const_iterator i;
  for (i = containers.begin(); i != containers.end(); ++i) {
    (*i)->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    root_window->AddChild(*i);
    (*i)->Show();
  }

  stacking_controller_.reset(new internal::StackingController);

  InitLayoutManagers(root_window);

  if (!command_line->HasSwitch(switches::kAuraNoShadows))
    shadow_controller_.reset(new internal::ShadowController());

  // Force a layout.
  root_window->layout_manager()->OnWindowResized();

  // InputMethodEventFilter must be added first since it has the highest
  // priority.
  DCHECK(!GetRootWindowEventFilterCount());
  input_method_filter_.reset(new internal::InputMethodEventFilter);
  AddRootWindowEventFilter(input_method_filter_.get());

  window_modality_controller_.reset(new internal::WindowModalityController);
  AddRootWindowEventFilter(window_modality_controller_.get());

  accelerator_filter_.reset(new internal::AcceleratorFilter);
  AddRootWindowEventFilter(accelerator_filter_.get());

  tooltip_controller_.reset(new internal::TooltipController);
  AddRootWindowEventFilter(tooltip_controller_.get());
  aura::client::SetTooltipClient(tooltip_controller_.get());

  drag_drop_controller_.reset(new internal::DragDropController);
  power_button_controller_.reset(new PowerButtonController);
  window_cycle_controller_.reset(new WindowCycleController);
}

Shell::WindowMode Shell::ComputeWindowMode(const gfx::Size& monitor_size,
                                           CommandLine* command_line) const {
  // If user set the flag, use their desired behavior.
  if (command_line->HasSwitch(switches::kAuraWindowMode)) {
    std::string mode =
        command_line->GetSwitchValueASCII(switches::kAuraWindowMode);
    if (mode == switches::kAuraWindowModeNormal)
      return NORMAL_MODE;
    if (mode == switches::kAuraWindowModeCompact)
      return COMPACT_MODE;
  }

  // Developers often run the Aura shell in small windows on their desktop.
  // Prefer normal mode for them.
  if (!aura::RootWindow::use_fullscreen_host_window())
    return NORMAL_MODE;

  // If the screen is narrow we prefer a single compact window display.
  // We explicitly don't care about height, since users don't generally stack
  // browser windows vertically.
  if (monitor_size.width() <= kCompactWindowModeWidthThreshold)
    return COMPACT_MODE;

  return NORMAL_MODE;
}

void Shell::InitLayoutManagers(aura::RootWindow* root_window) {
  internal::RootWindowLayoutManager* root_window_layout =
      new internal::RootWindowLayoutManager(root_window);
  root_window->SetLayoutManager(root_window_layout);

  views::Widget* status_widget = NULL;
  if (delegate_.get())
    status_widget = delegate_->CreateStatusArea();
  if (!status_widget)
    status_widget = internal::CreateStatusArea();

  aura::Window* default_container =
      GetContainer(internal::kShellWindowId_DefaultContainer);
  launcher_.reset(new Launcher(default_container));

  // Compact mode has a simplified layout manager and doesn't use the shelf,
  // desktop background, etc.  The launcher still exists so we can use its
  // data model and list of open windows, but we hide the UI to save space.
  if (IsWindowModeCompact()) {
    default_container->SetLayoutManager(
        new internal::CompactLayoutManager(status_widget));
    internal::CompactStatusAreaLayoutManager* status_area_layout_manager =
        new internal::CompactStatusAreaLayoutManager(status_widget);
    GetContainer(internal::kShellWindowId_StatusContainer)->
        SetLayoutManager(status_area_layout_manager);
    launcher_->widget()->Hide();
    return;
  }

  root_window_layout->set_background_widget(
      internal::CreateDesktopBackground());

  internal::ShelfLayoutManager* shelf_layout_manager =
      new internal::ShelfLayoutManager(launcher_->widget(), status_widget);
  GetContainer(internal::kShellWindowId_LauncherContainer)->
      SetLayoutManager(shelf_layout_manager);

  internal::StatusAreaLayoutManager* status_area_layout_manager =
      new internal::StatusAreaLayoutManager(shelf_layout_manager);
  GetContainer(internal::kShellWindowId_StatusContainer)->
      SetLayoutManager(status_area_layout_manager);

  // Workspace manager has its own layout managers.
  if (CommandLine::ForCurrentProcess()->
          HasSwitch(switches::kAuraWorkspaceManager)) {
    EnableWorkspaceManager();
    return;
  }

  // Default layout manager.
  internal::ToplevelLayoutManager* toplevel_layout_manager =
      new internal::ToplevelLayoutManager();
  default_container->SetLayoutManager(toplevel_layout_manager);
  toplevel_layout_manager->set_shelf(shelf_layout_manager);
}

aura::Window* Shell::GetContainer(int container_id) {
  return const_cast<aura::Window*>(
      const_cast<const Shell*>(this)->GetContainer(container_id));
}

const aura::Window* Shell::GetContainer(int container_id) const {
  return aura::RootWindow::GetInstance()->GetChildById(container_id);
}

void Shell::AddRootWindowEventFilter(aura::EventFilter* filter) {
  static_cast<internal::RootWindowEventFilter*>(
      aura::RootWindow::GetInstance()->event_filter())->AddFilter(filter);
}

void Shell::RemoveRootWindowEventFilter(aura::EventFilter* filter) {
  static_cast<internal::RootWindowEventFilter*>(
      aura::RootWindow::GetInstance()->event_filter())->RemoveFilter(filter);
}

size_t Shell::GetRootWindowEventFilterCount() const {
  return static_cast<internal::RootWindowEventFilter*>(
      aura::RootWindow::GetInstance()->event_filter())->GetFilterCount();
}

void Shell::ToggleOverview() {
  if (workspace_controller_.get())
    workspace_controller_->ToggleOverview();
}

void Shell::ToggleAppList() {
  if (!app_list_.get())
    app_list_.reset(new internal::AppList);
  app_list_->SetVisible(!app_list_->IsVisible());
}

bool Shell::IsScreenLocked() const {
  const aura::Window* lock_screen_container = GetContainer(
      internal::kShellWindowId_LockScreenContainer);
  return lock_screen_container->StopsEventPropagation();
}

bool Shell::IsModalWindowOpen() const {
  aura::Window* modal_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_AlwaysOnTopContainer);
  return !modal_container->children().empty();
}

views::NonClientFrameView* Shell::CreateDefaultNonClientFrameView(
    views::Widget* widget) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAuraGoogleDialogFrames)) {
    return new internal::DialogFrameView;
  }
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

void Shell::EnableWorkspaceManager() {
  aura::Window* default_container =
      GetContainer(internal::kShellWindowId_DefaultContainer);

  workspace_controller_.reset(
      new internal::WorkspaceController(default_container));
  workspace_controller_->SetLauncherModel(launcher_->model());
  default_container->SetEventFilter(
      new internal::DefaultContainerEventFilter(default_container));
  default_container->SetLayoutManager(
      new internal::DefaultContainerLayoutManager(
          workspace_controller_->workspace_manager()));
}

void Shell::ResetLayoutManager(int container_id) {
  GetContainer(container_id)->SetLayoutManager(NULL);
}

}  // namespace ash
