// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_manager.h"

#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/display/display_controller.h"
#include "ash/host/root_window_host_factory.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/aura/window_property.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chromeos/display/output_configurator.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/aura/remote_root_window_host_win.h"
#endif

DECLARE_WINDOW_PROPERTY_TYPE(int64);

namespace ash {
namespace internal {
typedef std::vector<gfx::Display> DisplayList;
typedef std::vector<DisplayInfo> DisplayInfoList;

namespace {

// Default bounds for a display.
const int kDefaultHostWindowX = 200;
const int kDefaultHostWindowY = 200;
const int kDefaultHostWindowWidth = 1366;
const int kDefaultHostWindowHeight = 768;

struct DisplaySortFunctor {
  bool operator()(const gfx::Display& a, const gfx::Display& b) {
    return a.id() < b.id();
  }
};

struct DisplayInfoSortFunctor {
  bool operator()(const DisplayInfo& a, const DisplayInfo& b) {
    return a.id() < b.id();
  }
};

gfx::Display& GetInvalidDisplay() {
  static gfx::Display* invalid_display = new gfx::Display();
  return *invalid_display;
}

#if defined(OS_CHROMEOS)

int64 FindInternalDisplayID() {
  std::vector<XID> outputs;
  ui::GetOutputDeviceHandles(&outputs);
  std::vector<std::string> output_names = ui::GetOutputNames(outputs);
  for (size_t i = 0; i < output_names.size(); ++i) {
    if (chromeos::OutputConfigurator::IsInternalOutputName(
            output_names[i])) {
      uint16 manufacturer_id = 0;
      uint16 product_code = 0;
      ui::GetOutputDeviceData(
          outputs[i], &manufacturer_id, &product_code, NULL);
      return gfx::Display::GetID(manufacturer_id, product_code, i);
    }
  }
  return gfx::Display::kInvalidDisplayID;
}

#endif

}  // namespace

using aura::RootWindow;
using aura::Window;
using std::string;
using std::vector;

DEFINE_WINDOW_PROPERTY_KEY(int64, kDisplayIdKey,
                           gfx::Display::kInvalidDisplayID);

DisplayManager::DisplayManager() :
    force_bounds_changed_(false) {
  Init();
}

DisplayManager::~DisplayManager() {
}

// static
void DisplayManager::CycleDisplay() {
  Shell::GetInstance()->display_manager()->CycleDisplayImpl();
}

// static
void DisplayManager::ToggleDisplayScale() {
  Shell::GetInstance()->display_manager()->ScaleDisplayImpl();
}

bool DisplayManager::IsActiveDisplay(const gfx::Display& display) const {
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    if ((*iter).id() == display.id())
      return true;
  }
  return false;
}

bool DisplayManager::HasInternalDisplay() const {
  return gfx::Display::InternalDisplayId() != gfx::Display::kInvalidDisplayID;
}

bool DisplayManager::IsInternalDisplayId(int64 id) const {
  return gfx::Display::InternalDisplayId() == id;
}

bool DisplayManager::UpdateWorkAreaOfDisplayNearestWindow(
    const aura::Window* window,
    const gfx::Insets& insets) {
  const RootWindow* root = window->GetRootWindow();
  gfx::Display& display = FindDisplayForRootWindow(root);
  gfx::Rect old_work_area = display.work_area();
  display.UpdateWorkAreaFromInsets(insets);
  return old_work_area != display.work_area();
}

const gfx::Display& DisplayManager::GetDisplayForId(int64 id) const {
  return const_cast<DisplayManager*>(this)->FindDisplayForId(id);
}

const gfx::Display& DisplayManager::FindDisplayContainingPoint(
    const gfx::Point& point_in_screen) const {
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    const gfx::Display& display = *iter;
    if (display.bounds().Contains(point_in_screen))
      return display;
  }
  return GetInvalidDisplay();
}

void DisplayManager::SetOverscanInsets(int64 display_id,
                                       const gfx::Insets& insets_in_dip) {
  display_info_[display_id].SetOverscanInsets(true, insets_in_dip);
  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    display_info_list.push_back(GetDisplayInfo(*iter));
  }
  UpdateDisplays(display_info_list);
}

