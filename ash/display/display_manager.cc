// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_manager.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/display/display_layout_store.h"
#include "ash/display/display_util.h"
#include "ash/display/extended_mouse_warp_controller.h"
#include "ash/display/null_mouse_warp_controller.h"
#include "ash/display/screen_ash.h"
#include "ash/display/unified_mouse_warp_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/screen.h"

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace ash {
typedef std::vector<DisplayInfo> DisplayInfoList;

namespace {

// We need to keep this in order for unittests to tell if
// the object in gfx::Screen::GetScreenByType is for shutdown.
gfx::Screen* screen_for_shutdown = nullptr;

// The number of pixels to overlap between the primary and secondary displays,
// in case that the offset value is too large.
const int kMinimumOverlapForInvalidOffset = 100;

struct DisplaySortFunctor {
  bool operator()(const gfx::Display& a, const gfx::Display& b) {
    return CompareDisplayIds(a.id(), b.id());
  }
};

struct DisplayInfoSortFunctor {
  bool operator()(const DisplayInfo& a, const DisplayInfo& b) {
    return CompareDisplayIds(a.id(), b.id());
  }
};

gfx::Display& GetInvalidDisplay() {
  static gfx::Display* invalid_display = new gfx::Display();
  return *invalid_display;
}

std::vector<DisplayMode>::const_iterator FindDisplayMode(
    const DisplayInfo& info,
    const DisplayMode& target_mode) {
  const std::vector<DisplayMode>& modes = info.display_modes();
  return std::find_if(modes.begin(), modes.end(),
                      [target_mode](const DisplayMode& mode) {
                        return target_mode.IsEquivalent(mode);
                      });
}

void SetInternalDisplayModeList(DisplayInfo* info) {
  DisplayMode native_mode;
  native_mode.size = info->bounds_in_native().size();
  native_mode.device_scale_factor = info->device_scale_factor();
  native_mode.ui_scale = 1.0f;
  info->SetDisplayModes(CreateInternalDisplayModeList(native_mode));
}

void MaybeInitInternalDisplay(DisplayInfo* info) {
  int64_t id = info->id();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshUseFirstDisplayAsInternal)) {
    gfx::Display::SetInternalDisplayId(id);
    SetInternalDisplayModeList(info);
  }
}

gfx::Size GetMaxNativeSize(const DisplayInfo& info) {
  gfx::Size size;
  for (auto& mode : info.display_modes()) {
    if (mode.size.GetArea() > size.GetArea())
      size = mode.size;
  }
  return size;
}

}  // namespace

using std::string;
using std::vector;

// static
int64_t DisplayManager::kUnifiedDisplayId = -10;

DisplayManager::DisplayManager()
    : delegate_(nullptr),
      screen_(new ScreenAsh),
      layout_store_(new DisplayLayoutStore),
      first_display_id_(gfx::Display::kInvalidDisplayID),
      num_connected_displays_(0),
      force_bounds_changed_(false),
      change_display_upon_host_resize_(false),
      multi_display_mode_(EXTENDED),
      current_default_multi_display_mode_(EXTENDED),
      mirroring_display_id_(gfx::Display::kInvalidDisplayID),
      registered_internal_display_rotation_lock_(false),
      registered_internal_display_rotation_(gfx::Display::ROTATE_0),
      unified_desktop_enabled_(false),
      weak_ptr_factory_(this) {
#if defined(OS_CHROMEOS)
  change_display_upon_host_resize_ = !base::SysInfo::IsRunningOnChromeOS();
  unified_desktop_enabled_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnableUnifiedDesktop);
#endif
  gfx::Screen* current = gfx::Screen::GetScreen();
  // If there is no native, or the native was for shutdown,
  // use ash's screen.
  if (!current || current == screen_for_shutdown)
    gfx::Screen::SetScreenInstance(screen_.get());
}

DisplayManager::~DisplayManager() {
#if defined(OS_CHROMEOS)
  // Reset the font params.
  gfx::SetFontRenderParamsDeviceScaleFactor(1.0f);
#endif
}

bool DisplayManager::InitFromCommandLine() {
  DisplayInfoList info_list;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kAshHostWindowBounds))
    return false;
  const string size_str =
      command_line->GetSwitchValueASCII(switches::kAshHostWindowBounds);
  for (const std::string& part : base::SplitString(
           size_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    info_list.push_back(DisplayInfo::CreateFromSpec(part));
    info_list.back().set_native(true);
  }
  MaybeInitInternalDisplay(&info_list[0]);
  if (info_list.size() > 1 &&
      command_line->HasSwitch(switches::kAshEnableSoftwareMirroring)) {
    SetMultiDisplayMode(MIRRORING);
  }
  OnNativeDisplaysChanged(info_list);
  return true;
}

void DisplayManager::InitDefaultDisplay() {
  DisplayInfoList info_list;
  info_list.push_back(DisplayInfo::CreateFromSpec(std::string()));
  info_list.back().set_native(true);
  MaybeInitInternalDisplay(&info_list[0]);
  OnNativeDisplaysChanged(info_list);
}

void DisplayManager::RefreshFontParams() {
#if defined(OS_CHROMEOS)
  // Use the largest device scale factor among currently active displays. Non
  // internal display may have bigger scale factor in case the external display
  // is an 4K display.
  float largest_device_scale_factor = 1.0f;
  for (const gfx::Display& display : active_display_list_) {
    const ash::DisplayInfo& info = display_info_[display.id()];
    largest_device_scale_factor = std::max(
        largest_device_scale_factor, info.GetEffectiveDeviceScaleFactor());
  }
  gfx::SetFontRenderParamsDeviceScaleFactor(largest_device_scale_factor);
#endif  // OS_CHROMEOS
}

const display::DisplayLayout& DisplayManager::GetCurrentDisplayLayout() const {
  DCHECK_LE(2U, num_connected_displays());
  if (num_connected_displays() > 1) {
    display::DisplayIdList list = GetCurrentDisplayIdList();
    return layout_store_->GetRegisteredDisplayLayout(list);
  }
  LOG(ERROR) << "DisplayLayout is requested for single display";
  // On release build, just fallback to default instead of blowing up.
  static display::DisplayLayout layout;
  layout.primary_id = active_display_list_[0].id();
  return layout;
}

