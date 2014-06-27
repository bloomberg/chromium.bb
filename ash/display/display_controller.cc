// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "ash/ash_switches.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_layout_store.h"
#include "ash/display/display_manager.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/root_window_transformers.h"
#include "ash/display/virtual_keyboard_window_controller.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/host/root_window_transformer.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/coordinate_conversion.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/wm/public/activation_client.h"

#if defined(OS_CHROMEOS)
#include "ash/display/display_configurator_animation.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#endif  // defined(OS_CHROMEOS)

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11_types.h"

// Including this at the bottom to avoid other
// potential conflict with chrome headers.
#include <X11/extensions/Xrandr.h>
#undef RootWindow
#endif  // defined(USE_X11)


namespace ash {
namespace {

// Primary display stored in global object as it can be
// accessed after Shell is deleted. A separate display instance is created
// during the shutdown instead of always keeping two display instances
// (one here and another one in display_manager) in sync, which is error prone.
// This is initialized in the constructor, and then in CreatePrimaryHost().
int64 primary_display_id = -1;

// Specifies how long the display change should have been disabled
// after each display change operations.
// |kCycleDisplayThrottleTimeoutMs| is set to be longer to avoid
// changing the settings while the system is still configurating
// displays. It will be overriden by |kAfterDisplayChangeThrottleTimeoutMs|
// when the display change happens, so the actual timeout is much shorter.
const int64 kAfterDisplayChangeThrottleTimeoutMs = 500;
const int64 kCycleDisplayThrottleTimeoutMs = 4000;
const int64 kSwapDisplayThrottleTimeoutMs = 500;

DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

void SetDisplayPropertiesOnHost(AshWindowTreeHost* ash_host,
                                const gfx::Display& display) {
  DisplayInfo info = GetDisplayManager()->GetDisplayInfo(display.id());
  aura::WindowTreeHost* host = ash_host->AsWindowTreeHost();
#if defined(OS_CHROMEOS) && defined(USE_X11)
  // Native window property (Atom in X11) that specifies the display's
  // rotation, scale factor and if it's internal display.  They are
  // read and used by touchpad/mouse driver directly on X (contact
  // adlr@ for more details on touchpad/mouse driver side). The value
  // of the rotation is one of 0 (normal), 1 (90 degrees clockwise), 2
  // (180 degree) or 3 (270 degrees clockwise).  The value of the
  // scale factor is in percent (100, 140, 200 etc).
  const char kRotationProp[] = "_CHROME_DISPLAY_ROTATION";
  const char kScaleFactorProp[] = "_CHROME_DISPLAY_SCALE_FACTOR";
  const char kInternalProp[] = "_CHROME_DISPLAY_INTERNAL";
  const char kCARDINAL[] = "CARDINAL";
  int xrandr_rotation = RR_Rotate_0;
  switch (info.rotation()) {
    case gfx::Display::ROTATE_0:
      xrandr_rotation = RR_Rotate_0;
      break;
    case gfx::Display::ROTATE_90:
      xrandr_rotation = RR_Rotate_90;
      break;
    case gfx::Display::ROTATE_180:
      xrandr_rotation = RR_Rotate_180;
      break;
    case gfx::Display::ROTATE_270:
      xrandr_rotation = RR_Rotate_270;
      break;
  }

  int internal = display.IsInternal() ? 1 : 0;
  gfx::AcceleratedWidget xwindow = host->GetAcceleratedWidget();
  ui::SetIntProperty(xwindow, kInternalProp, kCARDINAL, internal);
  ui::SetIntProperty(xwindow, kRotationProp, kCARDINAL, xrandr_rotation);
  ui::SetIntProperty(xwindow,
                     kScaleFactorProp,
                     kCARDINAL,
                     100 * display.device_scale_factor());
#endif
  scoped_ptr<RootWindowTransformer> transformer(
      CreateRootWindowTransformerForDisplay(host->window(), display));
  ash_host->SetRootWindowTransformer(transformer.Pass());

  DisplayMode mode;
  if (GetDisplayManager()->GetSelectedModeForDisplayId(display.id(), &mode) &&
      mode.refresh_rate > 0.0f) {
    host->compositor()->vsync_manager()->SetAuthoritativeVSyncInterval(
        base::TimeDelta::FromMicroseconds(
            base::Time::kMicrosecondsPerSecond / mode.refresh_rate));
  }
}

aura::Window* GetWindow(AshWindowTreeHost* ash_host) {
  CHECK(ash_host->AsWindowTreeHost());
  return ash_host->AsWindowTreeHost()->window();
}

}  // namespace

// A utility class to store/restore focused/active window
// when the display configuration has changed.
class FocusActivationStore {
 public:
  FocusActivationStore()
      : activation_client_(NULL),
        capture_client_(NULL),
        focus_client_(NULL),
        focused_(NULL),
        active_(NULL) {
  }