void DisplayManager::ClearCustomOverscanInsets(int64 display_id) {
  display_info_[display_id].clear_has_custom_overscan_insets();
  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    display_info_list.push_back(GetDisplayInfo(*iter));
  }
  UpdateDisplays(display_info_list);
}

gfx::Insets DisplayManager::GetOverscanInsets(int64 display_id) const {
  std::map<int64, DisplayInfo>::const_iterator it =
      display_info_.find(display_id);
  return (it != display_info_.end()) ?
      it->second.overscan_insets_in_dip() : gfx::Insets();
}

void DisplayManager::OnNativeDisplaysChanged(
    const std::vector<DisplayInfo>& updated_displays) {
  if (updated_displays.empty()) {
    // Don't update the displays when all displays are disconnected.
    // This happens when:
    // - the device is idle and powerd requested to turn off all displays.
    // - the device is suspended. (kernel turns off all displays)
    // - the internal display's brightness is set to 0 and no external
    //   display is connected.
    // - the internal display's brightness is 0 and external display is
    //   disconnected.
    // The display will be updated when one of displays is turned on, and the
    // display list will be updated correctly.
    return;
  }

  bool internal_display_connected = false;
  for (DisplayInfoList::const_iterator iter = updated_displays.begin();
       iter != updated_displays.end() && !internal_display_connected;
       ++iter) {
    internal_display_connected = IsInternalDisplayId(iter->id());
    if (internal_display_connected)
      internal_display_info_.reset(new DisplayInfo(*iter));
  }
  DisplayInfoList new_display_info_list = updated_displays;

  if (HasInternalDisplay() && !internal_display_connected) {
    if (!internal_display_info_.get()) {
      // TODO(oshima): Get has_custom value.
      internal_display_info_.reset(new DisplayInfo(
          gfx::Display::InternalDisplayId(),
          l10n_util::GetStringUTF8(IDS_ASH_INTERNAL_DISPLAY_NAME),
          false));
      internal_display_info_->SetBounds(gfx::Rect(0, 0, 800, 600));
    }
    new_display_info_list.push_back(*internal_display_info_.get());
  }

  UpdateDisplays(new_display_info_list);
}