display::DisplayIdList DisplayManager::GetCurrentDisplayIdList() const {
  if (IsInUnifiedMode()) {
    return CreateDisplayIdList(software_mirroring_display_list_);
  } else if (IsInMirrorMode()) {
    if (software_mirroring_enabled()) {
      CHECK_EQ(2u, num_connected_displays());
      // This comment is to make it easy to distinguish the crash
      // between two checks.
      CHECK_EQ(1u, active_display_list_.size());
    }
    int64_t ids[] = {active_display_list_[0].id(), mirroring_display_id_};
    return ash::GenerateDisplayIdList(std::begin(ids), std::end(ids));
  } else {
    CHECK_LE(2u, active_display_list_.size());
    return CreateDisplayIdList(active_display_list_);
  }
}

void DisplayManager::SetLayoutForCurrentDisplays(
    std::unique_ptr<display::DisplayLayout> layout) {
  if (GetNumDisplays() == 1)
    return;
  const display::DisplayIdList list = GetCurrentDisplayIdList();

  DCHECK(display::DisplayLayout::Validate(list, *layout));

  const display::DisplayLayout& current_layout =
      layout_store_->GetRegisteredDisplayLayout(list);

  if (layout->HasSamePlacementList(current_layout))
    return;

  layout_store_->RegisterLayoutForDisplayIdList(list, std::move(layout));
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(false);

  // TODO(oshima): Call UpdateDisplays instead.
  std::vector<int64_t> updated_ids;
  ApplyDisplayLayout(GetCurrentDisplayLayout(), &active_display_list_,
                     &updated_ids);
  for (int64_t id : updated_ids) {
    screen_->NotifyMetricsChanged(
        GetDisplayForId(id),
        gfx::DisplayObserver::DISPLAY_METRIC_BOUNDS |
            gfx::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  }

  if (delegate_)
    delegate_->PostDisplayConfigurationChange();
}

const gfx::Display& DisplayManager::GetDisplayForId(int64_t id) const {
  gfx::Display* display =
      const_cast<DisplayManager*>(this)->FindDisplayForId(id);
  return display ? *display : GetInvalidDisplay();
}

const gfx::Display& DisplayManager::FindDisplayContainingPoint(
    const gfx::Point& point_in_screen) const {
  int index =
      FindDisplayIndexContainingPoint(active_display_list_, point_in_screen);
  return index < 0 ? GetInvalidDisplay() : active_display_list_[index];
}

bool DisplayManager::UpdateWorkAreaOfDisplay(int64_t display_id,
                                             const gfx::Insets& insets) {
  gfx::Display* display = FindDisplayForId(display_id);
  DCHECK(display);
  gfx::Rect old_work_area = display->work_area();
  display->UpdateWorkAreaFromInsets(insets);
  return old_work_area != display->work_area();
}

void DisplayManager::SetOverscanInsets(int64_t display_id,
                                       const gfx::Insets& insets_in_dip) {
  bool update = false;
  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_) {
    DisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      if (insets_in_dip.IsEmpty()) {
        info.set_clear_overscan_insets(true);
      } else {
        info.set_clear_overscan_insets(false);
        info.SetOverscanInsets(insets_in_dip);
      }
      update = true;
    }
    display_info_list.push_back(info);
  }
  if (update) {
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplaysWith(display_info_list);
  } else {
    display_info_[display_id].SetOverscanInsets(insets_in_dip);
  }
}

void DisplayManager::SetDisplayRotation(int64_t display_id,
                                        gfx::Display::Rotation rotation,
                                        gfx::Display::RotationSource source) {
  if (IsInUnifiedMode())
    return;

  DisplayInfoList display_info_list;
  bool is_active = false;
  for (const auto& display : active_display_list_) {
    DisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      if (info.GetRotation(source) == rotation &&
          info.GetActiveRotation() == rotation) {
        return;
      }
      info.SetRotation(rotation, source);
      is_active = true;
    }
    display_info_list.push_back(info);
  }
  if (is_active) {
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplaysWith(display_info_list);
  } else if (display_info_.find(display_id) != display_info_.end()) {
    // Inactive displays can reactivate, ensure they have been updated.
    display_info_[display_id].SetRotation(rotation, source);
  }
}

bool DisplayManager::SetDisplayMode(int64_t display_id,
                                    const DisplayMode& display_mode) {
  bool change_ui_scale = GetDisplayIdForUIScaling() == display_id;

  DisplayInfoList display_info_list;
  bool display_property_changed = false;
  bool resolution_changed = false;
  for (const auto& display : active_display_list_) {
    DisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      auto iter = FindDisplayMode(info, display_mode);
      if (iter == info.display_modes().end()) {
        LOG(WARNING) << "Unsupported display mode was requested:"
                     << "size=" << display_mode.size.ToString()
                     << ", ui scale=" << display_mode.ui_scale
                     << ", scale fator=" << display_mode.device_scale_factor;
        return false;
      }

      if (change_ui_scale) {
        if (info.configured_ui_scale() == display_mode.ui_scale)
          return true;
        info.set_configured_ui_scale(display_mode.ui_scale);
        display_property_changed = true;
      } else {
        display_modes_[display_id] = *iter;
        if (info.bounds_in_native().size() != display_mode.size)
          resolution_changed = true;
        if (info.device_scale_factor() != display_mode.device_scale_factor) {
          info.set_device_scale_factor(display_mode.device_scale_factor);
          display_property_changed = true;
        }
      }
    }
    display_info_list.push_back(info);
  }
  if (display_property_changed) {
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplaysWith(display_info_list);
  }
  if (resolution_changed && IsInUnifiedMode()) {
    ReconfigureDisplays();
#if defined(OS_CHROMEOS)
  } else if (resolution_changed && base::SysInfo::IsRunningOnChromeOS()) {
    Shell::GetInstance()->display_configurator()->OnConfigurationChanged();
#endif
  }
  return resolution_changed || display_property_changed;
}