  void Store(bool clear_focus) {
    if (!activation_client_) {
      aura::Window* root = Shell::GetPrimaryRootWindow();
      activation_client_ = aura::client::GetActivationClient(root);
      capture_client_ = aura::client::GetCaptureClient(root);
      focus_client_ = aura::client::GetFocusClient(root);
    }
    focused_ = focus_client_->GetFocusedWindow();
    if (focused_)
      tracker_.Add(focused_);
    active_ = activation_client_->GetActiveWindow();
    if (active_ && focused_ != active_)
      tracker_.Add(active_);

    // Deactivate the window to close menu / bubble windows.
    if (clear_focus)
      activation_client_->DeactivateWindow(active_);

    // Release capture if any.
    capture_client_->SetCapture(NULL);
    // Clear the focused window if any. This is necessary because a
    // window may be deleted when losing focus (fullscreen flash for
    // example).  If the focused window is still alive after move, it'll
    // be re-focused below.
    if (clear_focus)
      focus_client_->FocusWindow(NULL);
  }

  void Restore() {
    // Restore focused or active window if it's still alive.
    if (focused_ && tracker_.Contains(focused_)) {
      focus_client_->FocusWindow(focused_);
    } else if (active_ && tracker_.Contains(active_)) {
      activation_client_->ActivateWindow(active_);
    }
    if (focused_)
      tracker_.Remove(focused_);
    if (active_)
      tracker_.Remove(active_);
    focused_ = NULL;
    active_ = NULL;
  }

 private:
  aura::client::ActivationClient* activation_client_;
  aura::client::CaptureClient* capture_client_;
  aura::client::FocusClient* focus_client_;
  aura::WindowTracker tracker_;
  aura::Window* focused_;
  aura::Window* active_;

  DISALLOW_COPY_AND_ASSIGN(FocusActivationStore);
};

////////////////////////////////////////////////////////////////////////////////
// DisplayChangeLimiter

DisplayController::DisplayChangeLimiter::DisplayChangeLimiter()
    : throttle_timeout_(base::Time::Now()) {
}

void DisplayController::DisplayChangeLimiter::SetThrottleTimeout(
    int64 throttle_ms) {
  throttle_timeout_ =
      base::Time::Now() + base::TimeDelta::FromMilliseconds(throttle_ms);
}

bool DisplayController::DisplayChangeLimiter::IsThrottled() const {
  return base::Time::Now() < throttle_timeout_;
}

////////////////////////////////////////////////////////////////////////////////
// DisplayController

DisplayController::DisplayController()
    : primary_tree_host_for_replace_(NULL),
      focus_activation_store_(new FocusActivationStore()),
      cursor_window_controller_(new CursorWindowController()),
      mirror_window_controller_(new MirrorWindowController()),
      weak_ptr_factory_(this) {
#if defined(OS_CHROMEOS)
  if (base::SysInfo::IsRunningOnChromeOS())
    limiter_.reset(new DisplayChangeLimiter);
#endif
  // Reset primary display to make sure that tests don't use
  // stale display info from previous tests.
  primary_display_id = gfx::Display::kInvalidDisplayID;
}

DisplayController::~DisplayController() {
}

void DisplayController::Start() {
  // Created here so that Shell has finished being created. Adds itself
  // as a ShellObserver.
  virtual_keyboard_window_controller_.reset(
      new VirtualKeyboardWindowController);
  Shell::GetScreen()->AddObserver(this);
  Shell::GetInstance()->display_manager()->set_delegate(this);
}

void DisplayController::Shutdown() {
  // Unset the display manager's delegate here because
  // DisplayManager outlives DisplayController.
  Shell::GetInstance()->display_manager()->set_delegate(NULL);

  cursor_window_controller_.reset();
  mirror_window_controller_.reset();
  virtual_keyboard_window_controller_.reset();

  Shell::GetScreen()->RemoveObserver(this);
  // Delete all root window controllers, which deletes root window
  // from the last so that the primary root window gets deleted last.
  for (WindowTreeHostMap::const_reverse_iterator it =
           window_tree_hosts_.rbegin();
       it != window_tree_hosts_.rend();
       ++it) {
    RootWindowController* controller =
        GetRootWindowController(GetWindow(it->second));
    DCHECK(controller);
    delete controller;
  }
}

void DisplayController::CreatePrimaryHost(
    const AshWindowTreeHostInitParams& init_params) {
  const gfx::Display& primary_candidate =
      GetDisplayManager()->GetPrimaryDisplayCandidate();
  primary_display_id = primary_candidate.id();
  CHECK_NE(gfx::Display::kInvalidDisplayID, primary_display_id);
  AddWindowTreeHostForDisplay(primary_candidate, init_params);
}

void DisplayController::InitDisplays() {
  RootWindowController::CreateForPrimaryDisplay(
      window_tree_hosts_[primary_display_id]);

  DisplayManager* display_manager = GetDisplayManager();
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    const gfx::Display& display = display_manager->GetDisplayAt(i);
    if (primary_display_id != display.id()) {
      AshWindowTreeHost* ash_host = AddWindowTreeHostForDisplay(
          display, AshWindowTreeHostInitParams());
      RootWindowController::CreateForSecondaryDisplay(ash_host);
    }
  }
  UpdateHostWindowNames();