void DisplayManager::UpdateDisplays(
    const std::vector<DisplayInfo>& updated_display_info_list) {
#if defined(OS_CHROMEOS)
  // Overscan is always enabled when not running on the device
  // in order for unit tests to work.
  bool can_overscan =
      !base::chromeos::IsRunningOnChromeOS() ||
      (Shell::GetInstance()->output_configurator()->output_state() !=
       chromeos::STATE_DUAL_MIRROR &&
       updated_display_info_list.size() == 1);
#else
  bool can_overscan = true;
#endif
  DisplayInfoList new_display_info_list = updated_display_info_list;
  std::sort(displays_.begin(), displays_.end(), DisplaySortFunctor());
  std::sort(new_display_info_list.begin(),
            new_display_info_list.end(),
            DisplayInfoSortFunctor());
  DisplayList removed_displays;
  std::vector<size_t> changed_display_indices;
  std::vector<size_t> added_display_indices;
  gfx::Display current_primary;
  if (DisplayController::HasPrimaryDisplay())
    current_primary = DisplayController::GetPrimaryDisplay();

  DisplayList::iterator curr_iter = displays_.begin();
  DisplayInfoList::const_iterator new_info_iter = new_display_info_list.begin();

  DisplayList new_displays;
  while (curr_iter != displays_.end() ||
         new_info_iter != new_display_info_list.end()) {
    if (curr_iter == displays_.end()) {
      // more displays in new list.
      added_display_indices.push_back(new_displays.size());
      InsertAndUpdateDisplayInfo(*new_info_iter, can_overscan);
      new_displays.push_back(
          CreateDisplayFromDisplayInfoById(new_info_iter->id()));
      ++new_info_iter;
    } else if (new_info_iter == new_display_info_list.end()) {
      // more displays in current list.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
    } else if (curr_iter->id() == new_info_iter->id()) {
      const gfx::Display& current_display = *curr_iter;
      // Copy the info because |CreateDisplayFromInfo| updates the instance.
      const DisplayInfo current_display_info = GetDisplayInfo(current_display);
      InsertAndUpdateDisplayInfo(*new_info_iter, can_overscan);
      gfx::Display new_display =
          CreateDisplayFromDisplayInfoById(new_info_iter->id());
      const DisplayInfo& new_display_info = GetDisplayInfo(new_display);
      if (force_bounds_changed_ ||
          (current_display_info.bounds_in_pixel() !=
           new_display_info.bounds_in_pixel()) ||
          (current_display.device_scale_factor() !=
           new_display.device_scale_factor())) {
        changed_display_indices.push_back(new_displays.size());
      }

      new_display.UpdateWorkAreaFromInsets(current_display.GetWorkAreaInsets());
      new_displays.push_back(new_display);
      ++curr_iter;
      ++new_info_iter;
    } else if (curr_iter->id() < new_info_iter->id()) {
      // more displays in current list between ids, which means it is deleted.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
    } else {
      // more displays in new list between ids, which means it is added.
      added_display_indices.push_back(new_displays.size());
      InsertAndUpdateDisplayInfo(*new_info_iter, can_overscan);
      new_displays.push_back(
          CreateDisplayFromDisplayInfoById(new_info_iter->id()));
      ++new_info_iter;
    }
  }

  // Do not update |displays_| if there's nothing to be updated. Without this,
  // it will not update the display layout, which causes the bug
  // http://crbug.com/155948.
  if (changed_display_indices.empty() && added_display_indices.empty() &&
      removed_displays.empty()) {
    return;
  }

  displays_ = new_displays;

  // Temporarily add displays to be removed because display object
  // being removed are accessed during shutting down the root.
  displays_.insert(displays_.end(), removed_displays.begin(),
                   removed_displays.end());
  for (DisplayList::const_reverse_iterator iter = removed_displays.rbegin();
       iter != removed_displays.rend(); ++iter) {
    Shell::GetInstance()->screen()->NotifyDisplayRemoved(displays_.back());
    displays_.pop_back();
  }
  for (std::vector<size_t>::iterator iter = added_display_indices.begin();
       iter != added_display_indices.end(); ++iter) {
    Shell::GetInstance()->screen()->NotifyDisplayAdded(displays_[*iter]);
  }
  for (std::vector<size_t>::iterator iter = changed_display_indices.begin();
       iter != changed_display_indices.end(); ++iter) {
    Shell::GetInstance()->screen()->NotifyBoundsChanged(displays_[*iter]);
  }
  EnsurePointerInDisplays();

#if defined(USE_X11) && defined(OS_CHROMEOS)
  if (!changed_display_indices.empty() && base::chromeos::IsRunningOnChromeOS())
    ui::ClearX11DefaultRootWindow();
#endif
}

RootWindow* DisplayManager::CreateRootWindowForDisplay(
    const gfx::Display& display) {
  static int root_window_count = 0;
  const gfx::Rect& bounds_in_pixel = GetDisplayInfo(display).bounds_in_pixel();
  RootWindow::CreateParams params(bounds_in_pixel);
  params.host = Shell::GetInstance()->root_window_host_factory()->
      CreateRootWindowHost(bounds_in_pixel);
  aura::RootWindow* root_window = new aura::RootWindow(params);
  root_window->SetName(StringPrintf("RootWindow-%d", root_window_count++));

  // No need to remove RootWindowObserver because
  // the DisplayManager object outlives RootWindow objects.
  root_window->AddRootWindowObserver(this);
  root_window->SetProperty(kDisplayIdKey, display.id());
  root_window->Init();
  return root_window;
}

gfx::Display* DisplayManager::GetDisplayAt(size_t index) {
  return index < displays_.size() ? &displays_[index] : NULL;
}

const gfx::Display* DisplayManager::GetPrimaryDisplayCandidate() const {
  const gfx::Display* primary_candidate = &displays_[0];
#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS()) {
    // On ChromeOS device, root windows are stacked vertically, and
    // default primary is the one on top.
    int count = GetNumDisplays();
    int y = GetDisplayInfo(*primary_candidate).bounds_in_pixel().y();
    for (int i = 1; i < count; ++i) {
      const gfx::Display* display = &displays_[i];
      const DisplayInfo& display_info = GetDisplayInfo(*display);
      if (display->IsInternal()) {
        primary_candidate = display;
        break;
      } else if (display_info.bounds_in_pixel().y() < y) {
        primary_candidate = display;
        y = display_info.bounds_in_pixel().y();
      }
    }
  }
