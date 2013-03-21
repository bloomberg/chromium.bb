// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"

#include <algorithm>

#include "ash/ash_switches.h"
#include "ash/display/display_manager.h"
#include "ash/host/root_window_host_factory.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/json/json_value_converter.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

#if defined(OS_CHROMEOS)
#include "ash/display/output_configurator_animation.h"
#include "base/chromeos/chromeos_version.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "chromeos/display/output_configurator.h"
#include "ui/base/x/x11_util.h"

// Including this at the bottom to avoid other
// potential conflict with chrome headers.
#include <X11/extensions/Xrandr.h>
#undef RootWindow
#endif  // defined(OS_CHROMEOS)

DECLARE_WINDOW_PROPERTY_TYPE(gfx::Display::Rotation);

namespace ash {
namespace {

DEFINE_WINDOW_PROPERTY_KEY(gfx::Display::Rotation, kRotationKey,
                           gfx::Display::ROTATE_0);

// Primary display stored in global object as it can be
// accessed after Shell is deleted. A separate display instance is created
// during the shutdown instead of always keeping two display instances
// (one here and another one in display_manager) in sync, which is error prone.
int64 primary_display_id = gfx::Display::kInvalidDisplayID;
gfx::Display* primary_display_for_shutdown = NULL;
// Keeps the number of displays during the shutdown after
// ash::Shell:: is deleted.
int num_displays_for_shutdown = -1;

// The maximum value for 'offset' in DisplayLayout in case of outliers.  Need
// to change this value in case to support even larger displays.
const int kMaxValidOffset = 10000;

// The number of pixels to overlap between the primary and secondary displays,
// in case that the offset value is too large.
const int kMinimumOverlapForInvalidOffset = 100;

// Specifies how long the display change should have been disabled
// after each display change operations.
// |kCycleDisplayThrottleTimeoutMs| is set to be longer to avoid
// changing the settings while the system is still configurating
// displays. It will be overriden by |kAfterDisplayChangeThrottleTimeoutMs|
// when the display change happens, so the actual timeout is much shorter.
const int64 kAfterDisplayChangeThrottleTimeoutMs = 500;
const int64 kCycleDisplayThrottleTimeoutMs = 4000;
const int64 kSwapDisplayThrottleTimeoutMs = 500;

bool GetPositionFromString(const base::StringPiece& position,
                           DisplayLayout::Position* field) {
  if (position == "top") {
    *field = DisplayLayout::TOP;
    return true;
  } else if (position == "bottom") {
    *field = DisplayLayout::BOTTOM;
    return true;
  } else if (position == "right") {
    *field = DisplayLayout::RIGHT;
    return true;
  } else if (position == "left") {
    *field = DisplayLayout::LEFT;
    return true;
  }
  LOG(ERROR) << "Invalid position value: " << position;
  return false;
}

std::string GetStringFromPosition(DisplayLayout::Position position) {
  switch (position) {
    case DisplayLayout::TOP:
      return std::string("top");
    case DisplayLayout::BOTTOM:
      return std::string("bottom");
    case DisplayLayout::RIGHT:
      return std::string("right");
    case DisplayLayout::LEFT:
      return std::string("left");
  }
  return std::string("unknown");
}

internal::DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

void RotateRootWindow(aura::RootWindow* root_window,
                      const gfx::Display& display,
                      const internal::DisplayInfo& info) {
  // TODO(oshima): Add animation. (crossfade+rotation, or just cross-fade)
#if defined(OS_WIN)
  // Windows 8 bots refused to resize the host window, and
  // updating the transform results in incorrectly resizing
  // the root window. Don't apply the transform unless
  // necessary so that unit tests pass on win8 bots.
  if (info.rotation() == root_window->GetProperty(kRotationKey))
    return;
  root_window->SetProperty(kRotationKey, info.rotation());
#endif
  gfx::Transform rotate;
  // The origin is (0, 0), so the translate width/height must be reduced by 1.
  switch (info.rotation()) {
    case gfx::Display::ROTATE_0:
      break;
    case gfx::Display::ROTATE_90:
      rotate.Translate(display.bounds().height() - 1, 0);
      rotate.Rotate(90);
      break;
    case gfx::Display::ROTATE_270:
      rotate.Translate(0, display.bounds().width() - 1);
      rotate.Rotate(270);
      break;
    case gfx::Display::ROTATE_180:
      rotate.Translate(display.bounds().width() - 1,
                       display.bounds().height() - 1);
      rotate.Rotate(180);
      break;
  }
  root_window->SetTransform(rotate);
}

void SetDisplayPropertiesOnHostWindow(aura::RootWindow* root,
                                      const gfx::Display& display) {
  internal::DisplayInfo info = GetDisplayManager()->GetDisplayInfo(display);
#if defined(OS_CHROMEOS)
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
  gfx::AcceleratedWidget xwindow = root->GetAcceleratedWidget();
  ui::SetIntProperty(xwindow, kInternalProp, kCARDINAL, internal);
  ui::SetIntProperty(xwindow, kRotationProp, kCARDINAL, xrandr_rotation);
  ui::SetIntProperty(xwindow,
                     kScaleFactorProp,
                     kCARDINAL,
                     100 * display.device_scale_factor());
#endif
  RotateRootWindow(root, display, info);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DisplayLayout

// static
DisplayLayout DisplayLayout::FromInts(int position, int offsets) {
  return DisplayLayout(static_cast<Position>(position), offsets);
}

DisplayLayout::DisplayLayout()
    : position(RIGHT),
      offset(0) {}

DisplayLayout::DisplayLayout(DisplayLayout::Position position, int offset)
    : position(position),
      offset(offset) {
  DCHECK_LE(TOP, position);
  DCHECK_GE(LEFT, position);

  // Set the default value to |position| in case position is invalid.  DCHECKs
  // above doesn't stop in Release builds.
  if (TOP > position || LEFT < position)
    this->position = RIGHT;

  DCHECK_GE(kMaxValidOffset, abs(offset));
}

DisplayLayout DisplayLayout::Invert() const {
  Position inverted_position = RIGHT;
  switch (position) {
    case TOP:
      inverted_position = BOTTOM;
      break;
    case BOTTOM:
      inverted_position = TOP;
      break;
    case RIGHT:
      inverted_position = LEFT;
      break;
    case LEFT:
      inverted_position = RIGHT;
      break;
  }
  return DisplayLayout(inverted_position, -offset);
}

// static
bool DisplayLayout::ConvertFromValue(const base::Value& value,
                                     DisplayLayout* layout) {
  base::JSONValueConverter<DisplayLayout> converter;
  return converter.Convert(value, layout);
}

// static
bool DisplayLayout::ConvertToValue(const DisplayLayout& layout,
                                   base::Value* value) {
  base::DictionaryValue* dict_value = NULL;
  if (!value->GetAsDictionary(&dict_value) || dict_value == NULL)
    return false;

  const std::string position_str = GetStringFromPosition(layout.position);
  dict_value->SetString("position", position_str);
  dict_value->SetInteger("offset", layout.offset);
  return true;
}

std::string DisplayLayout::ToString() const {
  const std::string position_str = GetStringFromPosition(position);
  return base::StringPrintf("%s, %d", position_str.c_str(), offset);
}

// static
void DisplayLayout::RegisterJSONConverter(
    base::JSONValueConverter<DisplayLayout>* converter) {
  converter->RegisterCustomField<Position>(
      "position", &DisplayLayout::position, &GetPositionFromString);
  converter->RegisterIntField("offset", &DisplayLayout::offset);
}

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
    : desired_primary_display_id_(gfx::Display::kInvalidDisplayID),
      primary_root_window_for_replace_(NULL) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
#if defined(OS_CHROMEOS)
  if (!command_line->HasSwitch(switches::kAshDisableDisplayChangeLimiter) &&
      base::chromeos::IsRunningOnChromeOS())
    limiter_.reset(new DisplayChangeLimiter);
#endif
  if (command_line->HasSwitch(switches::kAshSecondaryDisplayLayout)) {
    std::string value = command_line->GetSwitchValueASCII(
        switches::kAshSecondaryDisplayLayout);
    char layout;
    int offset = 0;
    if (sscanf(value.c_str(), "%c,%d", &layout, &offset) == 2) {
      if (layout == 't')
        default_display_layout_.position = DisplayLayout::TOP;
      else if (layout == 'b')
        default_display_layout_.position = DisplayLayout::BOTTOM;
      else if (layout == 'r')
        default_display_layout_.position = DisplayLayout::RIGHT;
      else if (layout == 'l')
        default_display_layout_.position = DisplayLayout::LEFT;
      default_display_layout_.offset = offset;
    }
  }
  // Reset primary display to make sure that tests don't use
  // stale display info from previous tests.
  primary_display_id = gfx::Display::kInvalidDisplayID;
  delete primary_display_for_shutdown;
  primary_display_for_shutdown = NULL;
  num_displays_for_shutdown = -1;

  Shell::GetScreen()->AddObserver(this);
}

DisplayController::~DisplayController() {
  DCHECK(primary_display_for_shutdown);
}

void DisplayController::Shutdown() {
  DCHECK(!primary_display_for_shutdown);
  primary_display_for_shutdown = new gfx::Display(
      GetDisplayManager()->GetDisplayForId(primary_display_id));
  num_displays_for_shutdown = GetDisplayManager()->GetNumDisplays();

  Shell::GetScreen()->RemoveObserver(this);
  // Delete all root window controllers, which deletes root window
  // from the last so that the primary root window gets deleted last.
  for (std::map<int64, aura::RootWindow*>::const_reverse_iterator it =
           root_windows_.rbegin(); it != root_windows_.rend(); ++it) {
    internal::RootWindowController* controller =
        GetRootWindowController(it->second);
    DCHECK(controller);
    delete controller;
  }
}

// static
const gfx::Display& DisplayController::GetPrimaryDisplay() {
  DCHECK_NE(primary_display_id, gfx::Display::kInvalidDisplayID);
  if (primary_display_for_shutdown)
    return *primary_display_for_shutdown;
  return GetDisplayManager()->GetDisplayForId(primary_display_id);
}

// static
int DisplayController::GetNumDisplays() {
  if (num_displays_for_shutdown >= 0)
    return num_displays_for_shutdown;
  return GetDisplayManager()->GetNumDisplays();
}

// static
bool DisplayController::HasPrimaryDisplay() {
  return primary_display_id != gfx::Display::kInvalidDisplayID;
}

void DisplayController::InitPrimaryDisplay() {
  const gfx::Display* primary_candidate =
      GetDisplayManager()->GetPrimaryDisplayCandidate();
  primary_display_id = primary_candidate->id();
  AddRootWindowForDisplay(*primary_candidate);
  UpdateDisplayBoundsForLayout();
}

void DisplayController::InitSecondaryDisplays() {
  internal::DisplayManager* display_manager = GetDisplayManager();
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    const gfx::Display* display = display_manager->GetDisplayAt(i);
    if (primary_display_id != display->id()) {
      aura::RootWindow* root = AddRootWindowForDisplay(*display);
      Shell::GetInstance()->InitRootWindowForSecondaryDisplay(root);
    }
  }
  UpdateDisplayBoundsForLayout();
}

void DisplayController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DisplayController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

aura::RootWindow* DisplayController::GetPrimaryRootWindow() {
  DCHECK(!root_windows_.empty());
  return root_windows_[primary_display_id];
}

aura::RootWindow* DisplayController::GetRootWindowForDisplayId(int64 id) {
  return root_windows_[id];
}

void DisplayController::CloseChildWindows() {
  for (std::map<int64, aura::RootWindow*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    aura::RootWindow* root_window = it->second;
    internal::RootWindowController* controller =
        GetRootWindowController(root_window);
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

std::vector<aura::RootWindow*> DisplayController::GetAllRootWindows() {
  std::vector<aura::RootWindow*> windows;
  for (std::map<int64, aura::RootWindow*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    DCHECK(it->second);
    if (GetRootWindowController(it->second))
      windows.push_back(it->second);
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

void DisplayController::ClearCustomOverscanInsets(int64 display_id) {
  GetDisplayManager()->ClearCustomOverscanInsets(display_id);
}

std::vector<internal::RootWindowController*>
DisplayController::GetAllRootWindowControllers() {
  std::vector<internal::RootWindowController*> controllers;
  for (std::map<int64, aura::RootWindow*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    internal::RootWindowController* controller =
        GetRootWindowController(it->second);
    if (controller)
      controllers.push_back(controller);
  }
  return controllers;
}

void DisplayController::SetDefaultDisplayLayout(const DisplayLayout& layout) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kAshSecondaryDisplayLayout) &&
      (default_display_layout_.position != layout.position ||
       default_display_layout_.offset != layout.offset)) {
    default_display_layout_ = layout;
    NotifyDisplayConfigurationChanging();
    UpdateDisplayBoundsForLayout();
    NotifyDisplayConfigurationChanged();
  }
}

void DisplayController::RegisterLayoutForDisplayIdPair(
    int64 id1,
    int64 id2,
    const DisplayLayout& layout) {
  RegisterLayoutForDisplayIdPairInternal(id1, id2, layout, true);
}

void DisplayController::RegisterLayoutForDisplayId(
    int64 id,
    const DisplayLayout& layout) {
  int64 first_id = gfx::Display::InternalDisplayId();
  if (first_id == gfx::Display::kInvalidDisplayID)
    first_id = GetDisplayManager()->first_display_id();
  // Caveat: This doesn't work if the machine booted with
  // no display.
  // Ignore if the layout was registered for the internal or
  // 1st display.
  if (first_id != id)
    RegisterLayoutForDisplayIdPairInternal(first_id, id, layout, false);
}

void DisplayController::SetLayoutForCurrentDisplays(
    const DisplayLayout& layout_relative_to_primary) {
  DCHECK_EQ(2U, GetDisplayManager()->GetNumDisplays());
  if (GetDisplayManager()->GetNumDisplays() < 2)
    return;
  const gfx::Display& primary = GetPrimaryDisplay();
  const DisplayIdPair pair = GetCurrentDisplayIdPair();
  // Invert if the primary was swapped.
  DisplayLayout to_set = pair.first == primary.id() ?
      layout_relative_to_primary : layout_relative_to_primary.Invert();

  const DisplayLayout& current_layout = paired_layouts_[pair];
  if (to_set.position != current_layout.position ||
      to_set.offset != current_layout.offset) {
    paired_layouts_[pair] = to_set;
    NotifyDisplayConfigurationChanging();
    UpdateDisplayBoundsForLayout();
    NotifyDisplayConfigurationChanged();
  }
}

DisplayLayout DisplayController::GetCurrentDisplayLayout() const {
  DCHECK_EQ(2U, GetDisplayManager()->GetNumDisplays());
  // Invert if the primary was swapped.
  if (GetDisplayManager()->GetNumDisplays() > 1) {
    DisplayIdPair pair = GetCurrentDisplayIdPair();
    DisplayLayout layout = GetRegisteredDisplayLayout(pair);
    const gfx::Display& primary = GetPrimaryDisplay();
    // Invert if the primary was swapped.
    return pair.first == primary.id() ? layout : layout.Invert();
  }
  // On release build, just fallback to default instead of blowing up.
  return default_display_layout_;
}

DisplayIdPair DisplayController::GetCurrentDisplayIdPair() const {
  const gfx::Display& primary = GetPrimaryDisplay();
  const gfx::Display& secondary = ScreenAsh::GetSecondaryDisplay();
  DisplayIdPair pair;
  if (primary.IsInternal() ||
      GetDisplayManager()->first_display_id() == primary.id()) {
    pair = std::make_pair(primary.id(), secondary.id());
  } else {
    // Display has been Swapped.
    pair = std::make_pair(secondary.id(), primary.id());
  }
  return pair;
}

DisplayLayout DisplayController::GetRegisteredDisplayLayout(
    const DisplayIdPair& pair) const {
  std::map<DisplayIdPair, DisplayLayout>::const_iterator iter =
      paired_layouts_.find(pair);
  return iter != paired_layouts_.end() ? iter->second : default_display_layout_;
}

void DisplayController::CycleDisplayMode() {
  if (limiter_.get()) {
    if  (limiter_->IsThrottled())
      return;
    limiter_->SetThrottleTimeout(kCycleDisplayThrottleTimeoutMs);
  }
#if defined(OS_CHROMEOS)
  Shell* shell = Shell::GetInstance();
  if (!base::chromeos::IsRunningOnChromeOS()) {
    internal::DisplayManager::CycleDisplay();
  } else if (shell->output_configurator()->connected_output_count() > 1) {
    internal::OutputConfiguratorAnimation* animation =
        shell->output_configurator_animation();
    animation->StartFadeOutAnimation(base::Bind(
        base::IgnoreResult(&chromeos::OutputConfigurator::CycleDisplayMode),
        base::Unretained(shell->output_configurator())));
  }
#endif
}

void DisplayController::SwapPrimaryDisplay() {
  if (limiter_.get()) {
    if  (limiter_->IsThrottled())
      return;
    limiter_->SetThrottleTimeout(kSwapDisplayThrottleTimeoutMs);
  }

  if (Shell::GetScreen()->GetNumDisplays() > 1) {
#if defined(OS_CHROMEOS)
    internal::OutputConfiguratorAnimation* animation =
        Shell::GetInstance()->output_configurator_animation();
    if (animation) {
      animation->StartFadeOutAnimation(base::Bind(
          &DisplayController::OnFadeOutForSwapDisplayFinished,
          base::Unretained(this)));
    } else {
      SetPrimaryDisplay(ScreenAsh::GetSecondaryDisplay());
    }
#else
    SetPrimaryDisplay(ScreenAsh::GetSecondaryDisplay());
#endif
  }
}

void DisplayController::SetPrimaryDisplayId(int64 id) {
  desired_primary_display_id_ = id;

  if (desired_primary_display_id_ == primary_display_id)
    return;

  internal::DisplayManager* display_manager = GetDisplayManager();
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    gfx::Display* display = display_manager->GetDisplayAt(i);
    if (display->id() == id) {
      SetPrimaryDisplay(*display);
      break;
    }
  }
}

void DisplayController::SetPrimaryDisplay(
    const gfx::Display& new_primary_display) {
  internal::DisplayManager* display_manager = GetDisplayManager();
  DCHECK(new_primary_display.is_valid());
  DCHECK(display_manager->IsActiveDisplay(new_primary_display));

  if (!new_primary_display.is_valid() ||
      !display_manager->IsActiveDisplay(new_primary_display)) {
    LOG(ERROR) << "Invalid or non-existent display is requested:"
               << new_primary_display.ToString();
    return;
  }

  if (primary_display_id == new_primary_display.id() ||
      root_windows_.size() < 2) {
    return;
  }

  aura::RootWindow* non_primary_root = root_windows_[new_primary_display.id()];
  LOG_IF(ERROR, !non_primary_root)
      << "Unknown display is requested in SetPrimaryDisplay: id="
      << new_primary_display.id();
  if (!non_primary_root)
    return;

  gfx::Display old_primary_display = GetPrimaryDisplay();

  // Swap root windows between current and new primary display.
  aura::RootWindow* primary_root = root_windows_[primary_display_id];
  DCHECK(primary_root);
  DCHECK_NE(primary_root, non_primary_root);

  root_windows_[new_primary_display.id()] = primary_root;
  primary_root->SetProperty(internal::kDisplayIdKey, new_primary_display.id());

  root_windows_[old_primary_display.id()] = non_primary_root;
  non_primary_root->SetProperty(internal::kDisplayIdKey,
                                old_primary_display.id());

  primary_display_id = new_primary_display.id();
  desired_primary_display_id_ = primary_display_id;

  display_manager->UpdateWorkAreaOfDisplayNearestWindow(
      primary_root, old_primary_display.GetWorkAreaInsets());
  display_manager->UpdateWorkAreaOfDisplayNearestWindow(
      non_primary_root, new_primary_display.GetWorkAreaInsets());

  // Update the dispay manager with new display info.
  std::vector<internal::DisplayInfo> display_info_list;
  display_info_list.push_back(display_manager->GetDisplayInfo(
      display_manager->GetDisplayForId(primary_display_id)));
  display_info_list.push_back(display_manager->GetDisplayInfo(
      *GetSecondaryDisplay()));
  GetDisplayManager()->set_force_bounds_changed(true);
  GetDisplayManager()->UpdateDisplays(display_info_list);
  GetDisplayManager()->set_force_bounds_changed(false);
}

gfx::Display* DisplayController::GetSecondaryDisplay() {
  internal::DisplayManager* display_manager = GetDisplayManager();
  CHECK_EQ(2U, display_manager->GetNumDisplays());
  return display_manager->GetDisplayAt(0)->id() == primary_display_id ?
      display_manager->GetDisplayAt(1) : display_manager->GetDisplayAt(0);
}

void DisplayController::OnDisplayBoundsChanged(const gfx::Display& display) {
  if (limiter_.get())
    limiter_->SetThrottleTimeout(kAfterDisplayChangeThrottleTimeoutMs);
  const internal::DisplayInfo& display_info =
      GetDisplayManager()->GetDisplayInfo(display);
  DCHECK(!display_info.bounds_in_pixel().IsEmpty());

  UpdateDisplayBoundsForLayout();
  aura::RootWindow* root = root_windows_[display.id()];
  root->SetHostBoundsAndInsetsAndRootWindowScale(
      display_info.bounds_in_pixel(),
      display_info.GetOverscanInsetsInPixel(),
      display_info.ui_scale());
  SetDisplayPropertiesOnHostWindow(root, display);
}

void DisplayController::OnDisplayAdded(const gfx::Display& display) {
  if (limiter_.get())
    limiter_->SetThrottleTimeout(kAfterDisplayChangeThrottleTimeoutMs);

  if (primary_root_window_for_replace_) {
    DCHECK(root_windows_.empty());
    primary_display_id = display.id();
    root_windows_[display.id()] = primary_root_window_for_replace_;
    primary_root_window_for_replace_->SetProperty(
        internal::kDisplayIdKey, display.id());
    primary_root_window_for_replace_ = NULL;
    UpdateDisplayBoundsForLayout();
    const internal::DisplayInfo& display_info =
        GetDisplayManager()->GetDisplayInfo(display);
    root_windows_[display.id()]->SetHostBoundsAndInsetsAndRootWindowScale(
        display_info.bounds_in_pixel(),
        display_info.GetOverscanInsetsInPixel(),
        display_info.ui_scale());
  } else {
    DCHECK(!root_windows_.empty());
    aura::RootWindow* root = AddRootWindowForDisplay(display);
    UpdateDisplayBoundsForLayout();
    if (desired_primary_display_id_ == display.id())
      SetPrimaryDisplay(display);
    Shell::GetInstance()->InitRootWindowForSecondaryDisplay(root);
  }
}

void DisplayController::OnDisplayRemoved(const gfx::Display& display) {
  if (limiter_.get())
    limiter_->SetThrottleTimeout(kAfterDisplayChangeThrottleTimeoutMs);

  aura::RootWindow* root_to_delete = root_windows_[display.id()];
  DCHECK(root_to_delete) << display.ToString();

  // Display for root window will be deleted when the Primary RootWindow
  // is deleted by the Shell.
  root_windows_.erase(display.id());

  // When the primary root window's display is removed, move the primary
  // root to the other display.
  if (primary_display_id == display.id()) {
    // Temporarily store the primary root window in
    // |primary_root_window_for_replace_| when replacing the display.
    if (root_windows_.size() == 0) {
      primary_display_id = gfx::Display::kInvalidDisplayID;
      primary_root_window_for_replace_ = root_to_delete;
      return;
    }
    DCHECK_EQ(1U, root_windows_.size());
    primary_display_id = GetSecondaryDisplay()->id();
    aura::RootWindow* primary_root = root_to_delete;

    // Delete the other root instead.
    root_to_delete = root_windows_[primary_display_id];
    root_to_delete->SetProperty(internal::kDisplayIdKey, display.id());

    // Setup primary root.
    root_windows_[primary_display_id] = primary_root;
    primary_root->SetProperty(internal::kDisplayIdKey, primary_display_id);

    OnDisplayBoundsChanged(
        GetDisplayManager()->GetDisplayForId(primary_display_id));
  }
  internal::RootWindowController* controller =
      GetRootWindowController(root_to_delete);
  DCHECK(controller);
  controller->MoveWindowsTo(GetPrimaryRootWindow());
  // Delete most of root window related objects, but don't delete
  // root window itself yet because the stack may be using it.
  controller->Shutdown();
  MessageLoop::current()->DeleteSoon(FROM_HERE, controller);
}

aura::RootWindow* DisplayController::CreateRootWindowForDisplay(
    const gfx::Display& display) {
  static int root_window_count = 0;
  const internal::DisplayInfo& display_info =
      GetDisplayManager()->GetDisplayInfo(display);
  const gfx::Rect& bounds_in_pixel = display_info.bounds_in_pixel();
  aura::RootWindow::CreateParams params(bounds_in_pixel);
  params.host = Shell::GetInstance()->root_window_host_factory()->
      CreateRootWindowHost(bounds_in_pixel);
  params.initial_insets = display_info.GetOverscanInsetsInPixel();
  params.initial_root_window_scale = display_info.ui_scale();
  aura::RootWindow* root_window = new aura::RootWindow(params);
  root_window->SetName(
      base::StringPrintf("RootWindow-%d", root_window_count++));

  // No need to remove RootWindowObserver because
  // the DisplayManager object outlives RootWindow objects.
  root_window->AddRootWindowObserver(GetDisplayManager());
  root_window->SetProperty(internal::kDisplayIdKey, display.id());
  root_window->Init();
  return root_window;
}

aura::RootWindow* DisplayController::AddRootWindowForDisplay(
    const gfx::Display& display) {
  aura::RootWindow* root = CreateRootWindowForDisplay(display);
  root_windows_[display.id()] = root;
  SetDisplayPropertiesOnHostWindow(root, display);

#if defined(OS_CHROMEOS)
  static bool force_constrain_pointer_to_root =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshConstrainPointerToRoot);
  if (base::chromeos::IsRunningOnChromeOS() || force_constrain_pointer_to_root)
    root->ConfineCursorToWindow();
#endif
  return root;
}

