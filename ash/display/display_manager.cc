// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_manager.h"

#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/display/display_layout_store.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/display/output_configurator_animation.h"
#include "base/chromeos/chromeos_version.h"
#include "chromeos/display/output_configurator.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace ash {
namespace internal {
typedef std::vector<gfx::Display> DisplayList;
typedef std::vector<DisplayInfo> DisplayInfoList;

namespace {

// The number of pixels to overlap between the primary and secondary displays,
// in case that the offset value is too large.
const int kMinimumOverlapForInvalidOffset = 100;

// List of value UI Scale values. Scales for 2x are equivalent to 640,
// 800, 1024, 1280, 1440, 1600 and 1920 pixel width respectively on
// 2560 pixel width 2x density display. Please see crbug.com/233375
// for the full list of resolutions.
const float kUIScalesFor2x[] = {0.5f, 0.625f, 0.8f, 1.0f, 1.125f, 1.25f, 1.5f};
const float kUIScalesFor1280[] = {0.5f, 0.625f, 0.8f, 1.0f, 1.125f };
const float kUIScalesFor1366[] = {0.5f, 0.6f, 0.75f, 1.0f, 1.125f };

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

struct ScaleComparator {
  ScaleComparator(float s) : scale(s) {}

  bool operator()(float s) const {
    const float kEpsilon = 0.0001f;
    return std::abs(scale - s) < kEpsilon;
  }
  float scale;
};

gfx::Display& GetInvalidDisplay() {
  static gfx::Display* invalid_display = new gfx::Display();
  return *invalid_display;
}

// Scoped objects used to either create or close the mirror window
// at specific timing.
class MirrorWindowCreator {
 public:
  MirrorWindowCreator(DisplayManager::Delegate* delegate,
                      const DisplayInfo& display_info)
      : delegate_(delegate),
        display_info_(display_info) {
  }

  virtual ~MirrorWindowCreator() {
    if (delegate_)
      delegate_->CreateOrUpdateMirrorWindow(display_info_);
  }

 private:
  DisplayManager::Delegate* delegate_;
  const DisplayInfo display_info_;
  DISALLOW_COPY_AND_ASSIGN(MirrorWindowCreator);
};

class MirrorWindowCloser {
 public:
  explicit MirrorWindowCloser(DisplayManager::Delegate* delegate)
      : delegate_(delegate)  {}

  virtual ~MirrorWindowCloser() {
    if (delegate_)
      delegate_->CloseMirrorWindow();
  }

 private:
  DisplayManager::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(MirrorWindowCloser);
};

}  // namespace

using std::string;
using std::vector;

DisplayManager::DisplayManager()
    : delegate_(NULL),
      layout_store_(new DisplayLayoutStore),
      first_display_id_(gfx::Display::kInvalidDisplayID),
      num_connected_displays_(0),
      force_bounds_changed_(false),
      change_display_upon_host_resize_(false),
      software_mirroring_enabled_(false) {
#if defined(OS_CHROMEOS)
  change_display_upon_host_resize_ = !base::chromeos::IsRunningOnChromeOS();
#endif
}

DisplayManager::~DisplayManager() {
}

// static
std::vector<float> DisplayManager::GetScalesForDisplay(
    const DisplayInfo& info) {
  std::vector<float> ret;
  if (info.device_scale_factor() == 2.0f) {
    ret.assign(kUIScalesFor2x, kUIScalesFor2x + arraysize(kUIScalesFor2x));
    return ret;
  }
  switch (info.bounds_in_pixel().width()) {
    case 1280:
      ret.assign(kUIScalesFor1280,
                 kUIScalesFor1280 + arraysize(kUIScalesFor1280));
      break;
    case 1366:
      ret.assign(kUIScalesFor1366,
                 kUIScalesFor1366 + arraysize(kUIScalesFor1366));
      break;
    default:
      ret.assign(kUIScalesFor1280,
                 kUIScalesFor1280 + arraysize(kUIScalesFor1280));
#if defined(OS_CHROMEOS)
      if (base::chromeos::IsRunningOnChromeOS())
        NOTREACHED() << "Unknown resolution:" << info.ToString();
#endif
  }
  return ret;
}

// static
float DisplayManager::GetNextUIScale(const DisplayInfo& info, bool up) {
  float scale = info.ui_scale();
  std::vector<float> scales = GetScalesForDisplay(info);
  for (size_t i = 0; i < scales.size(); ++i) {
    if (ScaleComparator(scales[i])(scale)) {
      if (up && i != scales.size() - 1)
        return scales[i + 1];
      if (!up && i != 0)
        return scales[i - 1];
      return scales[i];
    }
  }
  // Fallback to 1.0f if the |scale| wasn't in the list.
  return 1.0f;
}

void DisplayManager::InitFromCommandLine() {
  DisplayInfoList info_list;

  const string size_str = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAshHostWindowBounds);
  vector<string> parts;
  base::SplitString(size_str, ',', &parts);
  for (vector<string>::const_iterator iter = parts.begin();
       iter != parts.end(); ++iter) {
    info_list.push_back(DisplayInfo::CreateFromSpec(*iter));
  }
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshUseFirstDisplayAsInternal))
    gfx::Display::SetInternalDisplayId(info_list[0].id());
  OnNativeDisplaysChanged(info_list);
}

