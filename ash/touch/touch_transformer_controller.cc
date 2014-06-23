// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_transformer_controller.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/display/types/chromeos/display_snapshot.h"
#include "ui/events/device_data_manager.h"

namespace ash {

namespace {

DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

}  // namespace

// This function computes the extended mode TouchTransformer for
// |touch_display|. The TouchTransformer maps the touch event position
// from framebuffer size to the display size.
gfx::Transform
TouchTransformerController::GetExtendedModeTouchTransformer(
    const DisplayInfo& touch_display, const gfx::Size& fb_size) const {
  gfx::Transform ctm;
  if (touch_display.touch_device_id() == 0 ||
      fb_size.width() == 0.0 ||
      fb_size.height() == 0.0)
    return ctm;
  float width = touch_display.bounds_in_native().width();
  float height = touch_display.bounds_in_native().height();
  ctm.Scale(width / fb_size.width(), height / fb_size.height());
  return ctm;
}

bool TouchTransformerController::ShouldComputeMirrorModeTouchTransformer(
    const DisplayInfo& touch_display) const {
  if (force_compute_mirror_mode_touch_transformer_)
    return true;

  if (touch_display.touch_device_id() == 0)
    return false;

  const ui::DisplayConfigurator::DisplayState* state = NULL;
  const std::vector<ui::DisplayConfigurator::DisplayState>& cached_displays =
      Shell::GetInstance()->display_configurator()->cached_displays();
  for (size_t i = 0; i < cached_displays.size(); i++) {
    if (cached_displays[i].touch_device_id == touch_display.touch_device_id()) {
      state = &cached_displays[i];
      break;
    }
  }

  if (!state || state->mirror_mode == state->display->native_mode() ||
      !state->display->is_aspect_preserving_scaling()) {
    return false;
  }
  return true;
}

// This function computes the mirror mode TouchTransformer for |touch_display|.
// When internal monitor is applied a resolution that does not have
// the same aspect ratio as its native resolution, there would be
// blank regions in the letterboxing/pillarboxing mode.
// The TouchTransformer will make sure the touch events on the blank region
// have negative coordinates and touch events within the chrome region
// have the correct positive coordinates.
gfx::Transform TouchTransformerController::GetMirrorModeTouchTransformer(
    const DisplayInfo& touch_display) const {
  gfx::Transform ctm;
  if (!ShouldComputeMirrorModeTouchTransformer(touch_display))
    return ctm;

  float mirror_width = touch_display.bounds_in_native().width();
  float mirror_height = touch_display.bounds_in_native().height();
  float native_width = 0;
  float native_height = 0;

  std::vector<DisplayMode> modes = touch_display.display_modes();
  for (size_t i = 0; i < modes.size(); i++) {
       if (modes[i].native) {
         native_width = modes[i].size.width();
         native_height = modes[i].size.height();
         break;
       }
  }

  if (native_height == 0.0 || mirror_height == 0.0 ||
      native_width == 0.0 || mirror_width == 0.0)
    return ctm;

  float native_ar = static_cast<float>(native_width) /
      static_cast<float>(native_height);
  float mirror_ar = static_cast<float>(mirror_width) /
      static_cast<float>(mirror_height);

  if (mirror_ar > native_ar) {  // Letterboxing
    // Translate before scale.
    ctm.Translate(0.0, (1.0 - mirror_ar / native_ar) * 0.5 * mirror_height);
    ctm.Scale(1.0, mirror_ar / native_ar);
    return ctm;
  }

  if (native_ar > mirror_ar) {  // Pillarboxing
    // Translate before scale.
    ctm.Translate((1.0 - native_ar / mirror_ar) * 0.5 * mirror_width, 0.0);
    ctm.Scale(native_ar / mirror_ar, 1.0);
    return ctm;
  }

  return ctm;  // Same aspect ratio - return identity
}

TouchTransformerController::TouchTransformerController() :
    force_compute_mirror_mode_touch_transformer_ (false) {
  Shell::GetInstance()->display_controller()->AddObserver(this);
}

TouchTransformerController::~TouchTransformerController() {
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
}

void TouchTransformerController::UpdateTouchTransformer() const {
  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  device_manager->ClearTouchTransformerRecord();

  // Display IDs and DisplayInfo for mirror or extended mode.
  int64 display1_id = gfx::Display::kInvalidDisplayID;
  int64 display2_id = gfx::Display::kInvalidDisplayID;
  DisplayInfo display1;
  DisplayInfo display2;
  // Display ID and DisplayInfo for single display mode.
  int64 single_display_id = gfx::Display::kInvalidDisplayID;
  DisplayInfo single_display;

  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  ui::MultipleDisplayState display_state =
      Shell::GetInstance()->display_configurator()->display_state();
  if (display_state == ui::MULTIPLE_DISPLAY_STATE_INVALID ||
      display_state == ui::MULTIPLE_DISPLAY_STATE_HEADLESS) {
    return;
  } else if (display_state == ui::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR ||
             display_state == ui::MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED) {
    // TODO(miletus) : Handle DUAL_EXTENDED with software mirroring.
    DisplayIdPair id_pair = GetDisplayManager()->GetCurrentDisplayIdPair();
    display1_id = id_pair.first;
    display2_id = id_pair.second;
    DCHECK(display1_id != gfx::Display::kInvalidDisplayID &&
           display2_id != gfx::Display::kInvalidDisplayID);
    display1 = GetDisplayManager()->GetDisplayInfo(display1_id);
    display2 = GetDisplayManager()->GetDisplayInfo(display2_id);
  } else {
    single_display_id = GetDisplayManager()->first_display_id();
    DCHECK(single_display_id != gfx::Display::kInvalidDisplayID);
    single_display = GetDisplayManager()->GetDisplayInfo(single_display_id);
  }

  if (display_state == ui::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR) {
    // In mirror mode, both displays share the same root window so
    // both display ids are associated with the root window.
    aura::Window* root = display_controller->GetPrimaryRootWindow();
    RootWindowController::ForWindow(root)->ash_host()->UpdateDisplayID(
        display1_id, display2_id);
    device_manager->UpdateTouchInfoForDisplay(
        display1_id,
        display1.touch_device_id(),
        GetMirrorModeTouchTransformer(display1));
    device_manager->UpdateTouchInfoForDisplay(
        display2_id,
        display2.touch_device_id(),
        GetMirrorModeTouchTransformer(display2));
    return;
  }

  if (display_state == ui::MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED) {
    // TODO(miletus) : Handle the case the state is DUAL_EXTENDED but it
    // is actually doing software mirroring.
    if (GetDisplayManager()->software_mirroring_enabled())
      return;
    // In extended mode, each display is associated with one root window.
    aura::Window* root1 =
        display_controller->GetRootWindowForDisplayId(display1_id);
    aura::Window* root2 =
        display_controller->GetRootWindowForDisplayId(display2_id);
    RootWindowController::ForWindow(root1)->ash_host()->UpdateDisplayID(
        display1_id, gfx::Display::kInvalidDisplayID);
    RootWindowController::ForWindow(root2)->ash_host()->UpdateDisplayID(
        display2_id, gfx::Display::kInvalidDisplayID);
    gfx::Size fb_size =
        Shell::GetInstance()->display_configurator()->framebuffer_size();
    device_manager->UpdateTouchInfoForDisplay(
        display1_id,
        display1.touch_device_id(),
        GetExtendedModeTouchTransformer(display1, fb_size));
    device_manager->UpdateTouchInfoForDisplay(
        display2_id,
        display2.touch_device_id(),
        GetExtendedModeTouchTransformer(display2, fb_size));
    return;
  }

  // Single display mode. The root window has one associated display id.
  aura::Window* root =
      display_controller->GetRootWindowForDisplayId(single_display.id());
  RootWindowController::ForWindow(root)->ash_host()->UpdateDisplayID(
      single_display.id(), gfx::Display::kInvalidDisplayID);
  device_manager->UpdateTouchInfoForDisplay(single_display_id,
                                            single_display.touch_device_id(),
                                            gfx::Transform());
}

void TouchTransformerController::OnDisplaysInitialized() {
  UpdateTouchTransformer();
}

void TouchTransformerController::OnDisplayConfigurationChanged() {
  UpdateTouchTransformer();
}

}  // namespace ash
