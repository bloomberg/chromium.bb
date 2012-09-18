// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"

#include <algorithm>

#include "ash/ash_switches.h"
#include "ash/display/multi_display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/json/json_value_converter.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#endif

namespace ash {
namespace {

// The maximum value for 'offset' in DisplayLayout in case of outliers.  Need
// to change this value in case to support even larger displays.
const int kMaxValidOffset = 10000;

// The number of pixels to overlap between the primary and secondary displays,
// in case that the offset value is too large.
const int kMinimumOverlapForInvalidOffset = 50;

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

}  // namespace

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

  std::string position_value;
  switch (layout.position) {
    case TOP:
      position_value = "top";
      break;
    case BOTTOM:
      position_value = "bottom";
      break;
    case RIGHT:
      position_value = "right";
      break;
    case LEFT:
      position_value = "left";
      break;
    default:
      return false;
  }

  dict_value->SetString("position", position_value);
  dict_value->SetInteger("offset", layout.offset);
  return true;
}

// static
void DisplayLayout::RegisterJSONConverter(
    base::JSONValueConverter<DisplayLayout>* converter) {
  converter->RegisterCustomField<Position>(
      "position", &DisplayLayout::position, &GetPositionFromString);
  converter->RegisterIntField("offset", &DisplayLayout::offset);
}

DisplayController::DisplayController() {
  aura::Env::GetInstance()->display_manager()->AddObserver(this);
}

DisplayController::~DisplayController() {
  aura::Env::GetInstance()->display_manager()->RemoveObserver(this);
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

void DisplayController::InitPrimaryDisplay() {
  aura::DisplayManager* display_manager =
      aura::Env::GetInstance()->display_manager();
  const gfx::Display* display = display_manager->GetDisplayAt(0);
  aura::RootWindow* root = AddRootWindowForDisplay(*display);
  root->SetHostBounds(display->bounds_in_pixel());
}

void DisplayController::InitSecondaryDisplays() {
  aura::DisplayManager* display_manager =
      aura::Env::GetInstance()->display_manager();
  for (size_t i = 1; i < display_manager->GetNumDisplays(); ++i) {
    const gfx::Display* display = display_manager->GetDisplayAt(i);
    aura::RootWindow* root = AddRootWindowForDisplay(*display);
    Shell::GetInstance()->InitRootWindowForSecondaryDisplay(root);
  }
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshSecondaryDisplayLayout)) {
    std::string value = command_line->GetSwitchValueASCII(
        switches::kAshSecondaryDisplayLayout);
    char layout;
    int offset;
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
  aura::DisplayManager* display_manager =
      aura::Env::GetInstance()->display_manager();
  return root_windows_[display_manager->GetDisplayAt(0)->id()];
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
  if (default_display_layout_.position != layout.position ||
      default_display_layout_.offset != layout.offset) {
    default_display_layout_ = layout;
    NotifyDisplayConfigurationChanging();
    UpdateDisplayBoundsForLayout();
  }
}

void DisplayController::SetLayoutForDisplayName(const std::string& name,
                                                const DisplayLayout& layout) {
  DisplayLayout& display_for_name = secondary_layouts_[name];
  if (display_for_name.position != layout.position ||
      display_for_name.offset != layout.offset) {
    secondary_layouts_[name] = layout;
    NotifyDisplayConfigurationChanging();
    UpdateDisplayBoundsForLayout();
  }
}

const DisplayLayout& DisplayController::GetLayoutForDisplayName(
    const std::string& name) {
  std::map<std::string, DisplayLayout>::const_iterator it =
      secondary_layouts_.find(name);

  if (it != secondary_layouts_.end())
    return it->second;
  return default_display_layout_;
}

void DisplayController::OnDisplayBoundsChanged(const gfx::Display& display) {
  NotifyDisplayConfigurationChanging();
  root_windows_[display.id()]->SetHostBounds(display.bounds_in_pixel());
  UpdateDisplayBoundsForLayout();
}

void DisplayController::OnDisplayAdded(const gfx::Display& display) {
  DCHECK(!root_windows_.empty());
  NotifyDisplayConfigurationChanging();
  aura::RootWindow* root = AddRootWindowForDisplay(display);
  Shell::GetInstance()->InitRootWindowForSecondaryDisplay(root);
  UpdateDisplayBoundsForLayout();
}

void DisplayController::OnDisplayRemoved(const gfx::Display& display) {
  aura::RootWindow* root = root_windows_[display.id()];
  DCHECK(root);
  // Primary display should never be removed by DisplayManager.
  DCHECK(root != GetPrimaryRootWindow());
  NotifyDisplayConfigurationChanging();
  // Display for root window will be deleted when the Primary RootWindow
  // is deleted by the Shell.
  if (root != GetPrimaryRootWindow()) {
    root_windows_.erase(display.id());
    internal::RootWindowController* controller =
        GetRootWindowController(root);
    DCHECK(controller);
    controller->MoveWindowsTo(GetPrimaryRootWindow());
    // Delete most of root window related objects, but don't delete
    // root window itself yet because the stak may be using it.
    controller->Shutdown();
    MessageLoop::current()->DeleteSoon(FROM_HERE, controller);
  }
}

aura::RootWindow* DisplayController::AddRootWindowForDisplay(
    const gfx::Display& display) {
  aura::RootWindow* root = aura::Env::GetInstance()->display_manager()->
      CreateRootWindowForDisplay(display);
  root_windows_[display.id()] = root;

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
  if (gfx::Screen::GetNumDisplays() <= 1)
    return;

  DCHECK_EQ(2, gfx::Screen::GetNumDisplays());
  aura::DisplayManager* display_manager =
      aura::Env::GetInstance()->display_manager();
  const gfx::Rect& primary_bounds = display_manager->GetDisplayAt(0)->bounds();
  gfx::Display* secondary_display = display_manager->GetDisplayAt(1);
  const std::string& secondary_name = display_manager->GetDisplayNameAt(1);
  const gfx::Rect& secondary_bounds = secondary_display->bounds();
  gfx::Point new_secondary_origin = primary_bounds.origin();

  const DisplayLayout* layout = &default_display_layout_;
  std::map<std::string, DisplayLayout>::const_iterator iter =
      secondary_layouts_.find(secondary_name);
  if (iter != secondary_layouts_.end())
    layout = &iter->second;

  DisplayLayout::Position position = layout->position;

  // Ignore the offset in case the secondary display doesn't share edges with
  // the primary display.
  int offset = layout->offset;
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

}  // namespace ash