void DisplayManager::RegisterDisplayProperty(
    int64_t display_id,
    gfx::Display::Rotation rotation,
    float ui_scale,
    const gfx::Insets* overscan_insets,
    const gfx::Size& resolution_in_pixels,
    float device_scale_factor,
    ui::ColorCalibrationProfile color_profile) {
  if (display_info_.find(display_id) == display_info_.end())
    display_info_[display_id] = DisplayInfo(display_id, std::string(), false);

  // Do not allow rotation in unified desktop mode.
  if (display_id == kUnifiedDisplayId)
    rotation = gfx::Display::ROTATE_0;

  display_info_[display_id].SetRotation(rotation,
                                        gfx::Display::ROTATION_SOURCE_USER);
  display_info_[display_id].SetRotation(rotation,
                                        gfx::Display::ROTATION_SOURCE_ACTIVE);
  display_info_[display_id].SetColorProfile(color_profile);
  // Just in case the preference file was corrupted.
  // TODO(mukai): register |display_modes_| here as well, so the lookup for the
  // default mode in GetActiveModeForDisplayId() gets much simpler.
  if (0.5f <= ui_scale && ui_scale <= 2.0f)
    display_info_[display_id].set_configured_ui_scale(ui_scale);
  if (overscan_insets)
    display_info_[display_id].SetOverscanInsets(*overscan_insets);
  if (!resolution_in_pixels.IsEmpty()) {
    DCHECK(!gfx::Display::IsInternalDisplayId(display_id));
    // Default refresh rate, until OnNativeDisplaysChanged() updates us with the
    // actual display info, is 60 Hz.
    DisplayMode mode(resolution_in_pixels, 60.0f, false, false);
    mode.device_scale_factor = device_scale_factor;
    display_modes_[display_id] = mode;
  }
}

DisplayMode DisplayManager::GetActiveModeForDisplayId(
    int64_t display_id) const {
  DisplayMode selected_mode;
  if (GetSelectedModeForDisplayId(display_id, &selected_mode))
    return selected_mode;

  // If 'selected' mode is empty, it should return the default mode. This means
  // the native mode for the external display. Unfortunately this is not true
  // for the internal display because restoring UI-scale doesn't register the
  // restored mode to |display_mode_|, so it needs to look up the mode whose
  // UI-scale value matches. See the TODO in RegisterDisplayProperty().
  const DisplayInfo& info = GetDisplayInfo(display_id);

  for (auto& mode : info.display_modes()) {
    if (GetDisplayIdForUIScaling() == display_id) {
      if (info.configured_ui_scale() == mode.ui_scale)
        return mode;
    } else if (mode.native) {
      return mode;
    }
  }
  return selected_mode;
}

void DisplayManager::RegisterDisplayRotationProperties(bool rotation_lock,
    gfx::Display::Rotation rotation) {
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(false);
  registered_internal_display_rotation_lock_ = rotation_lock;
  registered_internal_display_rotation_ = rotation;
  if (delegate_)
    delegate_->PostDisplayConfigurationChange();
}

bool DisplayManager::GetSelectedModeForDisplayId(int64_t id,
                                                 DisplayMode* mode_out) const {
  std::map<int64_t, DisplayMode>::const_iterator iter = display_modes_.find(id);
  if (iter == display_modes_.end())
    return false;
  *mode_out = iter->second;
  return true;
}

bool DisplayManager::IsDisplayUIScalingEnabled() const {
  return GetDisplayIdForUIScaling() != gfx::Display::kInvalidDisplayID;
}

gfx::Insets DisplayManager::GetOverscanInsets(int64_t display_id) const {
  std::map<int64_t, DisplayInfo>::const_iterator it =
      display_info_.find(display_id);
  return (it != display_info_.end()) ?
      it->second.overscan_insets_in_dip() : gfx::Insets();
}

void DisplayManager::SetColorCalibrationProfile(
    int64_t display_id,
    ui::ColorCalibrationProfile profile) {
#if defined(OS_CHROMEOS)
  if (!display_info_[display_id].IsColorProfileAvailable(profile))
    return;

  if (delegate_)
    delegate_->PreDisplayConfigurationChange(false);
  // Just sets color profile if it's not running on ChromeOS (like tests).
  if (!base::SysInfo::IsRunningOnChromeOS() ||
      Shell::GetInstance()->display_configurator()->SetColorCalibrationProfile(
          display_id, profile)) {
    display_info_[display_id].SetColorProfile(profile);
    UMA_HISTOGRAM_ENUMERATION(
        "ChromeOS.Display.ColorProfile", profile, ui::NUM_COLOR_PROFILES);
  }
  if (delegate_)
    delegate_->PostDisplayConfigurationChange();
#endif
}

