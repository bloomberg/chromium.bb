// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>

#include "ash/app_list/app_list.h"
#include "ash/ash_switches.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/focus_cycler.h"
#include "ash/ime/input_method_event_filter.h"
#include "ash/launcher/launcher.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/tooltips/tooltip_controller.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/compact_layout_manager.h"
#include "ash/wm/compact_status_area_layout_manager.h"
#include "ash/wm/dialog_frame_view.h"
#include "ash/wm/panel_window_event_filter.h"
#include "ash/wm/panel_layout_manager.h"
#include "ash/wm/partial_screenshot_event_filter.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/root_window_event_filter.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/shadow_controller.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/stacking_controller.h"
#include "ash/wm/status_area_layout_manager.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/toplevel_layout_manager.h"
#include "ash/wm/toplevel_window_event_filter.h"
#include "ash/wm/video_detector.h"
#include "ash/wm/visibility_controller.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_modality_controller.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "ash/wm/workspace/workspace_event_filter.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace/workspace_manager.h"
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

#if !defined(OS_MACOSX)
#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_filter.h"
#include "ash/accelerators/nested_dispatcher_controller.h"
#endif

namespace ash {

namespace {

using aura::Window;
using views::Widget;

// Screen width above which we automatically start in overlapping window mode,
// in pixels. Should be at least 1366 pixels as we traditionally used a single
// window on Chrome OS ZGB devices with that width.
const int kOverlappingWindowModeWidthThreshold = 1366;

// Screen height above which we automatically start in overlapping window mode,
// in pixels. Should be at least 800 pixels as we traditionally used a single
// window on Chrome OS alex devices with that height.
const int kOverlappingWindowModeHeightThreshold = 800;

// Creates each of the special window containers that holds windows of various
// types in the shell UI. They are added to |containers| from back to front in
// the z-index.
void CreateSpecialContainers(aura::Window::Windows* containers) {
  aura::Window* unparented_control_container = new aura::Window(NULL);
  unparented_control_container->set_id(
      internal::kShellWindowId_UnparentedControlContainer);
  containers->push_back(unparented_control_container);

  aura::Window* background_container = new aura::Window(NULL);
  background_container->set_id(
      internal::kShellWindowId_DesktopBackgroundContainer);
  containers->push_back(background_container);

  aura::Window* default_container = new aura::Window(NULL);
  default_container->SetEventFilter(
      new ToplevelWindowEventFilter(default_container));
  default_container->set_id(internal::kShellWindowId_DefaultContainer);
  SetChildWindowVisibilityChangesAnimated(default_container);
  containers->push_back(default_container);

  aura::Window* always_on_top_container = new aura::Window(NULL);
  always_on_top_container->SetEventFilter(
      new ToplevelWindowEventFilter(always_on_top_container));
  always_on_top_container->set_id(
      internal::kShellWindowId_AlwaysOnTopContainer);
  SetChildWindowVisibilityChangesAnimated(always_on_top_container);
  containers->push_back(always_on_top_container);

  aura::Window* panel_container = new aura::Window(NULL);
  panel_container->set_id(internal::kShellWindowId_PanelContainer);
  if (CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kAuraPanelManager)) {
    internal::PanelLayoutManager* layout_manager =
        new internal::PanelLayoutManager(panel_container);
    panel_container->SetEventFilter(
        new internal::PanelWindowEventFilter(panel_container, layout_manager));
    panel_container->SetLayoutManager(layout_manager);
  }
  containers->push_back(panel_container);

  aura::Window* launcher_container = new aura::Window(NULL);
  launcher_container->set_id(internal::kShellWindowId_LauncherContainer);
  containers->push_back(launcher_container);

  aura::Window* modal_container = new aura::Window(NULL);
  modal_container->SetEventFilter(
      new ToplevelWindowEventFilter(modal_container));
  modal_container->SetLayoutManager(
      new internal::SystemModalContainerLayoutManager(modal_container));
  modal_container->set_id(internal::kShellWindowId_SystemModalContainer);
  SetChildWindowVisibilityChangesAnimated(modal_container);
  containers->push_back(modal_container);

  // TODO(beng): Figure out if we can make this use
  // SystemModalContainerEventFilter instead of stops_event_propagation.
  aura::Window* lock_container = new aura::Window(NULL);
  lock_container->SetLayoutManager(new internal::BaseLayoutManager);
  lock_container->set_stops_event_propagation(true);
  lock_container->set_id(internal::kShellWindowId_LockScreenContainer);
  containers->push_back(lock_container);