// static
void DisplayManager::UpdateDisplayBoundsForLayoutById(
    const DisplayLayout& layout,
    const gfx::Display& primary_display,
    int64 secondary_display_id) {
  DCHECK_NE(gfx::Display::kInvalidDisplayID, secondary_display_id);
  UpdateDisplayBoundsForLayout(
      layout, primary_display,
      Shell::GetInstance()->display_manager()->
      FindDisplayForId(secondary_display_id));
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

const gfx::Display& DisplayManager::GetDisplayForId(int64 id) const {
  gfx::Display* display =
      const_cast<DisplayManager*>(this)->FindDisplayForId(id);
  return display ? *display : GetInvalidDisplay();
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

bool DisplayManager::UpdateWorkAreaOfDisplay(int64 display_id,
                                             const gfx::Insets& insets) {
  gfx::Display* display = FindDisplayForId(display_id);
  DCHECK(display);
  gfx::Rect old_work_area = display->work_area();
  display->UpdateWorkAreaFromInsets(insets);
  return old_work_area != display->work_area();
}

void DisplayManager::SetOverscanInsets(int64 display_id,
                                       const gfx::Insets& insets_in_dip) {
  display_info_[display_id].SetOverscanInsets(insets_in_dip);
  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    display_info_list.push_back(GetDisplayInfo(iter->id()));
  }
  AddMirrorDisplayInfoIfAny(&display_info_list);
  UpdateDisplays(display_info_list);
}

void DisplayManager::SetDisplayRotation(int64 display_id,
                                        gfx::Display::Rotation rotation) {
  if (!IsDisplayRotationEnabled())
    return;
  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    DisplayInfo info = GetDisplayInfo(iter->id());
    if (info.id() == display_id) {
      if (info.rotation() == rotation)
        return;
      info.set_rotation(rotation);
    }
    display_info_list.push_back(info);
  }
  AddMirrorDisplayInfoIfAny(&display_info_list);
  UpdateDisplays(display_info_list);
}

void DisplayManager::SetDisplayUIScale(int64 display_id,
                                       float ui_scale) {
  if (!IsDisplayUIScalingEnabled() ||
      gfx::Display::InternalDisplayId() != display_id) {
    return;
  }

  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    DisplayInfo info = GetDisplayInfo(iter->id());
    if (info.id() == display_id) {
      if (info.ui_scale() == ui_scale)
        return;
      std::vector<float> scales = GetScalesForDisplay(info);
      ScaleComparator comparator(ui_scale);
      if (std::find_if(scales.begin(), scales.end(), comparator) ==
          scales.end()) {
        return;
      }
      info.set_ui_scale(ui_scale);
    }
    display_info_list.push_back(info);
  }
  AddMirrorDisplayInfoIfAny(&display_info_list);
  UpdateDisplays(display_info_list);
}