void DisplayManager::OnNativeDisplaysChanged(
    const std::vector<DisplayInfo>& updated_displays) {
  if (updated_displays.empty()) {
    VLOG(1) << "OnNativeDisplaysChanged(0): # of current displays="
            << active_display_list_.size();
    // If the device is booted without display, or chrome is started
    // without --ash-host-window-bounds on linux desktop, use the
    // default display.
    if (active_display_list_.empty()) {
      std::vector<DisplayInfo> init_displays;
      init_displays.push_back(DisplayInfo::CreateFromSpec(std::string()));
      MaybeInitInternalDisplay(&init_displays[0]);
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
  mirroring_display_id_ = gfx::Display::kInvalidDisplayID;
  software_mirroring_display_list_.clear();
  DisplayInfoList new_display_info_list;
  for (DisplayInfoList::const_iterator iter = updated_displays.begin();
       iter != updated_displays.end();
       ++iter) {
    if (!internal_display_connected)
      internal_display_connected =
          gfx::Display::IsInternalDisplayId(iter->id());
    // Mirrored monitors have the same origins.
    gfx::Point origin = iter->bounds_in_native().origin();
    if (origins.find(origin) != origins.end()) {
      InsertAndUpdateDisplayInfo(*iter);
      mirroring_display_id_ = iter->id();
    } else {
      origins.insert(origin);
      new_display_info_list.push_back(*iter);
    }

    DisplayMode new_mode;
    new_mode.size = iter->bounds_in_native().size();
    new_mode.device_scale_factor = iter->device_scale_factor();
    new_mode.ui_scale = iter->configured_ui_scale();
    const std::vector<DisplayMode>& display_modes = iter->display_modes();
    // This is empty the displays are initialized from InitFromCommandLine.
    if (!display_modes.size())
      continue;
    auto display_modes_iter = FindDisplayMode(*iter, new_mode);
    // Update the actual resolution selected as the resolution request may fail.
    if (display_modes_iter == display_modes.end())
      display_modes_.erase(iter->id());
    else if (display_modes_.find(iter->id()) != display_modes_.end())
      display_modes_[iter->id()] = *display_modes_iter;
  }
  if (gfx::Display::HasInternalDisplay() && !internal_display_connected) {
    if (display_info_.find(gfx::Display::InternalDisplayId()) ==
        display_info_.end()) {
      // Create a dummy internal display if the chrome restarted
      // in docked mode.
      DisplayInfo internal_display_info(
          gfx::Display::InternalDisplayId(),
          l10n_util::GetStringUTF8(IDS_ASH_INTERNAL_DISPLAY_NAME),
          false /*Internal display must not have overscan */);
      internal_display_info.SetBounds(gfx::Rect(0, 0, 800, 600));
      display_info_[gfx::Display::InternalDisplayId()] = internal_display_info;
    } else {
      // Internal display is no longer active. Reset its rotation to user
      // preference, so that it is restored when the internal display becomes
      // active again.
      gfx::Display::Rotation user_rotation =
          display_info_[gfx::Display::InternalDisplayId()].GetRotation(
              gfx::Display::ROTATION_SOURCE_USER);
      display_info_[gfx::Display::InternalDisplayId()].SetRotation(
          user_rotation, gfx::Display::ROTATION_SOURCE_USER);
    }
  }

#if defined(OS_CHROMEOS)
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      new_display_info_list.size() > 1) {
    display::DisplayIdList list = GenerateDisplayIdList(
        new_display_info_list.begin(), new_display_info_list.end(),
        [](const DisplayInfo& info) { return info.id(); });

    const display::DisplayLayout& layout =
        layout_store_->GetRegisteredDisplayLayout(list);
    // Mirror mode is set by DisplayConfigurator on the device.
    // Emulate it when running on linux desktop.
    if (layout.mirrored)
      SetMultiDisplayMode(MIRRORING);
  }
#endif

  UpdateDisplaysWith(new_display_info_list);
}

void DisplayManager::UpdateDisplays() {
  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_)
    display_info_list.push_back(GetDisplayInfo(display.id()));
  AddMirrorDisplayInfoIfAny(&display_info_list);
  UpdateDisplaysWith(display_info_list);
}