  aura::Window* lock_modal_container = new aura::Window(NULL);
  lock_modal_container->SetEventFilter(
      new ToplevelWindowEventFilter(lock_modal_container));
  lock_modal_container->SetLayoutManager(
      new internal::SystemModalContainerLayoutManager(lock_modal_container));
  lock_modal_container->set_id(
      internal::kShellWindowId_LockSystemModalContainer);
  SetChildWindowVisibilityChangesAnimated(lock_modal_container);
  containers->push_back(lock_modal_container);

  aura::Window* status_container = new aura::Window(NULL);
  status_container->set_id(internal::kShellWindowId_StatusContainer);
  containers->push_back(status_container);

  aura::Window* menu_container = new aura::Window(NULL);
  menu_container->set_id(internal::kShellWindowId_MenuContainer);
  SetChildWindowVisibilityChangesAnimated(menu_container);
  containers->push_back(menu_container);

  aura::Window* drag_drop_container = new aura::Window(NULL);
  drag_drop_container->set_id(
      internal::kShellWindowId_DragImageAndTooltipContainer);
  SetChildWindowVisibilityChangesAnimated(drag_drop_container);
  containers->push_back(drag_drop_container);

  aura::Window* setting_bubble_container = new aura::Window(NULL);
  setting_bubble_container->set_id(
      internal::kShellWindowId_SettingBubbleContainer);
  containers->push_back(setting_bubble_container);

  aura::Window* overlay_container = new aura::Window(NULL);
  overlay_container->set_id(internal::kShellWindowId_OverlayContainer);
  containers->push_back(overlay_container);
}

// Maximizes all the windows in a |container|.
void MaximizeWindows(aura::Window* container) {
  const std::vector<aura::Window*>& windows = container->children();
  for (std::vector<aura::Window*>::const_iterator it = windows.begin();
       it != windows.end();
       ++it)
    window_util::MaximizeWindow(*it);
}

// Restores all maximized windows in a |container|.
void RestoreMaximizedWindows(aura::Window* container) {
  const std::vector<aura::Window*>& windows = container->children();
  for (std::vector<aura::Window*>::const_iterator it = windows.begin();
       it != windows.end();
       ++it) {
    if (window_util::IsWindowMaximized(*it))
      window_util::RestoreWindow(*it);
  }
}

}  // namespace

// static
Shell* Shell::instance_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

Shell::Shell(ShellDelegate* delegate)
    : root_filter_(new internal::RootWindowEventFilter),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
#if !defined(OS_MACOSX)
      nested_dispatcher_controller_(new NestedDispatcherController),
      accelerator_controller_(new AcceleratorController),
#endif
      delegate_(delegate),
      shelf_(NULL),
      dynamic_window_mode_(false),
      window_mode_(MODE_OVERLAPPING),
      desktop_background_mode_(BACKGROUND_IMAGE),
      root_window_layout_(NULL),
      status_widget_(NULL) {
  // Pass ownership of the filter to the root window.
  GetRootWindow()->SetEventFilter(root_filter_);
}