void DisplayManager::SetDisplayResolution(int64 display_id,
                                          const gfx::Size& resolution) {
  DCHECK_NE(gfx::Display::InternalDisplayId(), display_id);
  if (gfx::Display::InternalDisplayId() == display_id)
    return;
  resolutions_[display_id] = resolution;
#if defined(OS_CHROMEOS) && defined(USE_X11)
  if (base::chromeos::IsRunningOnChromeOS())
    Shell::GetInstance()->output_configurator()->ScheduleConfigureOutputs();
#endif
}

void DisplayManager::RegisterDisplayProperty(
    int64 display_id,
    gfx::Display::Rotation rotation,
    float ui_scale,
    const gfx::Insets* overscan_insets,
    const gfx::Size& resolution_in_pixels) {
  if (display_info_.find(display_id) == display_info_.end()) {
    display_info_[display_id] =
        DisplayInfo(display_id, std::string(""), false);
  }

  display_info_[display_id].set_rotation(rotation);
  // Just in case the preference file was corrupted.
  if (0.5f <= ui_scale && ui_scale <= 2.0f)
    display_info_[display_id].set_ui_scale(ui_scale);
  if (overscan_insets)
    display_info_[display_id].SetOverscanInsets(*overscan_insets);
  if (!resolution_in_pixels.IsEmpty())
    resolutions_[display_id] = resolution_in_pixels;
}

bool DisplayManager::GetSelectedResolutionForDisplayId(
    int64 id,
    gfx::Size* resolution_out) const {
  std::map<int64, gfx::Size>::const_iterator iter =
      resolutions_.find(id);
  if (iter == resolutions_.end())
    return false;
  *resolution_out = iter->second;
  return true;
}

bool DisplayManager::IsDisplayRotationEnabled() const {
  static bool enabled = !CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kAshDisableDisplayRotation);
  return enabled;
}

bool DisplayManager::IsDisplayUIScalingEnabled() const {
  static bool enabled = !CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kAshDisableUIScaling);
  if (!enabled)
    return false;
  return GetDisplayIdForUIScaling() != gfx::Display::kInvalidDisplayID;
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
    // If the device is booted without display, or chrome is started
    // without --ash-host-window-bounds on linux desktop, use the
    // default display.
    if (displays_.empty()) {
      std::vector<DisplayInfo> init_displays;
      init_displays.push_back(DisplayInfo::CreateFromSpec(std::string()));
      OnNativeDisplaysChanged(init_displays);
    } else {
      // Otherwise don't update the displays when all displays are disconnected.
      // This happens when:
      // - the device is idle and powerd requested to turn off all displays.
      // - the device is suspended. (kernel turns off all displays)
      // - the internal display's brightness is set to 0 and no external
      //   display is connected.
      // - the internal display's brightness is 0 and external display is
      //   disconnected.
      // The display will be updated when one of displays is turned on, and the
      // display list will be updated correctly.
    }
    return;
  }
  first_display_id_ = updated_displays[0].id();
  std::set<gfx::Point> origins;

  if (updated_displays.size() == 1) {
    VLOG(1) << "OnNativeDisplaysChanged(1):" << updated_displays[0].ToString();
  } else {
    VLOG(1) << "OnNativeDisplaysChanged(" << updated_displays.size()
            << ") [0]=" << updated_displays[0].ToString()
            << ", [1]=" << updated_displays[1].ToString();
  }

  bool internal_display_connected = false;
  num_connected_displays_ = updated_displays.size();
  mirrored_display_ = gfx::Display();
  DisplayInfoList new_display_info_list;
  for (DisplayInfoList::const_iterator iter = updated_displays.begin();
       iter != updated_displays.end();
       ++iter) {
    if (!internal_display_connected)
      internal_display_connected = IsInternalDisplayId(iter->id());
    // Mirrored monitors have the same origins.
    gfx::Point origin = iter->bounds_in_pixel().origin();
    if (origins.find(origin) != origins.end()) {
      InsertAndUpdateDisplayInfo(*iter);
      mirrored_display_ = CreateDisplayFromDisplayInfoById(iter->id());
    } else {
      origins.insert(origin);
      new_display_info_list.push_back(*iter);
    }
  }
  if (HasInternalDisplay() &&
      !internal_display_connected &&
      display_info_.find(gfx::Display::InternalDisplayId()) ==
      display_info_.end()) {
    DisplayInfo internal_display_info(
        gfx::Display::InternalDisplayId(),
        l10n_util::GetStringUTF8(IDS_ASH_INTERNAL_DISPLAY_NAME),
        false  /*Internal display must not have overscan */);
    internal_display_info.SetBounds(gfx::Rect(0, 0, 800, 600));
    display_info_[gfx::Display::InternalDisplayId()] = internal_display_info;
  }
  UpdateDisplays(new_display_info_list);
}