#endif
  return primary_candidate;
}

size_t DisplayManager::GetNumDisplays() const {
  return displays_.size();
}

const gfx::Display& DisplayManager::GetDisplayNearestWindow(
    const Window* window) const {
  if (!window)
    return DisplayController::GetPrimaryDisplay();
  const RootWindow* root = window->GetRootWindow();
  DisplayManager* manager = const_cast<DisplayManager*>(this);
  return root ?
      manager->FindDisplayForRootWindow(root) :
      DisplayController::GetPrimaryDisplay();
}

const gfx::Display& DisplayManager::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  // Fallback to the primary display if there is no root display containing
  // the |point|.
  const gfx::Display& display = FindDisplayContainingPoint(point);
  return display.is_valid() ? display : DisplayController::GetPrimaryDisplay();
}

const gfx::Display& DisplayManager::GetDisplayMatching(
    const gfx::Rect& rect) const {
  if (rect.IsEmpty())
    return GetDisplayNearestPoint(rect.origin());

  int max = 0;
  const gfx::Display* matching = 0;
  for (std::vector<gfx::Display>::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    const gfx::Display& display = *iter;
    gfx::Rect intersect = gfx::IntersectRects(display.bounds(), rect);
    int area = intersect.width() * intersect.height();
    if (area > max) {
      max = area;
      matching = &(*iter);
    }
  }
  // Fallback to the primary display if there is no matching display.
  return matching ? *matching : DisplayController::GetPrimaryDisplay();
}

const DisplayInfo& DisplayManager::GetDisplayInfo(
    const gfx::Display& display) const {
  std::map<int64, DisplayInfo>::const_iterator iter =
      display_info_.find(display.id());
  CHECK(iter != display_info_.end());
  return iter->second;
}

std::string DisplayManager::GetDisplayNameFor(
    const gfx::Display& display) {
  if (!display.is_valid())
    return l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);

  std::map<int64, DisplayInfo>::const_iterator iter =
      display_info_.find(display.id());
  if (iter != display_info_.end() && !iter->second.name().empty())
    return iter->second.name();

  return base::StringPrintf("Display %d", static_cast<int>(display.id()));
}

void DisplayManager::OnRootWindowResized(const aura::RootWindow* root,
                                         const gfx::Size& old_size) {
  bool user_may_change_root = false;
#if defined(OS_CHROMEOS)
  user_may_change_root = !base::chromeos::IsRunningOnChromeOS();
#endif
  if (user_may_change_root) {
    gfx::Display& display = FindDisplayForRootWindow(root);
    if (display.size() != root->GetHostSize()) {
      display.SetSize(root->GetHostSize());
      display_info_[display.id()].UpdateBounds(
          gfx::Rect(root->GetHostOrigin(), root->GetHostSize()));
      Shell::GetInstance()->screen()->NotifyBoundsChanged(display);
    }
  }
}

void DisplayManager::Init() {
#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS())
    gfx::Display::SetInternalDisplayId(FindInternalDisplayID());
#endif

  // TODO(oshima): Move this logic to DisplayChangeObserver.
  const string size_str = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAshHostWindowBounds);
  vector<string> parts;
  base::SplitString(size_str, ',', &parts);
  for (vector<string>::const_iterator iter = parts.begin();
       iter != parts.end(); ++iter) {
    AddDisplayFromSpec(*iter);
  }
  if (displays_.empty())
    AddDisplayFromSpec(std::string() /* default */);
}

void DisplayManager::CycleDisplayImpl() {
  DCHECK(!displays_.empty());
  std::vector<DisplayInfo> new_display_info_list;
  new_display_info_list.push_back(
      GetDisplayInfo(DisplayController::GetPrimaryDisplay()));
  // Add if there is only one display.
  if (displays_.size() == 1) {
    // Layout the 2nd display below the primary as with the real device.
    aura::RootWindow* primary = Shell::GetPrimaryRootWindow();
    gfx::Rect host_bounds =
        gfx::Rect(primary->GetHostOrigin(),  primary->GetHostSize());
    new_display_info_list.push_back(DisplayInfo::CreateFromSpec(
        StringPrintf("%d+%d-500x400", host_bounds.x(), host_bounds.bottom())));
  }
  OnNativeDisplaysChanged(new_display_info_list);
}