Shell::~Shell() {
  RemoveRootWindowEventFilter(partial_screenshot_filter_.get());
  RemoveRootWindowEventFilter(input_method_filter_.get());
  RemoveRootWindowEventFilter(window_modality_controller_.get());
#if !defined(OS_MACOSX)
  RemoveRootWindowEventFilter(accelerator_filter_.get());
#endif

  // Close background widget now so that the focus manager of the
  // widget gets deleted in the final message loop run.
  root_window_layout_->SetBackgroundWidget(NULL);

  // TooltipController is deleted with the Shell so removing its references.
  RemoveRootWindowEventFilter(tooltip_controller_.get());
  aura::client::SetTooltipClient(NULL);

  // Make sure we delete WorkspaceController before launcher is
  // deleted as it has a reference to launcher model.
  workspace_controller_.reset();

  // Delete containers now so that child windows does not access
  // observers when they are destructed.
  aura::RootWindow* root_window = GetRootWindow();
  while (!root_window->children().empty()) {
    aura::Window* child = root_window->children()[0];
    delete child;
  }

  // These need a valid Shell instance to clean up properly, so explicitly
  // delete them before invalidating the instance.
  drag_drop_controller_.reset();
  window_cycle_controller_.reset();

  // Launcher widget has a InputMethodBridge that references to
  // input_method_filter_'s input_method_. So explicitly release launcher_
  // before input_method_filter_. And this needs to be after we delete all
  // containers in case there are still live browser windows which access
  // LauncherModel during close.
  launcher_.reset();

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

// static
aura::RootWindow* Shell::GetRootWindow() {
  return aura::RootWindow::GetInstance();
}

void Shell::Init() {
  DCHECK(!GetRootWindowEventFilterCount());

  // PartialScreenshotEventFilter must be the first one to capture key
  // events when the taking partial screenshot UI is there.
  partial_screenshot_filter_.reset(new internal::PartialScreenshotEventFilter);
  AddRootWindowEventFilter(partial_screenshot_filter_.get());

  // Then AcceleratorFilter and InputMethodEventFilter must be added (in this
  // order) since they have the second highest priority.
  DCHECK_EQ(1U, GetRootWindowEventFilterCount());
#if !defined(OS_MACOSX)
  accelerator_filter_.reset(new internal::AcceleratorFilter);
  AddRootWindowEventFilter(accelerator_filter_.get());
  DCHECK_EQ(2U, GetRootWindowEventFilterCount());
#endif
  input_method_filter_.reset(new internal::InputMethodEventFilter);
  AddRootWindowEventFilter(input_method_filter_.get());

  // Dynamic window mode affects window mode computation.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  dynamic_window_mode_ = command_line->HasSwitch(
      switches::kAuraDynamicWindowMode);

  // Window mode must be set before computing containers or layout managers.
  gfx::Size monitor_size = gfx::Screen::GetPrimaryMonitorSize();
  window_mode_ = ComputeWindowMode(monitor_size, command_line);

  aura::RootWindow* root_window = GetRootWindow();
  root_window->SetCursor(aura::kCursorPointer);

  activation_controller_.reset(new internal::ActivationController);

  aura::Window::Windows containers;
  CreateSpecialContainers(&containers);
  aura::Window::Windows::const_iterator i;
  for (i = containers.begin(); i != containers.end(); ++i) {
    (*i)->Init(ui::Layer::LAYER_NOT_DRAWN);
    root_window->AddChild(*i);
    if ((*i)->id() != internal::kShellWindowId_UnparentedControlContainer)
      (*i)->Show();
  }

  stacking_controller_.reset(new internal::StackingController);

  root_window_layout_ = new internal::RootWindowLayoutManager(root_window);
  root_window->SetLayoutManager(root_window_layout_);

  if (delegate_.get())
    status_widget_ = delegate_->CreateStatusArea();
  if (!status_widget_)
    status_widget_ = internal::CreateStatusArea();

  aura::Window* default_container =
      GetContainer(internal::kShellWindowId_DefaultContainer);
  launcher_.reset(new Launcher(default_container));

  if (window_mode_ == MODE_COMPACT)
    SetupCompactWindowMode();
  else
    SetupNonCompactWindowMode();

  if (!command_line->HasSwitch(switches::kAuraNoShadows))
    shadow_controller_.reset(new internal::ShadowController());

  focus_cycler_.reset(new internal::FocusCycler());
  focus_cycler_->AddWidget(status_widget_);
  focus_cycler_->AddWidget(launcher_->widget());
  launcher_->SetFocusCycler(focus_cycler_.get());

  // Force a layout.
  root_window->layout_manager()->OnWindowResized();

  window_modality_controller_.reset(new internal::WindowModalityController);
  AddRootWindowEventFilter(window_modality_controller_.get());

  visibility_controller_.reset(new internal::VisibilityController);
  aura::client::SetVisibilityClient(visibility_controller_.get());

  tooltip_controller_.reset(new internal::TooltipController);
  AddRootWindowEventFilter(tooltip_controller_.get());
  aura::client::SetTooltipClient(tooltip_controller_.get());

  drag_drop_controller_.reset(new internal::DragDropController);
  power_button_controller_.reset(new PowerButtonController);
  video_detector_.reset(new VideoDetector);
  window_cycle_controller_.reset(new WindowCycleController);
}