void DisplayManager::UpdateDisplays() {
  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    display_info_list.push_back(GetDisplayInfo(iter->id()));
  }
  AddMirrorDisplayInfoIfAny(&display_info_list);
  UpdateDisplays(display_info_list);
}

void DisplayManager::UpdateDisplays(
    const std::vector<DisplayInfo>& updated_display_info_list) {
#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    DCHECK_EQ(1u, updated_display_info_list.size()) <<
        "Multiple display test does not work on Win8 bots. Please "
        "skip (don't disable) the test using SupportsMultipleDisplays()";
  }
#endif

  DisplayInfoList new_display_info_list = updated_display_info_list;
  std::sort(displays_.begin(), displays_.end(), DisplaySortFunctor());
  std::sort(new_display_info_list.begin(),
            new_display_info_list.end(),
            DisplayInfoSortFunctor());
  DisplayList removed_displays;
  std::vector<size_t> changed_display_indices;
  std::vector<size_t> added_display_indices;

  DisplayList::iterator curr_iter = displays_.begin();
  DisplayInfoList::const_iterator new_info_iter = new_display_info_list.begin();

  DisplayList new_displays;

  scoped_ptr<MirrorWindowCreator> mirror_window_creater;

  // Use the internal display or 1st as the mirror source, then scale
  // the root window so that it matches the external display's
  // resolution. This is necessary in order for scaling to work while
  // mirrored.
  int64 mirrored_display_id = gfx::Display::kInvalidDisplayID;
  if (software_mirroring_enabled_ && new_display_info_list.size() == 2)
    mirrored_display_id = new_display_info_list[1].id();

  while (curr_iter != displays_.end() ||
         new_info_iter != new_display_info_list.end()) {
    if (new_info_iter != new_display_info_list.end() &&
        mirrored_display_id == new_info_iter->id()) {
      DisplayInfo info = *new_info_iter;
      info.SetOverscanInsets(gfx::Insets());
      InsertAndUpdateDisplayInfo(info);

      mirrored_display_ = CreateDisplayFromDisplayInfoById(new_info_iter->id());
      mirror_window_creater.reset(new MirrorWindowCreator(
          delegate_, display_info_[new_info_iter->id()]));
      ++new_info_iter;
      // Remove existing external dispaly if it is going to be mirrored.
      if (curr_iter != displays_.end() &&
          curr_iter->id() == mirrored_display_id) {
        removed_displays.push_back(*curr_iter);
        ++curr_iter;
      }
      continue;
    }

    if (curr_iter == displays_.end()) {
      // more displays in new list.
      added_display_indices.push_back(new_displays.size());
      InsertAndUpdateDisplayInfo(*new_info_iter);
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
      const DisplayInfo current_display_info =
          GetDisplayInfo(current_display.id());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      gfx::Display new_display =
          CreateDisplayFromDisplayInfoById(new_info_iter->id());
      const DisplayInfo& new_display_info = GetDisplayInfo(new_display.id());

      bool host_window_bounds_changed =
          current_display_info.bounds_in_pixel() !=
          new_display_info.bounds_in_pixel();

      if (force_bounds_changed_ ||
          host_window_bounds_changed ||
          (current_display.device_scale_factor() !=
           new_display.device_scale_factor()) ||
          (current_display_info.size_in_pixel() !=
           new_display.GetSizeInPixel()) ||
          (current_display.rotation() != new_display.rotation())) {

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
      InsertAndUpdateDisplayInfo(*new_info_iter);
      new_displays.push_back(
          CreateDisplayFromDisplayInfoById(new_info_iter->id()));
      ++new_info_iter;
    }
  }

  scoped_ptr<MirrorWindowCloser> mirror_window_closer;
  // Try to close mirror window unless mirror window is necessary.
  if (!mirror_window_creater.get())
    mirror_window_closer.reset(new MirrorWindowCloser(delegate_));

  // Do not update |displays_| if there's nothing to be updated. Without this,
  // it will not update the display layout, which causes the bug
  // http://crbug.com/155948.
  if (changed_display_indices.empty() && added_display_indices.empty() &&
      removed_displays.empty()) {
    return;
  }
  if (delegate_)
    delegate_->PreDisplayConfigurationChange();

  size_t updated_index;
  if (UpdateSecondaryDisplayBoundsForLayout(&new_displays, &updated_index) &&
      std::find(added_display_indices.begin(),
                added_display_indices.end(),
                updated_index) == added_display_indices.end() &&
      std::find(changed_display_indices.begin(),
                changed_display_indices.end(),
                updated_index) == changed_display_indices.end()) {
    changed_display_indices.push_back(updated_index);
  }

  displays_ = new_displays;

  base::AutoReset<bool> resetter(&change_display_upon_host_resize_, false);

  // Temporarily add displays to be removed because display object
  // being removed are accessed during shutting down the root.
  displays_.insert(displays_.end(), removed_displays.begin(),
                   removed_displays.end());

  for (DisplayList::const_reverse_iterator iter = removed_displays.rbegin();
       iter != removed_displays.rend(); ++iter) {
    Shell::GetInstance()->screen()->NotifyDisplayRemoved(displays_.back());
    displays_.pop_back();
  }
  // Close the mirror window here to avoid creating two compositor on
  // one display.
  mirror_window_closer.reset();
  for (std::vector<size_t>::iterator iter = added_display_indices.begin();
       iter != added_display_indices.end(); ++iter) {
    Shell::GetInstance()->screen()->NotifyDisplayAdded(displays_[*iter]);
  }
  // Create the mirror window after all displays are added so that
  // it can mirror the display newly added. This can happen when switching
  // from dock mode to software mirror mode.
  mirror_window_creater.reset();
  for (std::vector<size_t>::iterator iter = changed_display_indices.begin();
       iter != changed_display_indices.end(); ++iter) {
    Shell::GetInstance()->screen()->NotifyBoundsChanged(displays_[*iter]);
  }
  if (delegate_)
    delegate_->PostDisplayConfigurationChange();

#if defined(USE_X11) && defined(OS_CHROMEOS)
  if (!changed_display_indices.empty() && base::chromeos::IsRunningOnChromeOS())
    ui::ClearX11DefaultRootWindow();
#endif
}