  FOR_EACH_OBSERVER(Observer, observers_, OnDisplaysInitialized());
}

void DisplayController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DisplayController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
int64 DisplayController::GetPrimaryDisplayId() {
  CHECK_NE(gfx::Display::kInvalidDisplayID, primary_display_id);
  return primary_display_id;
}

aura::Window* DisplayController::GetPrimaryRootWindow() {
  return GetRootWindowForDisplayId(primary_display_id);
}

aura::Window* DisplayController::GetRootWindowForDisplayId(int64 id) {
  DCHECK_EQ(1u, window_tree_hosts_.count(id));
  AshWindowTreeHost* host = window_tree_hosts_[id];
  CHECK(host);
  return GetWindow(host);
}

void DisplayController::CloseChildWindows() {
  for (WindowTreeHostMap::const_iterator it = window_tree_hosts_.begin();
       it != window_tree_hosts_.end();
       ++it) {
    aura::Window* root_window = GetWindow(it->second);
    RootWindowController* controller = GetRootWindowController(root_window);
    if (controller) {
      controller->CloseChildWindows();
    } else {
      while (!root_window->children().empty()) {
        aura::Window* child = root_window->children()[0];
        delete child;
      }
    }
  }
}

aura::Window::Windows DisplayController::GetAllRootWindows() {
  aura::Window::Windows windows;
  for (WindowTreeHostMap::const_iterator it = window_tree_hosts_.begin();
       it != window_tree_hosts_.end();
       ++it) {
    DCHECK(it->second);
    if (GetRootWindowController(GetWindow(it->second)))
      windows.push_back(GetWindow(it->second));
  }
  return windows;
}

gfx::Insets DisplayController::GetOverscanInsets(int64 display_id) const {
  return GetDisplayManager()->GetOverscanInsets(display_id);
}

void DisplayController::SetOverscanInsets(int64 display_id,
                                          const gfx::Insets& insets_in_dip) {
  GetDisplayManager()->SetOverscanInsets(display_id, insets_in_dip);
}

std::vector<RootWindowController*>
DisplayController::GetAllRootWindowControllers() {
  std::vector<RootWindowController*> controllers;
  for (WindowTreeHostMap::const_iterator it = window_tree_hosts_.begin();
       it != window_tree_hosts_.end();
       ++it) {
    RootWindowController* controller =
        GetRootWindowController(GetWindow(it->second));
    if (controller)
      controllers.push_back(controller);
  }
  return controllers;
}

