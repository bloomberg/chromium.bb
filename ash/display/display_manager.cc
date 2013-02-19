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
typedef std::vector<gfx::Display> DisplayList;

namespace ash {
namespace internal {
namespace {

// Default bounds for a display.
const int kDefaultHostWindowX = 200;
const int kDefaultHostWindowY = 200;
const int kDefaultHostWindowWidth = 1280;
const int kDefaultHostWindowHeight = 1024;

struct DisplaySortFunctor {
  bool operator()(const gfx::Display& a, const gfx::Display& b) {
    return a.id() < b.id();
  }
};

gfx::Display& GetInvalidDisplay() {
  static gfx::Display* invalid_display = new gfx::Display();
  return *invalid_display;
}

#if defined(OS_CHROMEOS)
int64 GetDisplayIdForOutput(XID output, int output_index) {
  uint16 manufacturer_id = 0;
  uint16 product_code = 0;
  ui::GetOutputDeviceData(
      output, &manufacturer_id, &product_code, NULL);
  return gfx::Display::GetID(manufacturer_id, product_code, output_index);
}
#endif

gfx::Insets GetDefaultDisplayOverscan(const gfx::Display& display) {
  // Currently we assume 5% overscan and hope for the best if TV claims it
  // overscan, but doesn't expose how much.
  int width = display.bounds().width() / 40;
  int height = display.bounds().height() / 40;
  return gfx::Insets(height, width, height, width);
}

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
  display_info_[display_id].overscan_insets_in_dip = insets_in_dip;
  display_info_[display_id].has_custom_overscan_insets = true;