const gfx::Display& DisplayManager::GetDisplayAt(size_t index) const {
  DCHECK_LT(index, displays_.size());
  return displays_[index];
}

const gfx::Display* DisplayManager::GetPrimaryDisplayCandidate() const {
  const gfx::Display* primary_candidate = &displays_[0];
#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS()) {
    // On ChromeOS device, root windows are stacked vertically, and
    // default primary is the one on top.
    int count = GetNumDisplays();
    int y = GetDisplayInfo(primary_candidate->id()).bounds_in_pixel().y();
    for (int i = 1; i < count; ++i) {
      const gfx::Display* display = &displays_[i];
      const DisplayInfo& display_info = GetDisplayInfo(display->id());
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

bool DisplayManager::IsMirrored() const {
  return mirrored_display_.id() != gfx::Display::kInvalidDisplayID;
}

const DisplayInfo& DisplayManager::GetDisplayInfo(int64 display_id) const {
  std::map<int64, DisplayInfo>::const_iterator iter =
      display_info_.find(display_id);
  CHECK(iter != display_info_.end()) << display_id;
  return iter->second;
}

std::string DisplayManager::GetDisplayNameForId(int64 id) {
  if (id == gfx::Display::kInvalidDisplayID)
    return l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);

  std::map<int64, DisplayInfo>::const_iterator iter = display_info_.find(id);
  if (iter != display_info_.end() && !iter->second.name().empty())
    return iter->second.name();

  return base::StringPrintf("Display %d", static_cast<int>(id));
}

int64 DisplayManager::GetDisplayIdForUIScaling() const {
  // UI Scaling is effective only on internal display.
  int64 display_id = gfx::Display::InternalDisplayId();
#if defined(OS_WIN)
  display_id = first_display_id();
#endif
  return display_id;
}

void DisplayManager::SetMirrorMode(bool mirrored) {
  if (num_connected_displays() <= 1)
    return;

#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS()) {
    chromeos::OutputState new_state = mirrored ?
        chromeos::STATE_DUAL_MIRROR : chromeos::STATE_DUAL_EXTENDED;
    Shell::GetInstance()->output_configurator()->SetDisplayMode(new_state);
    return;
  }
#endif
  SetSoftwareMirroring(mirrored);
  DisplayInfoList display_info_list;
  int count = 0;
  for (std::map<int64, DisplayInfo>::const_iterator iter =
           display_info_.begin();
       count < 2; ++iter, ++count) {
    display_info_list.push_back(GetDisplayInfo(iter->second.id()));
  }
  UpdateDisplays(display_info_list);
#if defined(OS_CHROMEOS)
  if (Shell::GetInstance()->output_configurator_animation()) {
    Shell::GetInstance()->output_configurator_animation()->
        StartFadeInAnimation();
  }
#endif
}