Shell::WindowMode Shell::ComputeWindowMode(const gfx::Size& monitor_size,
                                           CommandLine* command_line) const {
  // Some devices don't perform well with overlapping windows.
  if (command_line->HasSwitch(switches::kAuraForceCompactWindowMode))
    return MODE_COMPACT;

  // If user set the flag, use their desired behavior.
  if (command_line->HasSwitch(switches::kAuraWindowMode)) {
    std::string mode =
        command_line->GetSwitchValueASCII(switches::kAuraWindowMode);
    if (mode == switches::kAuraWindowModeCompact)
      return MODE_COMPACT;
    if (mode == switches::kAuraWindowModeManaged)
      return MODE_MANAGED;
    if (mode == switches::kAuraWindowModeOverlapping)
      return MODE_OVERLAPPING;
  }

  // If we're trying to dynamically choose window mode based on screen
  // resolution, use compact mode for narrow/short screens.
  // TODO(jamescook): If we go with a simple variant of overlapping mode for
  // small screens we should remove this and the dynamic mode switching code
  // in SetupCompactWindowMode and SetupNonCompactWindowMode.
  if (dynamic_window_mode_ &&
      monitor_size.width() <= kOverlappingWindowModeWidthThreshold &&
      monitor_size.height() <= kOverlappingWindowModeHeightThreshold)
    return Shell::MODE_COMPACT;

  // Overlapping is the default.
  return Shell::MODE_OVERLAPPING;
}

aura::Window* Shell::GetContainer(int container_id) {
  return const_cast<aura::Window*>(
      const_cast<const Shell*>(this)->GetContainer(container_id));
}

const aura::Window* Shell::GetContainer(int container_id) const {
  return GetRootWindow()->GetChildById(container_id);
}

void Shell::AddRootWindowEventFilter(aura::EventFilter* filter) {
  static_cast<internal::RootWindowEventFilter*>(
      GetRootWindow()->event_filter())->AddFilter(filter);
}

void Shell::RemoveRootWindowEventFilter(aura::EventFilter* filter) {
  static_cast<internal::RootWindowEventFilter*>(
      GetRootWindow()->event_filter())->RemoveFilter(filter);
}

size_t Shell::GetRootWindowEventFilterCount() const {
  return static_cast<internal::RootWindowEventFilter*>(
      GetRootWindow()->event_filter())->GetFilterCount();
}

void Shell::ShowBackgroundMenu(views::Widget* widget,
                               const gfx::Point& location) {
  if (workspace_controller_.get())
    workspace_controller_->ShowMenu(widget, location);
}

void Shell::ToggleAppList() {
  if (!app_list_.get())
    app_list_.reset(new internal::AppList);
  app_list_->SetVisible(!app_list_->IsVisible());
}

void Shell::ChangeWindowMode(WindowMode mode) {
  if (mode == window_mode_)
    return;
  // Window mode must be set before we resize/layout the windows.
  window_mode_ = mode;
  if (window_mode_ == MODE_COMPACT)
    SetupCompactWindowMode();
  else
    SetupNonCompactWindowMode();
}

void Shell::SetWindowModeForMonitorSize(const gfx::Size& monitor_size) {
  // If we're running on a device, a resolution change means the user plugged in
  // or unplugged an external monitor. Change window mode to be appropriate for
  // the new screen resolution.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  WindowMode new_mode = ComputeWindowMode(monitor_size, command_line);
  ChangeWindowMode(new_mode);
}

void Shell::SetDesktopBackgroundMode(BackgroundMode mode) {
  if (mode == BACKGROUND_SOLID_COLOR) {
    // Set a solid black background.
    // TODO(derat): Remove this in favor of having the compositor only clear the
    // viewport when there are regions not covered by a layer:
    // http://crbug.com/113445
    ui::Layer* background_layer = new ui::Layer(ui::Layer::LAYER_SOLID_COLOR);
    background_layer->SetColor(SK_ColorBLACK);
    GetContainer(internal::kShellWindowId_DesktopBackgroundContainer)->
        layer()->Add(background_layer);
    root_window_layout_->SetBackgroundLayer(background_layer);
    root_window_layout_->SetBackgroundWidget(NULL);
  } else {
    // Create the desktop background image.
    root_window_layout_->SetBackgroundLayer(NULL);
    root_window_layout_->SetBackgroundWidget(
        internal::CreateDesktopBackground());
  }
  desktop_background_mode_ = mode;
}

bool Shell::IsScreenLocked() const {
  const aura::Window* lock_screen_container = GetContainer(
      internal::kShellWindowId_LockScreenContainer);
  return lock_screen_container->StopsEventPropagation();
}

bool Shell::IsModalWindowOpen() const {
  const aura::Window* modal_container = GetContainer(
      internal::kShellWindowId_SystemModalContainer);
  return !modal_container->children().empty();
}

void Shell::SetCompactStatusAreaOffset(gfx::Size& offset) {
  compact_status_area_offset_ = offset;

  // Trigger a relayout.
  if (IsWindowModeCompact()) {
    aura::LayoutManager* layout_manager = GetContainer(
        internal::kShellWindowId_StatusContainer)->layout_manager();
    if (layout_manager)
      layout_manager->OnWindowResized();
  }
}