void DisplayManager::UpdateDisplaysWith(
    const std::vector<DisplayInfo>& updated_display_info_list) {
#if defined(OS_WIN)
  DCHECK_EQ(1u, updated_display_info_list.size()) <<
      ": Multiple display test does not work on Windows bots. Please "
      "skip (don't disable) the test using SupportsMultipleDisplays()";
#endif

  DisplayInfoList new_display_info_list = updated_display_info_list;
  std::sort(active_display_list_.begin(), active_display_list_.end(),
            DisplaySortFunctor());
  std::sort(new_display_info_list.begin(),
            new_display_info_list.end(),
            DisplayInfoSortFunctor());

  if (new_display_info_list.size() > 1) {
    display::DisplayIdList list = GenerateDisplayIdList(
        new_display_info_list.begin(), new_display_info_list.end(),
        [](const DisplayInfo& info) { return info.id(); });
    const display::DisplayLayout& layout =
        layout_store_->GetRegisteredDisplayLayout(list);
    current_default_multi_display_mode_ =
        (layout.default_unified && unified_desktop_enabled_) ? UNIFIED
                                                             : EXTENDED;
  }

  if (multi_display_mode_ != MIRRORING)
    multi_display_mode_ = current_default_multi_display_mode_;

  CreateSoftwareMirroringDisplayInfo(&new_display_info_list);

  // Close the mirroring window if any here to avoid creating two compositor on
  // one display.
  if (delegate_)
    delegate_->CloseMirroringDisplayIfNotNecessary();

  display::DisplayList new_displays;
  display::DisplayList removed_displays;
  std::map<size_t, uint32_t> display_changes;
  std::vector<size_t> added_display_indices;

  display::DisplayList::iterator curr_iter = active_display_list_.begin();
  DisplayInfoList::const_iterator new_info_iter = new_display_info_list.begin();

  while (curr_iter != active_display_list_.end() ||
         new_info_iter != new_display_info_list.end()) {
    if (curr_iter == active_display_list_.end()) {
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
      // Copy the info because |InsertAndUpdateDisplayInfo| updates the
      // instance.
      const DisplayInfo current_display_info =
          GetDisplayInfo(current_display.id());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      gfx::Display new_display =
          CreateDisplayFromDisplayInfoById(new_info_iter->id());
      const DisplayInfo& new_display_info = GetDisplayInfo(new_display.id());

      uint32_t metrics = gfx::DisplayObserver::DISPLAY_METRIC_NONE;

      // At that point the new Display objects we have are not entirely updated,
      // they are missing the translation related to the Display disposition in
      // the layout.
      // Using display.bounds() and display.work_area() would fail most of the
      // time.
      if (force_bounds_changed_ ||
          (current_display_info.bounds_in_native() !=
           new_display_info.bounds_in_native()) ||
          (current_display_info.GetOverscanInsetsInPixel() !=
           new_display_info.GetOverscanInsetsInPixel()) ||
          current_display.size() != new_display.size()) {
        metrics |= gfx::DisplayObserver::DISPLAY_METRIC_BOUNDS |
            gfx::DisplayObserver::DISPLAY_METRIC_WORK_AREA;
      }

      if (current_display.device_scale_factor() !=
          new_display.device_scale_factor()) {
        metrics |= gfx::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
      }

      if (current_display.rotation() != new_display.rotation())
        metrics |= gfx::DisplayObserver::DISPLAY_METRIC_ROTATION;

      if (metrics != gfx::DisplayObserver::DISPLAY_METRIC_NONE) {
        display_changes.insert(
            std::pair<size_t, uint32_t>(new_displays.size(), metrics));
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
  gfx::Display old_primary;
  if (delegate_)
    old_primary = screen_->GetPrimaryDisplay();

  // Clear focus if the display has been removed, but don't clear focus if
  // the destkop has been moved from one display to another
  // (mirror -> docked, docked -> single internal).
  bool clear_focus =
      !removed_displays.empty() &&
      !(removed_displays.size() == 1 && added_display_indices.size() == 1);
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(clear_focus);

  std::vector<size_t> updated_indices;
  UpdateNonPrimaryDisplayBoundsForLayout(&new_displays, &updated_indices);
  for (size_t updated_index : updated_indices) {
    if (std::find(added_display_indices.begin(), added_display_indices.end(),
                  updated_index) == added_display_indices.end()) {
      uint32_t metrics = gfx::DisplayObserver::DISPLAY_METRIC_BOUNDS |
                         gfx::DisplayObserver::DISPLAY_METRIC_WORK_AREA;
      if (display_changes.find(updated_index) != display_changes.end())
        metrics |= display_changes[updated_index];

      display_changes[updated_index] = metrics;
    }
  }

  active_display_list_ = new_displays;

  RefreshFontParams();
  base::AutoReset<bool> resetter(&change_display_upon_host_resize_, false);

  int active_display_list_size = active_display_list_.size();
  // Temporarily add displays to be removed because display object
  // being removed are accessed during shutting down the root.
  active_display_list_.insert(active_display_list_.end(),
                              removed_displays.begin(), removed_displays.end());

  for (const auto& display : removed_displays)
    screen_->NotifyDisplayRemoved(display);

  for (size_t index : added_display_indices)
    screen_->NotifyDisplayAdded(active_display_list_[index]);

  active_display_list_.resize(active_display_list_size);

  bool notify_primary_change =
      delegate_ ? old_primary.id() != screen_->GetPrimaryDisplay().id() : false;

  for (std::map<size_t, uint32_t>::iterator iter = display_changes.begin();
       iter != display_changes.end();
       ++iter) {
    uint32_t metrics = iter->second;
    const gfx::Display& updated_display = active_display_list_[iter->first];

    if (notify_primary_change &&
        updated_display.id() == screen_->GetPrimaryDisplay().id()) {
      metrics |= gfx::DisplayObserver::DISPLAY_METRIC_PRIMARY;
      notify_primary_change = false;
    }
    screen_->NotifyMetricsChanged(updated_display, metrics);
  }

  if (notify_primary_change) {
    // This happens when a primary display has moved to anther display without
    // bounds change.
    const gfx::Display& primary = screen_->GetPrimaryDisplay();
    if (primary.id() != old_primary.id()) {
      uint32_t metrics = gfx::DisplayObserver::DISPLAY_METRIC_PRIMARY;
      if (primary.size() != old_primary.size()) {
        metrics |= (gfx::DisplayObserver::DISPLAY_METRIC_BOUNDS |
                    gfx::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
      }
      if (primary.device_scale_factor() != old_primary.device_scale_factor())
        metrics |= gfx::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;

      screen_->NotifyMetricsChanged(primary, metrics);
    }
  }

  if (delegate_)
    delegate_->PostDisplayConfigurationChange();

#if defined(USE_X11) && defined(OS_CHROMEOS)
  if (!display_changes.empty() && base::SysInfo::IsRunningOnChromeOS())
    ui::ClearX11DefaultRootWindow();
#endif

  // Create the mirroring window asynchronously after all displays
  // are added so that it can mirror the display newly added. This can
  // happen when switching from dock mode to software mirror mode.
  CreateMirrorWindowAsyncIfAny();
}

const gfx::Display& DisplayManager::GetDisplayAt(size_t index) const {
  DCHECK_LT(index, active_display_list_.size());
  return active_display_list_[index];
}

const gfx::Display& DisplayManager::GetPrimaryDisplayCandidate() const {
  if (GetNumDisplays() != 2)
    return active_display_list_[0];
  const display::DisplayLayout& layout =
      layout_store_->GetRegisteredDisplayLayout(GetCurrentDisplayIdList());
  return GetDisplayForId(layout.primary_id);
}

size_t DisplayManager::GetNumDisplays() const {
  return active_display_list_.size();
}

bool DisplayManager::IsActiveDisplayId(int64_t display_id) const {
  return std::find_if(active_display_list_.begin(), active_display_list_.end(),
                      [display_id](const gfx::Display& display) {
                        return display.id() == display_id;
                      }) != active_display_list_.end();
}

bool DisplayManager::IsInMirrorMode() const {
  return mirroring_display_id_ != gfx::Display::kInvalidDisplayID;
}

void DisplayManager::SetUnifiedDesktopEnabled(bool enable) {
  unified_desktop_enabled_ = enable;
  // There is no need to update the displays in mirror mode. Doing
  // this in hardware mirroring mode can cause crash because display
  // info in hardware mirroring comes from DisplayConfigurator.
  if (!IsInMirrorMode())
    ReconfigureDisplays();
}

bool DisplayManager::IsInUnifiedMode() const {
  return multi_display_mode_ == UNIFIED &&
         !software_mirroring_display_list_.empty();
}

const DisplayInfo& DisplayManager::GetDisplayInfo(int64_t display_id) const {
  DCHECK_NE(gfx::Display::kInvalidDisplayID, display_id);

  std::map<int64_t, DisplayInfo>::const_iterator iter =
      display_info_.find(display_id);
  CHECK(iter != display_info_.end()) << display_id;
  return iter->second;
}

const gfx::Display DisplayManager::GetMirroringDisplayById(
    int64_t display_id) const {
  auto iter = std::find_if(software_mirroring_display_list_.begin(),
                           software_mirroring_display_list_.end(),
                           [display_id](const gfx::Display& display) {
                             return display.id() == display_id;
                           });
  return iter == software_mirroring_display_list_.end() ? gfx::Display()
                                                        : *iter;
}

std::string DisplayManager::GetDisplayNameForId(int64_t id) {
  if (id == gfx::Display::kInvalidDisplayID)
    return l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);

  std::map<int64_t, DisplayInfo>::const_iterator iter = display_info_.find(id);
  if (iter != display_info_.end() && !iter->second.name().empty())
    return iter->second.name();

  return base::StringPrintf("Display %d", static_cast<int>(id));
}

int64_t DisplayManager::GetDisplayIdForUIScaling() const {
  // UI Scaling is effective on internal display.
  return gfx::Display::HasInternalDisplay() ? gfx::Display::InternalDisplayId()
                                            : gfx::Display::kInvalidDisplayID;
}

void DisplayManager::SetMirrorMode(bool mirror) {
  // TODO(oshima): Enable mirror mode for 2> displays. crbug.com/589319.
  if (num_connected_displays() != 2)
    return;

#if defined(OS_CHROMEOS)
  if (base::SysInfo::IsRunningOnChromeOS()) {
    ui::MultipleDisplayState new_state =
        mirror ? ui::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR
               : ui::MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED;
    Shell::GetInstance()->display_configurator()->SetDisplayMode(new_state);
    return;
  }
#endif
  multi_display_mode_ =
      mirror ? MIRRORING : current_default_multi_display_mode_;
  ReconfigureDisplays();
}

void DisplayManager::AddRemoveDisplay() {
  DCHECK(!active_display_list_.empty());
  std::vector<DisplayInfo> new_display_info_list;
  const DisplayInfo& first_display =
      IsInUnifiedMode()
          ? GetDisplayInfo(software_mirroring_display_list_[0].id())
          : GetDisplayInfo(active_display_list_[0].id());
  new_display_info_list.push_back(first_display);
  // Add if there is only one display connected.
  if (num_connected_displays() == 1) {
    const int kVerticalOffsetPx = 100;
    // Layout the 2nd display below the primary as with the real device.
    gfx::Rect host_bounds = first_display.bounds_in_native();
    new_display_info_list.push_back(
        DisplayInfo::CreateFromSpec(base::StringPrintf(
            "%d+%d-600x%d", host_bounds.x(),
            host_bounds.bottom() + kVerticalOffsetPx, host_bounds.height())));
  }
  num_connected_displays_ = new_display_info_list.size();
  mirroring_display_id_ = gfx::Display::kInvalidDisplayID;
  software_mirroring_display_list_.clear();
  UpdateDisplaysWith(new_display_info_list);
}

void DisplayManager::ToggleDisplayScaleFactor() {
  DCHECK(!active_display_list_.empty());
  std::vector<DisplayInfo> new_display_info_list;
  for (display::DisplayList::const_iterator iter = active_display_list_.begin();
       iter != active_display_list_.end(); ++iter) {
    DisplayInfo display_info = GetDisplayInfo(iter->id());
    display_info.set_device_scale_factor(
        display_info.device_scale_factor() == 1.0f ? 2.0f : 1.0f);
    new_display_info_list.push_back(display_info);
  }
  AddMirrorDisplayInfoIfAny(&new_display_info_list);
  UpdateDisplaysWith(new_display_info_list);
}

#if defined(OS_CHROMEOS)
void DisplayManager::SetSoftwareMirroring(bool enabled) {
  SetMultiDisplayMode(enabled ? MIRRORING
                              : current_default_multi_display_mode_);
}

bool DisplayManager::SoftwareMirroringEnabled() const {
  return software_mirroring_enabled();
}
#endif

void DisplayManager::SetDefaultMultiDisplayModeForCurrentDisplays(
    MultiDisplayMode mode) {
  DCHECK_NE(MIRRORING, mode);
  display::DisplayIdList list = GetCurrentDisplayIdList();
  layout_store_->UpdateMultiDisplayState(list, IsInMirrorMode(),
                                         mode == UNIFIED);
  ReconfigureDisplays();
}

void DisplayManager::SetMultiDisplayMode(MultiDisplayMode mode) {
  multi_display_mode_ = mode;
  mirroring_display_id_ = gfx::Display::kInvalidDisplayID;
  software_mirroring_display_list_.clear();
}

void DisplayManager::ReconfigureDisplays() {
  DisplayInfoList display_info_list;
  for (const gfx::Display& display : active_display_list_) {
    if (display.id() == kUnifiedDisplayId)
      continue;
    display_info_list.push_back(GetDisplayInfo(display.id()));
  }
  for (const gfx::Display& display : software_mirroring_display_list_)
    display_info_list.push_back(GetDisplayInfo(display.id()));
  mirroring_display_id_ = gfx::Display::kInvalidDisplayID;
  software_mirroring_display_list_.clear();
  UpdateDisplaysWith(display_info_list);
}

bool DisplayManager::UpdateDisplayBounds(int64_t display_id,
                                         const gfx::Rect& new_bounds) {
  if (change_display_upon_host_resize_) {
    display_info_[display_id].SetBounds(new_bounds);
    // Don't notify observers if the mirrored window has changed.
    if (software_mirroring_enabled() && mirroring_display_id_ == display_id)
      return false;
    gfx::Display* display = FindDisplayForId(display_id);
    display->SetSize(display_info_[display_id].size_in_pixel());
    screen_->NotifyMetricsChanged(*display,
                                  gfx::DisplayObserver::DISPLAY_METRIC_BOUNDS);
    return true;
  }
  return false;
}

void DisplayManager::CreateMirrorWindowAsyncIfAny() {
  // Do not post a task if the software mirroring doesn't exist, or
  // during initialization when compositor's init task isn't posted yet.
  // ash::Shell::Init() will call this after the compositor is initialized.
  if (software_mirroring_display_list_.empty() || !delegate_)
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DisplayManager::CreateMirrorWindowIfAny,
                            weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<MouseWarpController> DisplayManager::CreateMouseWarpController(
    aura::Window* drag_source) const {
  if (IsInUnifiedMode() && num_connected_displays() >= 2)
    return base::WrapUnique(new UnifiedMouseWarpController());
  // Extra check for |num_connected_displays()| is for SystemDisplayApiTest
  // that injects MockScreen.
  if (GetNumDisplays() < 2 || num_connected_displays() < 2)
    return base::WrapUnique(new NullMouseWarpController());
  return base::WrapUnique(new ExtendedMouseWarpController(drag_source));
}

void DisplayManager::CreateScreenForShutdown() const {
  delete screen_for_shutdown;
  screen_for_shutdown = screen_->CloneForShutdown();
  gfx::Screen::SetScreenInstance(screen_for_shutdown);
}

void DisplayManager::UpdateInternalDisplayModeListForTest() {
  if (!gfx::Display::HasInternalDisplay() ||
      display_info_.count(gfx::Display::InternalDisplayId()) == 0)
    return;
  DisplayInfo* info = &display_info_[gfx::Display::InternalDisplayId()];
  SetInternalDisplayModeList(info);
}

void DisplayManager::CreateSoftwareMirroringDisplayInfo(
    DisplayInfoList* display_info_list) {
  // Use the internal display or 1st as the mirror source, then scale
  // the root window so that it matches the external display's
  // resolution. This is necessary in order for scaling to work while
  // mirrored.
  switch (multi_display_mode_) {
    case MIRRORING: {
      if (display_info_list->size() != 2)
        return;
      bool zero_is_source =
          first_display_id_ == (*display_info_list)[0].id() ||
          gfx::Display::IsInternalDisplayId((*display_info_list)[0].id());
      DCHECK_EQ(MIRRORING, multi_display_mode_);
      mirroring_display_id_ = (*display_info_list)[zero_is_source ? 1 : 0].id();

      int64_t display_id = mirroring_display_id_;
      auto iter =
          std::find_if(display_info_list->begin(), display_info_list->end(),
                       [display_id](const DisplayInfo& info) {
                         return info.id() == display_id;
                       });
      DCHECK(iter != display_info_list->end());

      DisplayInfo info = *iter;
      info.SetOverscanInsets(gfx::Insets());
      InsertAndUpdateDisplayInfo(info);
      software_mirroring_display_list_.push_back(
          CreateMirroringDisplayFromDisplayInfoById(mirroring_display_id_,
                                                    gfx::Point(), 1.0f));
      display_info_list->erase(iter);
      break;
    }
    case UNIFIED: {
      if (display_info_list->size() == 1)
        return;
      // TODO(oshima): Currently, all displays are laid out horizontally,
      // from left to right. Allow more flexible layouts, such as
      // right to left, or vertical layouts.
      gfx::Rect unified_bounds;
      software_mirroring_display_list_.clear();
      // 1st Pass. Find the max size.
      int max_height = std::numeric_limits<int>::min();

      int default_height = 0;
      float default_device_scale_factor = 1.0f;
      for (auto& info : *display_info_list) {
        max_height = std::max(max_height, info.size_in_pixel().height());
        if (!default_height || gfx::Display::IsInternalDisplayId(info.id())) {
          default_height = info.size_in_pixel().height();
          default_device_scale_factor = info.device_scale_factor();
        }
      }

      std::vector<DisplayMode> display_mode_list;
      std::set<std::pair<float, float>> dsf_scale_list;

      // 2nd Pass. Compute the unified display size.
      for (auto& info : *display_info_list) {
        InsertAndUpdateDisplayInfo(info);
        gfx::Point origin(unified_bounds.right(), 0);
        float scale =
            info.size_in_pixel().height() / static_cast<float>(max_height);
        // The display is scaled to fit the unified desktop size.
        gfx::Display display = CreateMirroringDisplayFromDisplayInfoById(
            info.id(), origin, 1.0f / scale);
        unified_bounds.Union(display.bounds());

        dsf_scale_list.insert(
            std::make_pair(info.device_scale_factor(), scale));
      }

      DisplayInfo info(kUnifiedDisplayId, "Unified Desktop", false);

      DisplayMode native_mode(unified_bounds.size(), 60.0f, false, true);
      std::vector<DisplayMode> modes =
          CreateUnifiedDisplayModeList(native_mode, dsf_scale_list);

      // Find the default mode.
      auto iter = std::find_if(
          modes.begin(), modes.end(),
          [default_height,
           default_device_scale_factor](const DisplayMode& mode) {
            return mode.size.height() == default_height &&
                   mode.device_scale_factor == default_device_scale_factor;
          });
      iter->native = true;
      info.SetDisplayModes(modes);
      info.set_device_scale_factor(iter->device_scale_factor);
      info.SetBounds(gfx::Rect(iter->size));

      // Forget the configured resolution if the original unified
      // desktop resolution has changed.
      if (display_info_.count(kUnifiedDisplayId) != 0 &&
          GetMaxNativeSize(display_info_[kUnifiedDisplayId]) !=
              unified_bounds.size()) {
        display_modes_.erase(kUnifiedDisplayId);
      }

      // 3rd Pass. Set the selected mode, then recompute the mirroring
      // display size.
      DisplayMode mode;
      if (GetSelectedModeForDisplayId(kUnifiedDisplayId, &mode) &&
          FindDisplayMode(info, mode) != info.display_modes().end()) {
        info.set_device_scale_factor(mode.device_scale_factor);
        info.SetBounds(gfx::Rect(mode.size));
      } else {
        display_modes_.erase(kUnifiedDisplayId);
      }

      int unified_display_height = info.size_in_pixel().height();
      gfx::Point origin;
      for (auto& info : *display_info_list) {
        float display_scale = info.size_in_pixel().height() /
                              static_cast<float>(unified_display_height);
        gfx::Display display = CreateMirroringDisplayFromDisplayInfoById(
            info.id(), origin, 1.0f / display_scale);
        origin.Offset(display.size().width(), 0);
        display.UpdateWorkAreaFromInsets(gfx::Insets());
        software_mirroring_display_list_.push_back(display);
      }

      display_info_list->clear();
      display_info_list->push_back(info);
      InsertAndUpdateDisplayInfo(info);
      break;
    }
    case EXTENDED:
      break;
  }
}

gfx::Display* DisplayManager::FindDisplayForId(int64_t id) {
  auto iter = std::find_if(
      active_display_list_.begin(), active_display_list_.end(),
      [id](const gfx::Display& display) { return display.id() == id; });
  if (iter != active_display_list_.end())
    return &(*iter);
  // TODO(oshima): This happens when a windows in unified desktop have
  // been moved to normal window. Fix this.
  if (id != kUnifiedDisplayId)
    DLOG(WARNING) << "Could not find display:" << id;
  return nullptr;
}

void DisplayManager::AddMirrorDisplayInfoIfAny(
    std::vector<DisplayInfo>* display_info_list) {
  if (software_mirroring_enabled() && IsInMirrorMode()) {
    display_info_list->push_back(GetDisplayInfo(mirroring_display_id_));
    software_mirroring_display_list_.clear();
  }
}

void DisplayManager::InsertAndUpdateDisplayInfo(const DisplayInfo& new_info) {
  std::map<int64_t, DisplayInfo>::iterator info =
      display_info_.find(new_info.id());
  if (info != display_info_.end()) {
    info->second.Copy(new_info);
  } else {
    display_info_[new_info.id()] = new_info;
    display_info_[new_info.id()].set_native(false);
  }
  display_info_[new_info.id()].UpdateDisplaySize();
  OnDisplayInfoUpdated(display_info_[new_info.id()]);
}

void DisplayManager::OnDisplayInfoUpdated(const DisplayInfo& display_info) {
#if defined(OS_CHROMEOS)
  ui::ColorCalibrationProfile color_profile = display_info.color_profile();
  if (color_profile != ui::COLOR_PROFILE_STANDARD) {
    Shell::GetInstance()->display_configurator()->SetColorCalibrationProfile(
        display_info.id(), color_profile);
  }
#endif
}

gfx::Display DisplayManager::CreateDisplayFromDisplayInfoById(int64_t id) {
  DCHECK(display_info_.find(id) != display_info_.end()) << "id=" << id;
  const DisplayInfo& display_info = display_info_[id];

  gfx::Display new_display(display_info.id());
  gfx::Rect bounds_in_native(display_info.size_in_pixel());
  float device_scale_factor = display_info.GetEffectiveDeviceScaleFactor();

  // Simply set the origin to (0,0).  The primary display's origin is
  // always (0,0) and the bounds of non-primary display(s) will be updated
  // in |UpdateNonPrimaryDisplayBoundsForLayout| called in |UpdateDisplay|.
  new_display.SetScaleAndBounds(
      device_scale_factor, gfx::Rect(bounds_in_native.size()));
  new_display.set_rotation(display_info.GetActiveRotation());
  new_display.set_touch_support(display_info.touch_support());
  new_display.set_maximum_cursor_size(display_info.maximum_cursor_size());
  return new_display;
}

gfx::Display DisplayManager::CreateMirroringDisplayFromDisplayInfoById(
    int64_t id,
    const gfx::Point& origin,
    float scale) {
  DCHECK(display_info_.find(id) != display_info_.end()) << "id=" << id;
  const DisplayInfo& display_info = display_info_[id];

  gfx::Display new_display(display_info.id());
  new_display.SetScaleAndBounds(
      1.0f, gfx::Rect(origin, gfx::ScaleToFlooredSize(
                                  display_info.size_in_pixel(), scale)));
  new_display.set_touch_support(display_info.touch_support());
  new_display.set_maximum_cursor_size(display_info.maximum_cursor_size());
  return new_display;
}

void DisplayManager::UpdateNonPrimaryDisplayBoundsForLayout(
    display::DisplayList* display_list,
    std::vector<size_t>* updated_indices) {
  if (display_list->size() == 1u)
    return;

  const display::DisplayLayout& layout =
      layout_store_->GetRegisteredDisplayLayout(
          CreateDisplayIdList(*display_list));

  // Ignore if a user has a old format (should be extremely rare)
  // and this will be replaced with DCHECK.
  if (layout.primary_id == gfx::Display::kInvalidDisplayID)
    return;

  // display_list does not have translation set, so ApplyDisplayLayout cannot
  // provide accurate change information. We'll find the changes after the call.
  ApplyDisplayLayout(layout, display_list, nullptr);
  size_t num_displays = display_list->size();
  for (size_t index = 0; index < num_displays; ++index) {
    const gfx::Display& display = (*display_list)[index];
    int64_t id = display.id();
    const gfx::Display* active_display = FindDisplayForId(id);
    if (!active_display || (active_display->bounds() != display.bounds()))
      updated_indices->push_back(index);
  }
}

void DisplayManager::CreateMirrorWindowIfAny() {
  if (software_mirroring_display_list_.empty() || !delegate_)
    return;
  DisplayInfoList list;
  for (auto& display : software_mirroring_display_list_)
    list.push_back(GetDisplayInfo(display.id()));
  delegate_->CreateOrUpdateMirroringDisplay(list);
}

void DisplayManager::ApplyDisplayLayout(const display::DisplayLayout& layout,
                                        display::DisplayList* display_list,
                                        std::vector<int64_t>* updated_ids) {
  layout.ApplyToDisplayList(display_list, updated_ids,
                            kMinimumOverlapForInvalidOffset);
}

void DisplayManager::RunPendingTasksForTest() {
  if (!software_mirroring_display_list_.empty())
    base::RunLoop().RunUntilIdle();
}

}  // namespace ash