void DisplayManager::AddRemoveDisplay() {
  DCHECK(!displays_.empty());
  std::vector<DisplayInfo> new_display_info_list;
  DisplayInfo first_display = GetDisplayInfo(displays_[0].id());
  new_display_info_list.push_back(first_display);
  // Add if there is only one display connected.
  if (num_connected_displays() == 1) {
    // Layout the 2nd display below the primary as with the real device.
    gfx::Rect host_bounds = first_display.bounds_in_pixel();
    new_display_info_list.push_back(DisplayInfo::CreateFromSpec(
        base::StringPrintf(
            "%d+%d-500x400", host_bounds.x(), host_bounds.bottom())));
  }
  num_connected_displays_ = new_display_info_list.size();
  mirrored_display_ = gfx::Display();
  UpdateDisplays(new_display_info_list);
}

void DisplayManager::ToggleDisplayScaleFactor() {
  DCHECK(!displays_.empty());
  std::vector<DisplayInfo> new_display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    DisplayInfo display_info = GetDisplayInfo(iter->id());
    display_info.set_device_scale_factor(
        display_info.device_scale_factor() == 1.0f ? 2.0f : 1.0f);
    new_display_info_list.push_back(display_info);
  }
  AddMirrorDisplayInfoIfAny(&new_display_info_list);
  UpdateDisplays(new_display_info_list);
}

void DisplayManager::SetSoftwareMirroring(bool enabled) {
  software_mirroring_enabled_ = enabled;
  mirrored_display_ = gfx::Display();
}