void DisplayController::UpdateDisplayBoundsForLayout() {
  if (Shell::GetScreen()->GetNumDisplays() <= 1)
    return;

  DCHECK_EQ(2, Shell::GetScreen()->GetNumDisplays());
  const gfx::Rect& primary_bounds = GetPrimaryDisplay().bounds();

  gfx::Display* secondary_display = GetSecondaryDisplay();
  const gfx::Rect& secondary_bounds = secondary_display->bounds();
  gfx::Point new_secondary_origin = primary_bounds.origin();

  const DisplayLayout layout = GetCurrentDisplayLayout();
  DisplayLayout::Position position = layout.position;

  // Ignore the offset in case the secondary display doesn't share edges with
  // the primary display.
  int offset = layout.offset;
  if (position == DisplayLayout::TOP || position == DisplayLayout::BOTTOM) {
    offset = std::min(
        offset, primary_bounds.width() - kMinimumOverlapForInvalidOffset);
    offset = std::max(
        offset, -secondary_bounds.width() + kMinimumOverlapForInvalidOffset);
  } else {
    offset = std::min(
        offset, primary_bounds.height() - kMinimumOverlapForInvalidOffset);
    offset = std::max(
        offset, -secondary_bounds.height() + kMinimumOverlapForInvalidOffset);
  }
  switch (position) {
    case DisplayLayout::TOP:
      new_secondary_origin.Offset(offset, -secondary_bounds.height());
      break;
    case DisplayLayout::RIGHT:
      new_secondary_origin.Offset(primary_bounds.width(), offset);
      break;
    case DisplayLayout::BOTTOM:
      new_secondary_origin.Offset(offset, primary_bounds.height());
      break;
    case DisplayLayout::LEFT:
      new_secondary_origin.Offset(-secondary_bounds.width(), offset);
      break;
  }
  gfx::Insets insets = secondary_display->GetWorkAreaInsets();
  secondary_display->set_bounds(
      gfx::Rect(new_secondary_origin, secondary_bounds.size()));
  secondary_display->UpdateWorkAreaFromInsets(insets);
}

void DisplayController::NotifyDisplayConfigurationChanging() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDisplayConfigurationChanging());
}

void DisplayController::NotifyDisplayConfigurationChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDisplayConfigurationChanged());
}

void DisplayController::RegisterLayoutForDisplayIdPairInternal(
    int64 id1,
    int64 id2,
    const DisplayLayout& layout,
    bool override) {
  DisplayIdPair pair = std::make_pair(id1, id2);
  if (override || paired_layouts_.find(pair) == paired_layouts_.end())
    paired_layouts_[pair] = layout;
}

void DisplayController::OnFadeOutForSwapDisplayFinished() {
#if defined(OS_CHROMEOS)
  SetPrimaryDisplay(ScreenAsh::GetSecondaryDisplay());
  Shell::GetInstance()->output_configurator_animation()->StartFadeInAnimation();
#endif
}

}  // namespace ash