void DisplayController::ToggleMirrorMode() {
  DisplayManager* display_manager = GetDisplayManager();
  if (display_manager->num_connected_displays() <= 1)
    return;

  if (limiter_) {
    if  (limiter_->IsThrottled())
      return;
    limiter_->SetThrottleTimeout(kCycleDisplayThrottleTimeoutMs);
  }
#if defined(OS_CHROMEOS)
  Shell* shell = Shell::GetInstance();
  DisplayConfiguratorAnimation* animation =
      shell->display_configurator_animation();
  animation->StartFadeOutAnimation(
      base::Bind(&DisplayController::SetMirrorModeAfterAnimation,
                 weak_ptr_factory_.GetWeakPtr(),
                 !display_manager->IsMirrored()));
#endif
}

void DisplayController::SwapPrimaryDisplay() {
  if (limiter_) {
    if  (limiter_->IsThrottled())
      return;
    limiter_->SetThrottleTimeout(kSwapDisplayThrottleTimeoutMs);
  }

  if (Shell::GetScreen()->GetNumDisplays() > 1) {
#if defined(OS_CHROMEOS)
    DisplayConfiguratorAnimation* animation =
        Shell::GetInstance()->display_configurator_animation();
    if (animation) {
      animation->StartFadeOutAnimation(base::Bind(
          &DisplayController::OnFadeOutForSwapDisplayFinished,
          weak_ptr_factory_.GetWeakPtr()));
    } else {
      SetPrimaryDisplay(ScreenUtil::GetSecondaryDisplay());
    }
#else
    SetPrimaryDisplay(ScreenUtil::GetSecondaryDisplay());
#endif
  }
}

void DisplayController::SetPrimaryDisplayId(int64 id) {
  DCHECK_NE(gfx::Display::kInvalidDisplayID, id);
  if (id == gfx::Display::kInvalidDisplayID || primary_display_id == id)
    return;

  const gfx::Display& display = GetDisplayManager()->GetDisplayForId(id);
  if (display.is_valid())
    SetPrimaryDisplay(display);
}

void DisplayController::SetPrimaryDisplay(
    const gfx::Display& new_primary_display) {
  DisplayManager* display_manager = GetDisplayManager();
  DCHECK(new_primary_display.is_valid());
  DCHECK(display_manager->IsActiveDisplay(new_primary_display));

  if (!new_primary_display.is_valid() ||
      !display_manager->IsActiveDisplay(new_primary_display)) {
    LOG(ERROR) << "Invalid or non-existent display is requested:"
               << new_primary_display.ToString();
    return;
  }

  if (primary_display_id == new_primary_display.id() ||
      window_tree_hosts_.size() < 2) {
    return;
  }

  AshWindowTreeHost* non_primary_host =
      window_tree_hosts_[new_primary_display.id()];
  LOG_IF(ERROR, !non_primary_host)
      << "Unknown display is requested in SetPrimaryDisplay: id="
      << new_primary_display.id();
  if (!non_primary_host)
    return;

  gfx::Display old_primary_display = Shell::GetScreen()->GetPrimaryDisplay();

  // Swap root windows between current and new primary display.
  AshWindowTreeHost* primary_host = window_tree_hosts_[primary_display_id];
  CHECK(primary_host);
  CHECK_NE(primary_host, non_primary_host);

  window_tree_hosts_[new_primary_display.id()] = primary_host;
  GetRootWindowSettings(GetWindow(primary_host))->display_id =
      new_primary_display.id();

  window_tree_hosts_[old_primary_display.id()] = non_primary_host;
  GetRootWindowSettings(GetWindow(non_primary_host))->display_id =
      old_primary_display.id();

  primary_display_id = new_primary_display.id();
  GetDisplayManager()->layout_store()->UpdatePrimaryDisplayId(
      display_manager->GetCurrentDisplayIdPair(), primary_display_id);

  UpdateWorkAreaOfDisplayNearestWindow(GetWindow(primary_host),
                                       old_primary_display.GetWorkAreaInsets());
  UpdateWorkAreaOfDisplayNearestWindow(GetWindow(non_primary_host),
                                       new_primary_display.GetWorkAreaInsets());

  // Update the dispay manager with new display info.
  std::vector<DisplayInfo> display_info_list;
  display_info_list.push_back(display_manager->GetDisplayInfo(
      primary_display_id));
  display_info_list.push_back(display_manager->GetDisplayInfo(
      ScreenUtil::GetSecondaryDisplay().id()));
  GetDisplayManager()->set_force_bounds_changed(true);
  GetDisplayManager()->UpdateDisplays(display_info_list);
  GetDisplayManager()->set_force_bounds_changed(false);
}

