// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/multi_display_manager.h"

#include <string>
#include <vector>

#include "ash/display/display_controller.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/aura/window_property.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chromeos/display/output_configurator.h"
#endif

DECLARE_WINDOW_PROPERTY_TYPE(int64);

namespace ash {
namespace internal {
namespace {

gfx::Display& GetInvalidDisplay() {
  static gfx::Display* invalid_display = new gfx::Display();
  return *invalid_display;
}

}  // namespace

using aura::RootWindow;
using aura::Window;
using std::string;
using std::vector;

DEFINE_WINDOW_PROPERTY_KEY(int64, kDisplayIdKey,
                           gfx::Display::kInvalidDisplayID);

MultiDisplayManager::MultiDisplayManager() :
    internal_display_id_(gfx::Display::kInvalidDisplayID) {
  Init();
}

MultiDisplayManager::~MultiDisplayManager() {
}

// static
void MultiDisplayManager::CycleDisplay() {
  MultiDisplayManager* manager = static_cast<MultiDisplayManager*>(
      aura::Env::GetInstance()->display_manager());
  manager->CycleDisplayImpl();
}

// static
void MultiDisplayManager::ToggleDisplayScale() {
  MultiDisplayManager* manager = static_cast<MultiDisplayManager*>(
      aura::Env::GetInstance()->display_manager());
  manager->ScaleDisplayImpl();
}

void MultiDisplayManager::InitInternalDisplayInfo() {
#if defined(OS_CHROMEOS)
  if (!base::chromeos::IsRunningOnChromeOS())
    return;
  std::vector<XID> outputs;
  ui::GetOutputDeviceHandles(&outputs);
  std::vector<std::string> output_names = ui::GetOutputNames(outputs);
  for (size_t i = 0; i < output_names.size(); ++i) {
    if (chromeos::OutputConfigurator::IsInternalOutputName(
            output_names[i])) {
      XID internal_output = outputs[i];
      uint16 manufacturer_id = 0;
      uint32 serial_number = 0;
      ui::GetOutputDeviceData(
          internal_output, &manufacturer_id, &serial_number, NULL);
      internal_display_id_ =
          gfx::Display::GetID(manufacturer_id, serial_number);
      return;
    }
  }
#endif
}

bool MultiDisplayManager::HasInternalDisplay() const {
  return internal_display_id_ != gfx::Display::kInvalidDisplayID;
}

bool MultiDisplayManager::UpdateWorkAreaOfDisplayNearestWindow(
    const aura::Window* window,
    const gfx::Insets& insets) {
  const RootWindow* root = window->GetRootWindow();
  gfx::Display& display = FindDisplayForRootWindow(root);
  gfx::Rect old_work_area = display.work_area();
  display.UpdateWorkAreaFromInsets(insets);
  return old_work_area != display.work_area();
}

const gfx::Display& MultiDisplayManager::FindDisplayContainingPoint(
    const gfx::Point& point_in_screen) const {
  for (std::vector<gfx::Display>::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    const gfx::Display& display = *iter;
    if (display.bounds().Contains(point_in_screen))
      return display;
  }
  return GetInvalidDisplay();
}

void MultiDisplayManager::OnNativeDisplaysChanged(
    const std::vector<gfx::Display>& updated_displays) {
  if (updated_displays.size() == 0) {
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
  std::vector<gfx::Display> new_displays;
  if (internal_display_id_ != gfx::Display::kInvalidDisplayID) {
    bool internal_display_connected = false;
    for (Displays::const_iterator iter = updated_displays.begin();
         iter != updated_displays.end(); ++iter) {
      if ((*iter).id() == internal_display_id_) {
        internal_display_connected = true;
        // Update the internal display cache.
        internal_display_.reset(new gfx::Display);
        *internal_display_.get() = *iter;
        break;
      }
    }
    // If the internal display wasn't connected, use the cached value.
    if (!internal_display_connected)
      new_displays.push_back(*internal_display_.get());
    new_displays.insert(
        new_displays.end(), updated_displays.begin(), updated_displays.end());
  } else {
    new_displays = updated_displays;
  }

  size_t min = std::min(displays_.size(), new_displays.size());

  // TODO(oshima): Fix this so that we can differentiate outputs
  // and keep a content on one display stays on the same display
  // when a display is added or removed.
  for (size_t i = 0; i < min; ++i) {
    gfx::Display& current_display = displays_[i];
    const gfx::Display& new_display = new_displays[i];
    if (current_display.bounds_in_pixel() != new_display.bounds_in_pixel() ||
        current_display.device_scale_factor() !=
        new_display.device_scale_factor()) {
      current_display.SetScaleAndBounds(new_display.device_scale_factor(),
                                        new_display.bounds_in_pixel());
      NotifyBoundsChanged(current_display);
    }
  }

  if (displays_.size() < new_displays.size()) {
    // New displays added
    for (size_t i = min; i < new_displays.size(); ++i) {
      const gfx::Display& new_display = new_displays[i];
      displays_.push_back(gfx::Display(new_display.id()));
      gfx::Display& display = displays_.back();
      display.SetScaleAndBounds(new_display.device_scale_factor(),
                                new_display.bounds_in_pixel());
      NotifyDisplayAdded(display);
    }
  } else {
    // Displays are removed. We keep the display for the primary
    // display (at index 0) because it needs the display information
    // even if it doesn't exit.
    while (displays_.size() > new_displays.size() && displays_.size() > 1) {
      Displays::reverse_iterator iter = displays_.rbegin();
      NotifyDisplayRemoved(*iter);
      displays_.erase(iter.base() - 1);
    }
  }
}

RootWindow* MultiDisplayManager::CreateRootWindowForDisplay(
    const gfx::Display& display) {
  RootWindow* root_window =
      new RootWindow(RootWindow::CreateParams(display.bounds_in_pixel()));
  // No need to remove RootWindowObserver because
  // the DisplayManager object outlives RootWindow objects.
  root_window->AddRootWindowObserver(this);
  root_window->SetProperty(kDisplayIdKey, display.id());
  root_window->Init();
  return root_window;
}

gfx::Display* MultiDisplayManager::GetDisplayAt(size_t index) {
  return index < displays_.size() ? &displays_[index] : NULL;
}

size_t MultiDisplayManager::GetNumDisplays() const {
  return displays_.size();
}

const gfx::Display& MultiDisplayManager::GetDisplayNearestWindow(
    const Window* window) const {
  if (!window)
    return displays_[0];
  const RootWindow* root = window->GetRootWindow();
  MultiDisplayManager* manager = const_cast<MultiDisplayManager*>(this);
  return root ? manager->FindDisplayForRootWindow(root) : GetInvalidDisplay();
}

const gfx::Display& MultiDisplayManager::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  // Fallback to the primary display if there is no root display containing
  // the |point|.
  const gfx::Display& display = FindDisplayContainingPoint(point);
  return display.is_valid() ? display : displays_[0];
}

const gfx::Display& MultiDisplayManager::GetDisplayMatching(
    const gfx::Rect& rect) const {
  if (rect.IsEmpty())
    return GetDisplayNearestPoint(rect.origin());

  int max = 0;
  const gfx::Display* matching = 0;
  for (std::vector<gfx::Display>::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    const gfx::Display& display = *iter;
    gfx::Rect intersect = display.bounds().Intersect(rect);
    int area = intersect.width() * intersect.height();
    if (area > max) {
      max = area;
      matching = &(*iter);
    }
  }
  // Fallback to the primary display if there is no matching display.
  return matching ? *matching : displays_[0];
}

std::string MultiDisplayManager::GetDisplayNameAt(size_t index) {
#if defined(USE_X11)
  gfx::Display* display = GetDisplayAt(index);
  std::vector<XID> outputs;
  if (display && display->id() != gfx::Display::kInvalidDisplayID &&
      ui::GetOutputDeviceHandles(&outputs)) {
    for (size_t i = 0; i < outputs.size(); ++i) {
      uint16 manufacturer_id = 0;
      uint32 serial_number = 0;
      std::string name;
      if (ui::GetOutputDeviceData(
              outputs[i], &manufacturer_id, &serial_number, &name) &&
          display->id() ==
          gfx::Display::GetID(manufacturer_id, serial_number)) {
        return name;
      }
    }
  }
#endif

  return base::StringPrintf("Display %d", static_cast<int>(index + 1));
}

void MultiDisplayManager::OnRootWindowResized(const aura::RootWindow* root,
                                              const gfx::Size& old_size) {
  if (!use_fullscreen_host_window()) {
    gfx::Display& display = FindDisplayForRootWindow(root);
    if (display.size() != root->GetHostSize()) {
      display.SetSize(root->GetHostSize());
      NotifyBoundsChanged(display);
    }
  }
}

void MultiDisplayManager::Init() {
  // TODO(oshima): Move this logic to DisplayChangeObserver.
  const string size_str = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAuraHostWindowSize);
  vector<string> parts;
  base::SplitString(size_str, ',', &parts);
  for (vector<string>::const_iterator iter = parts.begin();
       iter != parts.end(); ++iter) {
    AddDisplayFromSpec(*iter);
  }
  if (displays_.empty())
    AddDisplayFromSpec(std::string() /* default */);
}

void MultiDisplayManager::CycleDisplayImpl() {
  std::vector<gfx::Display> new_displays;
  if (displays_.size() > 1) {
    // Remove if there is more than one display.
    int count = displays_.size() - 1;
    for (Displays::const_iterator iter = displays_.begin(); count-- > 0; ++iter)
      new_displays.push_back(*iter);
  } else {
    // Add if there is only one display.
    new_displays.push_back(displays_[0]);
    new_displays.push_back(CreateDisplayFromSpec("100+200-500x400"));
  }
  if (new_displays.size())
    OnNativeDisplaysChanged(new_displays);
}

void MultiDisplayManager::ScaleDisplayImpl() {
  if (displays_.size() > 0) {
    std::vector<gfx::Display> new_displays;
    for (Displays::const_iterator iter = displays_.begin();
         iter != displays_.end(); ++iter) {
      gfx::Display display = *iter;
      float factor = display.device_scale_factor() == 1.0f ? 2.0f : 1.0f;
      display.SetScaleAndBounds(
          factor, gfx::Rect(display.bounds_in_pixel().origin(),
                            display.size().Scale(factor)));
      new_displays.push_back(display);
    }
    OnNativeDisplaysChanged(new_displays);
  }
}

gfx::Display& MultiDisplayManager::FindDisplayForRootWindow(
    const aura::RootWindow* root_window) {
  int64 id = root_window->GetProperty(kDisplayIdKey);
  // if id is |kInvaildDisplayID|, it's being deleted.
  DCHECK(id != gfx::Display::kInvalidDisplayID);
  for (Displays::iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    if ((*iter).id() == id)
      return *iter;
  }
  DLOG(FATAL) << "Could not find display by id:" << id;
  return GetInvalidDisplay();
}

void MultiDisplayManager::AddDisplayFromSpec(const std::string& spec) {
  gfx::Display display = CreateDisplayFromSpec(spec);

  const gfx::Insets insets = display.GetWorkAreaInsets();
  const gfx::Rect& native_bounds = display.bounds_in_pixel();
  display.SetScaleAndBounds(display.device_scale_factor(), native_bounds);
  display.UpdateWorkAreaFromInsets(insets);
  displays_.push_back(display);
}

int64 MultiDisplayManager::EnableInternalDisplayForTest() {
  const int64 kInternalDisplayIdForTest = 9999;
  internal_display_id_ = kInternalDisplayIdForTest;
  internal_display_.reset(new gfx::Display(internal_display_id_,
                                           gfx::Rect(800, 600)));
  return kInternalDisplayIdForTest;
}

}  // namespace internal
}  // namespace ash