views::NonClientFrameView* Shell::CreateDefaultNonClientFrameView(
    views::Widget* widget) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAuraGoogleDialogFrames)) {
    return new internal::DialogFrameView;
  }
  return NULL;
}

void Shell::RotateFocus(Direction direction) {
  focus_cycler_->RotateFocus(
      direction == FORWARD ? internal::FocusCycler::FORWARD :
                             internal::FocusCycler::BACKWARD);
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

// Compact mode has a simplified layout manager and doesn't use the shelf,
// desktop background, etc.  The launcher still exists so we can use its
// data model and list of open windows, but we hide the UI to save space.
void Shell::SetupCompactWindowMode() {
  DCHECK(root_window_layout_);
  DCHECK(status_widget_);

  shelf_ = NULL;

  // Clean out the old layout managers before we start.
  ResetLayoutManager(internal::kShellWindowId_DefaultContainer);
  ResetLayoutManager(internal::kShellWindowId_LauncherContainer);
  ResetLayoutManager(internal::kShellWindowId_StatusContainer);

  // Don't use an event filter for the default container, as we don't support
  // window dragging, double-click to maximize, etc.
  aura::Window* default_container =
      GetContainer(internal::kShellWindowId_DefaultContainer);
  default_container->SetEventFilter(NULL);

  // Set up our new layout managers.
  internal::CompactLayoutManager* compact_layout_manager =
      new internal::CompactLayoutManager();
  compact_layout_manager->set_status_area_widget(status_widget_);
  internal::CompactStatusAreaLayoutManager* status_area_layout_manager =
      new internal::CompactStatusAreaLayoutManager(status_widget_);
  GetContainer(internal::kShellWindowId_StatusContainer)->
      SetLayoutManager(status_area_layout_manager);
  default_container->SetLayoutManager(compact_layout_manager);

  // Keep the launcher for its data model, but hide it.  Do this before
  // maximizing the windows so the work area is the right size.
  launcher_->widget()->Hide();

  // Maximize all the windows, using the new layout manager.
  MaximizeWindows(default_container);

  // Set a solid black background.
  SetDesktopBackgroundMode(BACKGROUND_SOLID_COLOR);
}

void Shell::SetupNonCompactWindowMode() {
  DCHECK(root_window_layout_);
  DCHECK(status_widget_);

  // Clean out the old layout managers before we start.
  ResetLayoutManager(internal::kShellWindowId_DefaultContainer);
  ResetLayoutManager(internal::kShellWindowId_LauncherContainer);
  ResetLayoutManager(internal::kShellWindowId_StatusContainer);

  internal::ShelfLayoutManager* shelf_layout_manager =
      new internal::ShelfLayoutManager(launcher_->widget(), status_widget_);
  GetContainer(internal::kShellWindowId_LauncherContainer)->
      SetLayoutManager(shelf_layout_manager);
  shelf_ = shelf_layout_manager;

  internal::StatusAreaLayoutManager* status_area_layout_manager =
      new internal::StatusAreaLayoutManager(shelf_layout_manager);
  GetContainer(internal::kShellWindowId_StatusContainer)->
      SetLayoutManager(status_area_layout_manager);

  aura::Window* default_container =
      GetContainer(internal::kShellWindowId_DefaultContainer);
  if (window_mode_ == MODE_MANAGED) {
    // Workspace manager has its own layout managers.
    workspace_controller_.reset(
        new internal::WorkspaceController(default_container));
    workspace_controller_->workspace_manager()->set_shelf(shelf_layout_manager);
  } else {
    // Default layout manager.
    internal::ToplevelLayoutManager* toplevel_layout_manager =
        new internal::ToplevelLayoutManager();
    toplevel_layout_manager->set_shelf(shelf_layout_manager);
    default_container->SetLayoutManager(toplevel_layout_manager);
    default_container->SetEventFilter(
        new ToplevelWindowEventFilter(default_container));
  }
  // Ensure launcher is visible.
  launcher_->widget()->Show();

  // Restore all maximized windows.  Don't change full screen windows, as we
  // don't want to disrupt a user trying to plug in an external monitor to
  // give a presentation.
  RestoreMaximizedWindows(default_container);

  // Create the desktop background image.
  SetDesktopBackgroundMode(BACKGROUND_IMAGE);
}

void Shell::ResetLayoutManager(int container_id) {
  GetContainer(container_id)->SetLayoutManager(NULL);
}

}  // namespace ash