void DisplayController::EnsurePointerInDisplays() {
  // If the mouse is currently on a display in native location,
  // use the same native location. Otherwise find the display closest
  // to the current cursor location in screen coordinates.

  gfx::Point point_in_screen = Shell::GetScreen()->GetCursorScreenPoint();
  gfx::Point target_location_in_native;
  int64 closest_distance_squared = -1;
  DisplayManager* display_manager = GetDisplayManager();

  aura::Window* dst_root_window = NULL;
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    const gfx::Display& display = display_manager->GetDisplayAt(i);
    const DisplayInfo display_info =
        display_manager->GetDisplayInfo(display.id());
    aura::Window* root_window = GetRootWindowForDisplayId(display.id());
    if (display_info.bounds_in_native().Contains(
            cursor_location_in_native_coords_for_restore_)) {
      dst_root_window = root_window;
      target_location_in_native = cursor_location_in_native_coords_for_restore_;
      break;
    }
    gfx::Point center = display.bounds().CenterPoint();
    // Use the distance squared from the center of the dislay. This is not
    // exactly "closest" display, but good enough to pick one
    // appropriate (and there are at most two displays).
    // We don't care about actual distance, only relative to other displays, so
    // using the LengthSquared() is cheaper than Length().

    int64 distance_squared = (center - point_in_screen).LengthSquared();
    if (closest_distance_squared < 0 ||
        closest_distance_squared > distance_squared) {
      aura::Window* root_window = GetRootWindowForDisplayId(display.id());
      aura::client::ScreenPositionClient* client =
          aura::client::GetScreenPositionClient(root_window);
      client->ConvertPointFromScreen(root_window, &center);
      root_window->GetHost()->ConvertPointToNativeScreen(&center);
      dst_root_window = root_window;
      target_location_in_native = center;
      closest_distance_squared = distance_squared;
    }
  }
  dst_root_window->GetHost()->ConvertPointFromNativeScreen(
      &target_location_in_native);
  dst_root_window->MoveCursorTo(target_location_in_native);
}

bool DisplayController::UpdateWorkAreaOfDisplayNearestWindow(
    const aura::Window* window,
    const gfx::Insets& insets) {
  const aura::Window* root_window = window->GetRootWindow();
  int64 id = GetRootWindowSettings(root_window)->display_id;
  // if id is |kInvaildDisplayID|, it's being deleted.
  DCHECK(id != gfx::Display::kInvalidDisplayID);
  return GetDisplayManager()->UpdateWorkAreaOfDisplay(id, insets);
}

void DisplayController::OnDisplayAdded(const gfx::Display& display) {
  if (primary_tree_host_for_replace_) {
    DCHECK(window_tree_hosts_.empty());
    primary_display_id = display.id();
    window_tree_hosts_[display.id()] = primary_tree_host_for_replace_;
    GetRootWindowSettings(GetWindow(primary_tree_host_for_replace_))
        ->display_id = display.id();
    primary_tree_host_for_replace_ = NULL;
    const DisplayInfo& display_info =
        GetDisplayManager()->GetDisplayInfo(display.id());
    AshWindowTreeHost* ash_host = window_tree_hosts_[display.id()];
    ash_host->AsWindowTreeHost()->SetBounds(display_info.bounds_in_native());
    SetDisplayPropertiesOnHost(ash_host, display);
  } else {
    if (primary_display_id == gfx::Display::kInvalidDisplayID)
      primary_display_id = display.id();
    DCHECK(!window_tree_hosts_.empty());
    AshWindowTreeHost* ash_host = AddWindowTreeHostForDisplay(
        display, AshWindowTreeHostInitParams());
    RootWindowController::CreateForSecondaryDisplay(ash_host);
  }
}