void DisplayManager::ScaleDisplayImpl() {
  DCHECK(!displays_.empty());
  std::vector<DisplayInfo> new_display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    DisplayInfo display_info = GetDisplayInfo(*iter);
    display_info.set_device_scale_factor(
        display_info.device_scale_factor() == 1.0f ? 2.0f : 1.0f);
    new_display_info_list.push_back(display_info);
  }
  OnNativeDisplaysChanged(new_display_info_list);
}

gfx::Display& DisplayManager::FindDisplayForRootWindow(
    const aura::RootWindow* root_window) {
  int64 id = root_window->GetProperty(kDisplayIdKey);
  // if id is |kInvaildDisplayID|, it's being deleted.
  DCHECK(id != gfx::Display::kInvalidDisplayID);
  gfx::Display& display = FindDisplayForId(id);
  DCHECK(display.is_valid());
  return display;
}

gfx::Display& DisplayManager::FindDisplayForId(int64 id) {
  for (DisplayList::iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    if ((*iter).id() == id)
      return *iter;
  }
  DLOG(WARNING) << "Could not find display:" << id;
  return GetInvalidDisplay();
}

void DisplayManager::AddDisplayFromSpec(const std::string& spec) {
  DisplayInfo display_info = DisplayInfo::CreateFromSpec(spec);
  InsertAndUpdateDisplayInfo(display_info, false);
  gfx::Display display = CreateDisplayFromDisplayInfoById(display_info.id());
  displays_.push_back(display);
}

void DisplayManager::EnsurePointerInDisplays() {
  // Don't try to move the pointer during the boot/startup.
  if (!DisplayController::HasPrimaryDisplay())
    return;
  gfx::Point location_in_screen = Shell::GetScreen()->GetCursorScreenPoint();
  gfx::Point target_location;
  int64 closest_distance_squared = -1;

  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    const gfx::Rect& display_bounds = iter->bounds();

    if (display_bounds.Contains(location_in_screen)) {
      target_location = location_in_screen;
      break;
    }
    gfx::Point center = display_bounds.CenterPoint();
    // Use the distance squared from the center of the dislay. This is not
    // exactly "closest" display, but good enough to pick one
    // appropriate (and there are at most two displays).
    // We don't care about actual distance, only relative to other displays, so
    // using the LengthSquared() is cheaper than Length().
    int64 distance_squared = (center - location_in_screen).LengthSquared();
    if (closest_distance_squared < 0 ||
        closest_distance_squared > distance_squared) {
      target_location = center;
      closest_distance_squared = distance_squared;
    }
  }

  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();
  aura::client::ScreenPositionClient* client =
      aura::client::GetScreenPositionClient(root_window);
  client->ConvertPointFromScreen(root_window, &target_location);

  root_window->MoveCursorTo(target_location);
}

void DisplayManager::InsertAndUpdateDisplayInfo(const DisplayInfo& new_info,
                                                bool can_overscan) {
  std::map<int64, DisplayInfo>::iterator info =
      display_info_.find(new_info.id());
  if (info != display_info_.end())
    info->second.CopyFromNative(new_info);
  else
    display_info_[new_info.id()] = new_info;

  display_info_[new_info.id()].UpdateOverscanInfo(can_overscan);
}

gfx::Display DisplayManager::CreateDisplayFromDisplayInfoById(int64 id) {
  DCHECK(display_info_.find(id) != display_info_.end());
  const DisplayInfo& display_info = display_info_[id];

  gfx::Display new_display(display_info.id());
  new_display.SetScaleAndBounds(
      display_info.device_scale_factor(), display_info.bounds_in_pixel());

  // If the display is primary, then simply set the origin to (0,0).
  // The secondary display's bounds will be updated by
  // |DisplayController::UpdateDisplayBoundsForLayout|, so no need
  // to change there.
  if (DisplayController::HasPrimaryDisplay()  &&
      display_info.id() == DisplayController::GetPrimaryDisplay().id()) {
    new_display.set_bounds(gfx::Rect(new_display.bounds().size()));
  }
  return new_display;
}

}  // namespace internal
}  // namespace ash