bool DisplayManager::UpdateDisplayBounds(int64 display_id,
                                         const gfx::Rect& new_bounds) {
  if (change_display_upon_host_resize_) {
    display_info_[display_id].SetBounds(new_bounds);
    // Don't notify observers if the mirrored window has changed.
    if (software_mirroring_enabled_ && mirrored_display_.id() == display_id)
      return false;
    gfx::Display* display = FindDisplayForId(display_id);
    display->SetSize(display_info_[display_id].size_in_pixel());
    Shell::GetInstance()->screen()->NotifyBoundsChanged(*display);
    return true;
  }
  return false;
}

gfx::Display* DisplayManager::FindDisplayForId(int64 id) {
  for (DisplayList::iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    if ((*iter).id() == id)
      return &(*iter);
  }
  DLOG(WARNING) << "Could not find display:" << id;
  return NULL;
}

void DisplayManager::AddMirrorDisplayInfoIfAny(
    std::vector<DisplayInfo>* display_info_list) {
  if (software_mirroring_enabled_ && mirrored_display_.is_valid())
    display_info_list->push_back(GetDisplayInfo(mirrored_display_.id()));
}

void DisplayManager::InsertAndUpdateDisplayInfo(const DisplayInfo& new_info) {
  std::map<int64, DisplayInfo>::iterator info =
      display_info_.find(new_info.id());
  if (info != display_info_.end())
    info->second.Copy(new_info);
  else {
    display_info_[new_info.id()] = new_info;
    display_info_[new_info.id()].set_native(false);
  }
  display_info_[new_info.id()].UpdateDisplaySize();
}

gfx::Display DisplayManager::CreateDisplayFromDisplayInfoById(int64 id) {
  DCHECK(display_info_.find(id) != display_info_.end());
  const DisplayInfo& display_info = display_info_[id];

  gfx::Display new_display(display_info.id());
  gfx::Rect bounds_in_pixel(display_info.size_in_pixel());

  // Simply set the origin to (0,0).  The primary display's origin is
  // always (0,0) and the secondary display's bounds will be updated
  // in |UpdateSecondaryDisplayBoundsForLayout| called in |UpdateDisplay|.
  new_display.SetScaleAndBounds(
      display_info.device_scale_factor(), gfx::Rect(bounds_in_pixel.size()));
  new_display.set_rotation(display_info.rotation());
  return new_display;
}

bool DisplayManager::UpdateSecondaryDisplayBoundsForLayout(
    DisplayList* displays,
    size_t* updated_index) const {
  if (displays->size() != 2U)
    return false;

  int64 id_at_zero = displays->at(0).id();
  DisplayIdPair pair =
      (id_at_zero == first_display_id_ ||
       id_at_zero == gfx::Display::InternalDisplayId()) ?
      std::make_pair(id_at_zero, displays->at(1).id()) :
      std::make_pair(displays->at(1).id(), id_at_zero) ;
  DisplayLayout layout =
      layout_store_->ComputeDisplayLayoutForDisplayIdPair(pair);

  // Ignore if a user has a old format (should be extremely rare)
  // and this will be replaced with DCHECK.
  if (layout.primary_id != gfx::Display::kInvalidDisplayID) {
    size_t primary_index, secondary_index;
    if (displays->at(0).id() == layout.primary_id) {
      primary_index = 0;
      secondary_index = 1;
    } else {
      primary_index = 1;
      secondary_index = 0;
    }
    gfx::Rect bounds =
        GetDisplayForId(displays->at(secondary_index).id()).bounds();
    UpdateDisplayBoundsForLayout(
        layout, displays->at(primary_index), &displays->at(secondary_index));
    *updated_index = secondary_index;
    return bounds != displays->at(secondary_index).bounds();
  }
  return false;
}

// static
void DisplayManager::UpdateDisplayBoundsForLayout(
    const DisplayLayout& layout,
    const gfx::Display& primary_display,
    gfx::Display* secondary_display) {
  DCHECK_EQ("0,0", primary_display.bounds().origin().ToString());

  const gfx::Rect& primary_bounds = primary_display.bounds();
  const gfx::Rect& secondary_bounds = secondary_display->bounds();
  gfx::Point new_secondary_origin = primary_bounds.origin();

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

}  // namespace internal
}  // namespace ash