void DisplayController::OnDisplayRemoved(const gfx::Display& display) {
  AshWindowTreeHost* host_to_delete = window_tree_hosts_[display.id()];
  CHECK(host_to_delete) << display.ToString();

  // Display for root window will be deleted when the Primary RootWindow
  // is deleted by the Shell.
  window_tree_hosts_.erase(display.id());

  // When the primary root window's display is removed, move the primary
  // root to the other display.
  if (primary_display_id == display.id()) {
    // Temporarily store the primary root window in
    // |primary_root_window_for_replace_| when replacing the display.
    if (window_tree_hosts_.size() == 0) {
      primary_display_id = gfx::Display::kInvalidDisplayID;
      primary_tree_host_for_replace_ = host_to_delete;
      return;
    }
    DCHECK_EQ(1U, window_tree_hosts_.size());
    primary_display_id = ScreenUtil::GetSecondaryDisplay().id();
    AshWindowTreeHost* primary_host = host_to_delete;

    // Delete the other host instead.
    host_to_delete = window_tree_hosts_[primary_display_id];
    GetRootWindowSettings(GetWindow(host_to_delete))->display_id = display.id();

    // Setup primary root.
    window_tree_hosts_[primary_display_id] = primary_host;
    GetRootWindowSettings(GetWindow(primary_host))->display_id =
        primary_display_id;

    OnDisplayMetricsChanged(
        GetDisplayManager()->GetDisplayForId(primary_display_id),
        DISPLAY_METRIC_BOUNDS);
  }
  RootWindowController* controller =
      GetRootWindowController(GetWindow(host_to_delete));
  DCHECK(controller);
  controller->MoveWindowsTo(GetPrimaryRootWindow());
  // Delete most of root window related objects, but don't delete
  // root window itself yet because the stack may be using it.
  controller->Shutdown();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, controller);
}

void DisplayController::OnDisplayMetricsChanged(const gfx::Display& display,
                                                uint32_t metrics) {
  if (!(metrics & (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION |
                   DISPLAY_METRIC_DEVICE_SCALE_FACTOR)))
    return;

  const DisplayInfo& display_info =
      GetDisplayManager()->GetDisplayInfo(display.id());
  DCHECK(!display_info.bounds_in_native().IsEmpty());
  AshWindowTreeHost* ash_host = window_tree_hosts_[display.id()];
  ash_host->AsWindowTreeHost()->SetBounds(display_info.bounds_in_native());
  SetDisplayPropertiesOnHost(ash_host, display);
}

void DisplayController::OnHostResized(const aura::WindowTreeHost* host) {
  gfx::Display display = Shell::GetScreen()->GetDisplayNearestWindow(
      const_cast<aura::Window*>(host->window()));

  DisplayManager* display_manager = GetDisplayManager();
  if (display_manager->UpdateDisplayBounds(display.id(), host->GetBounds())) {
    mirror_window_controller_->UpdateWindow();
    cursor_window_controller_->UpdateContainer();
  }
}

void DisplayController::CreateOrUpdateNonDesktopDisplay(
    const DisplayInfo& info) {
  switch (GetDisplayManager()->second_display_mode()) {
    case DisplayManager::MIRRORING:
      mirror_window_controller_->UpdateWindow(info);
      cursor_window_controller_->UpdateContainer();
      virtual_keyboard_window_controller_->Close();
      break;
    case DisplayManager::VIRTUAL_KEYBOARD:
      mirror_window_controller_->Close();
      cursor_window_controller_->UpdateContainer();
      virtual_keyboard_window_controller_->UpdateWindow(info);
      break;
    case DisplayManager::EXTENDED:
      NOTREACHED();
  }
}

void DisplayController::CloseNonDesktopDisplay() {
  mirror_window_controller_->Close();
  cursor_window_controller_->UpdateContainer();
  virtual_keyboard_window_controller_->Close();
}

void DisplayController::PreDisplayConfigurationChange(bool clear_focus) {
  FOR_EACH_OBSERVER(Observer, observers_, OnDisplayConfigurationChanging());
  focus_activation_store_->Store(clear_focus);
  gfx::Screen* screen = Shell::GetScreen();
  gfx::Point point_in_screen = screen->GetCursorScreenPoint();
  gfx::Display display = screen->GetDisplayNearestPoint(point_in_screen);
  aura::Window* root_window = GetRootWindowForDisplayId(display.id());

  aura::client::ScreenPositionClient* client =
      aura::client::GetScreenPositionClient(root_window);
  client->ConvertPointFromScreen(root_window, &point_in_screen);
  root_window->GetHost()->ConvertPointToNativeScreen(&point_in_screen);
  cursor_location_in_native_coords_for_restore_ = point_in_screen;
}