  // Copies the |displays_| because UpdateDisplays() compares the passed
  // displays and its internal |displays_|.
  DisplayList displays = displays_;
  UpdateDisplays(displays);
}

gfx::Insets DisplayManager::GetOverscanInsets(int64 display_id) const {
  std::map<int64, DisplayInfo>::const_iterator it =
      display_info_.find(display_id);
  return (it != display_info_.end()) ?
      it->second.overscan_insets_in_dip : gfx::Insets();
}

void DisplayManager::OnNativeDisplaysChanged(
    const std::vector<gfx::Display>& updated_displays) {
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
  DisplayList new_displays = updated_displays;
  if (HasInternalDisplay()) {
    bool internal_display_connected = false;
    for (DisplayList::const_iterator iter = updated_displays.begin();
         iter != updated_displays.end(); ++iter) {
      if ((*iter).IsInternal()) {
        internal_display_connected = true;
        // Update the internal display cache.
        internal_display_.reset(new gfx::Display);
        *internal_display_.get() = *iter;
        break;
      }
    }
    // If the internal display wasn't connected, use the cached value.
    if (!internal_display_connected) {
      // Internal display may be reported as disconnect during startup time.
      if (!internal_display_.get()) {
        internal_display_.reset(
            new gfx::Display(gfx::Display::InternalDisplayId(),
                             gfx::Rect(800, 600)));
      }
      new_displays.push_back(*internal_display_.get());
    }
  } else {
    new_displays = updated_displays;
  }

  RefreshDisplayInfo();

  for (DisplayList::const_iterator iter = new_displays.begin();
       iter != new_displays.end(); ++iter) {
    std::map<int64, DisplayInfo>::iterator info =
        display_info_.find(iter->id());
    if (info != display_info_.end()) {
      info->second.original_bounds_in_pixel = iter->bounds_in_pixel();
      if (info->second.has_overscan && !info->second.has_custom_overscan_insets)
        info->second.overscan_insets_in_dip = GetDefaultDisplayOverscan(*iter);
    } else {
      display_info_[iter->id()].original_bounds_in_pixel =
          iter->bounds_in_pixel();
    }
  }

  UpdateDisplays(new_displays);
}

void DisplayManager::UpdateDisplays(
    const std::vector<gfx::Display>& updated_displays) {
  DisplayList new_displays = updated_displays;
#if defined(OS_CHROMEOS)
  // Overscan is always enabled when not running on the device
  // in order for unit tests to work.
  bool can_overscan =
      !base::chromeos::IsRunningOnChromeOS() ||
      (Shell::GetInstance()->output_configurator()->output_state() !=
       chromeos::STATE_DUAL_MIRROR &&
       updated_displays.size() == 1);
#else
  bool can_overscan = true;
#endif
  if (can_overscan) {
    for (DisplayList::iterator iter = new_displays.begin();
         iter != new_displays.end(); ++iter) {
      std::map<int64, DisplayInfo>::const_iterator info =
          display_info_.find(iter->id());
      if (info != display_info_.end()) {
        gfx::Rect bounds = info->second.original_bounds_in_pixel;
        bounds.Inset(info->second.overscan_insets_in_dip.Scale(
            iter->device_scale_factor()));
        iter->SetScaleAndBounds(iter->device_scale_factor(), bounds);
      }
    }
  }

  std::sort(displays_.begin(), displays_.end(), DisplaySortFunctor());
  std::sort(new_displays.begin(), new_displays.end(), DisplaySortFunctor());
  DisplayList removed_displays;
  std::vector<size_t> changed_display_indices;
  std::vector<size_t> added_display_indices;
  gfx::Display current_primary;
  if (DisplayController::HasPrimaryDisplay())
    current_primary = DisplayController::GetPrimaryDisplay();

  for (DisplayList::iterator curr_iter = displays_.begin(),
       new_iter = new_displays.begin();
       curr_iter != displays_.end() || new_iter != new_displays.end();) {
    if (curr_iter == displays_.end()) {
      // more displays in new list.
      added_display_indices.push_back(new_iter - new_displays.begin());
      ++new_iter;
    } else if (new_iter == new_displays.end()) {
      // more displays in current list.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
    } else if ((*curr_iter).id() == (*new_iter).id()) {
      const gfx::Display& current_display = *curr_iter;
      gfx::Display& new_display = *new_iter;
      if (force_bounds_changed_ ||
          current_display.bounds_in_pixel() != new_display.bounds_in_pixel() ||
          current_display.device_scale_factor() !=
          new_display.device_scale_factor()) {
        changed_display_indices.push_back(new_iter - new_displays.begin());
      }
      // If the display is primary, then simpy set the origin to (0,0).
      // The secondary display's bounds will be updated by
      // |DisplayController::UpdateDisplayBoundsForLayout|, so no need
      // to change there.
      if ((*new_iter).id() == current_primary.id())
        new_display.set_bounds(gfx::Rect(new_display.bounds().size()));

      new_display.UpdateWorkAreaFromInsets(current_display.GetWorkAreaInsets());
      ++curr_iter;
      ++new_iter;
    } else if ((*curr_iter).id() < (*new_iter).id()) {
      // more displays in current list between ids, which means it is deleted.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
    } else {
      // more displays in new list between ids, which means it is added.
      added_display_indices.push_back(new_iter - new_displays.begin());
      ++new_iter;
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

  RootWindow::CreateParams params(display.bounds_in_pixel());
  params.host = Shell::GetInstance()->root_window_host_factory()->
      CreateRootWindowHost(display.bounds_in_pixel());
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

std::string DisplayManager::GetDisplayNameFor(
    const gfx::Display& display) {
  if (!display.is_valid())
    return l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);

  std::map<int64, DisplayInfo>::const_iterator iter =
      display_info_.find(display.id());
  if (iter != display_info_.end() && !iter->second.name.empty())
    return iter->second.name;

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
      Shell::GetInstance()->screen()->NotifyBoundsChanged(display);
    }
  }
}

void DisplayManager::Init() {
#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS()) {
    std::vector<XID> outputs;
    ui::GetOutputDeviceHandles(&outputs);
    std::vector<std::string> output_names = ui::GetOutputNames(outputs);
    for (size_t i = 0; i < output_names.size(); ++i) {
      if (chromeos::OutputConfigurator::IsInternalOutputName(
              output_names[i])) {
        gfx::Display::SetInternalDisplayId(
            GetDisplayIdForOutput(outputs[i], i));
        break;
      }
    }
  }
#endif