void DisplayController::PostDisplayConfigurationChange() {
  if (limiter_)
    limiter_->SetThrottleTimeout(kAfterDisplayChangeThrottleTimeoutMs);

  focus_activation_store_->Restore();

  DisplayManager* display_manager = GetDisplayManager();
  DisplayLayoutStore* layout_store = display_manager->layout_store();
  if (display_manager->num_connected_displays() > 1) {
    DisplayIdPair pair = display_manager->GetCurrentDisplayIdPair();
    layout_store->UpdateMirrorStatus(pair, display_manager->IsMirrored());
    DisplayLayout layout = layout_store->GetRegisteredDisplayLayout(pair);

    if (Shell::GetScreen()->GetNumDisplays() > 1 ) {
      int64 primary_id = layout.primary_id;
      SetPrimaryDisplayId(
          primary_id == gfx::Display::kInvalidDisplayID ?
          pair.first : primary_id);
      // Update the primary_id in case the above call is
      // ignored. Happens when a) default layout's primary id
      // doesn't exist, or b) the primary_id has already been
      // set to the same and didn't update it.
      layout_store->UpdatePrimaryDisplayId(
          pair, Shell::GetScreen()->GetPrimaryDisplay().id());
    }
  }
  FOR_EACH_OBSERVER(Observer, observers_, OnDisplayConfigurationChanged());
  UpdateHostWindowNames();
  EnsurePointerInDisplays();
}

AshWindowTreeHost* DisplayController::AddWindowTreeHostForDisplay(
    const gfx::Display& display,
    const AshWindowTreeHostInitParams& init_params) {
  static int host_count = 0;
  const DisplayInfo& display_info =
      GetDisplayManager()->GetDisplayInfo(display.id());
  AshWindowTreeHostInitParams params_with_bounds(init_params);
  params_with_bounds.initial_bounds = display_info.bounds_in_native();
  AshWindowTreeHost* ash_host = AshWindowTreeHost::Create(params_with_bounds);
  aura::WindowTreeHost* host = ash_host->AsWindowTreeHost();

  host->window()->SetName(base::StringPrintf("RootWindow-%d", host_count++));
  host->window()->SetTitle(base::UTF8ToUTF16(display_info.name()));
  host->compositor()->SetBackgroundColor(SK_ColorBLACK);
  // No need to remove our observer observer because the DisplayController
  // outlives the host.
  host->AddObserver(this);
  InitRootWindowSettings(host->window())->display_id = display.id();
  host->InitHost();

  window_tree_hosts_[display.id()] = ash_host;
  SetDisplayPropertiesOnHost(ash_host, display);

#if defined(OS_CHROMEOS)
  static bool force_constrain_pointer_to_root =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshConstrainPointerToRoot);
  if (base::SysInfo::IsRunningOnChromeOS() || force_constrain_pointer_to_root)
    ash_host->ConfineCursorToRootWindow();
#endif
  return ash_host;
}

void DisplayController::OnFadeOutForSwapDisplayFinished() {
#if defined(OS_CHROMEOS)
  SetPrimaryDisplay(ScreenUtil::GetSecondaryDisplay());
  Shell::GetInstance()->display_configurator_animation()
      ->StartFadeInAnimation();
#endif
}

void DisplayController::SetMirrorModeAfterAnimation(bool mirror) {
  GetDisplayManager()->SetMirrorMode(mirror);
}

void DisplayController::UpdateHostWindowNames() {
#if defined(USE_X11)
  // crbug.com/120229 - set the window title for the primary dislpay
  // to "aura_root_0" so gtalk can find the primary root window to broadcast.
  // TODO(jhorwich) Remove this once Chrome supports window-based broadcasting.
  aura::Window* primary = Shell::GetPrimaryRootWindow();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (size_t i = 0; i < root_windows.size(); ++i) {
    std::string name =
        root_windows[i] == primary ? "aura_root_0" : "aura_root_x";
    gfx::AcceleratedWidget xwindow =
        root_windows[i]->GetHost()->GetAcceleratedWidget();
    XStoreName(gfx::GetXDisplay(), xwindow, name.c_str());
  }
#endif
}

}  // namespace ash