  RefreshDisplayInfo();

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
  std::vector<gfx::Display> new_displays;
  new_displays.push_back(DisplayController::GetPrimaryDisplay());
  // Add if there is only one display.
  if (displays_.size() == 1) {
    // Layout the 2nd display below the primary as with the real device.
    aura::RootWindow* primary = Shell::GetPrimaryRootWindow();
    gfx::Rect host_bounds =
        gfx::Rect(primary->GetHostOrigin(),  primary->GetHostSize());
    new_displays.push_back(CreateDisplayFromSpec(
        StringPrintf("%d+%d-500x400", host_bounds.x(), host_bounds.bottom())));
  }
  OnNativeDisplaysChanged(new_displays);
}

void DisplayManager::ScaleDisplayImpl() {
  DCHECK(!displays_.empty());
  std::vector<gfx::Display> new_displays;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    gfx::Display display = *iter;
    float factor = display.device_scale_factor() == 1.0f ? 2.0f : 1.0f;
    gfx::Point display_origin = display.bounds_in_pixel().origin();
    gfx::Size display_size = gfx::ToFlooredSize(
        gfx::ScaleSize(display.size(), factor));
    display.SetScaleAndBounds(factor, gfx::Rect(display_origin, display_size));
    new_displays.push_back(display);
  }
  OnNativeDisplaysChanged(new_displays);
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
  gfx::Display display = CreateDisplayFromSpec(spec);

  const gfx::Insets insets = display.GetWorkAreaInsets();
  const gfx::Rect& native_bounds = display.bounds_in_pixel();
  display.SetScaleAndBounds(display.device_scale_factor(), native_bounds);
  display.UpdateWorkAreaFromInsets(insets);
  displays_.push_back(display);
}

int64 DisplayManager::SetFirstDisplayAsInternalDisplayForTest() {
  gfx::Display::SetInternalDisplayId(displays_[0].id());
  internal_display_.reset(new gfx::Display);
  *internal_display_ = displays_[0];
  return gfx::Display::InternalDisplayId();
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

DisplayManager::DisplayInfo::DisplayInfo()
  : has_overscan(false),
    has_custom_overscan_insets(false) {
}

void DisplayManager::RefreshDisplayInfo() {
#if defined(OS_CHROMEOS)
  if (!base::chromeos::IsRunningOnChromeOS())
    return;
#endif

#if defined(USE_X11)
  std::vector<XID> outputs;
  if (!ui::GetOutputDeviceHandles(&outputs))
    return;

  for (size_t output_index = 0; output_index < outputs.size(); ++output_index) {
    uint16 manufacturer_id = 0;
    uint16 product_code = 0;
    std::string name;
    ui::GetOutputDeviceData(
        outputs[output_index], &manufacturer_id, &product_code, &name);
    int64 id = gfx::Display::GetID(manufacturer_id, product_code, output_index);
    if (IsInternalDisplayId(id)) {
      display_info_[id].name =
          l10n_util::GetStringUTF8(IDS_ASH_INTERNAL_DISPLAY_NAME);
    } else if (!name.empty()) {
      display_info_[id].name = name;
    }

    ui::GetOutputOverscanFlag(
        outputs[output_index], &display_info_[id].has_overscan);
  }
#endif
}

void DisplayManager::SetDisplayIdsForTest(DisplayList* to_update) const {
  DisplayList::iterator iter_to_update = to_update->begin();
  DisplayList::const_iterator iter = displays_.begin();
  for (; iter != displays_.end() && iter_to_update != to_update->end();
       ++iter, ++iter_to_update) {
    (*iter_to_update).set_id((*iter).id());
  }
}

void DisplayManager::SetHasOverscanFlagForTest(int64 id, bool has_overscan) {
  display_info_[id].has_overscan = has_overscan;
}

gfx::Display CreateDisplayFromSpec(const std::string& spec) {
  static int64 synthesized_display_id = 1000;

#if defined(OS_WIN)
  gfx::Rect bounds(aura::RootWindowHost::GetNativeScreenSize());
#else
  gfx::Rect bounds(kDefaultHostWindowX, kDefaultHostWindowY,
                   kDefaultHostWindowWidth, kDefaultHostWindowHeight);
#endif
  int x = 0, y = 0, width, height;
  float scale = 1.0f;
  if (sscanf(spec.c_str(), "%dx%d*%f", &width, &height, &scale) >= 2 ||
      sscanf(spec.c_str(), "%d+%d-%dx%d*%f", &x, &y, &width, &height,
             &scale) >= 4) {
    bounds.SetRect(x, y, width, height);
  }

  gfx::Display display(synthesized_display_id++);
  display.SetScaleAndBounds(scale, bounds);
  DVLOG(1) << "Display bounds=" << bounds.ToString() << ", scale=" << scale;
  return display;
}

}  // namespace internal
}  // namespace ash
