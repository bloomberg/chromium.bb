// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/palette_delegate_chromeos.h"

#include "ash/accelerators/accelerator_controller_delegate_aura.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/utility/screenshot_controller.h"
#include "chrome/browser/chromeos/note_taking_app_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/events/devices/input_device_manager.h"

namespace chromeos {
namespace {

Profile* GetProfile() {
  return ProfileManager::GetActiveUserProfile();
}

}  // namespace

PaletteDelegateChromeOS::PaletteDelegateChromeOS() {
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
}

PaletteDelegateChromeOS::~PaletteDelegateChromeOS() {
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
}

void PaletteDelegateChromeOS::CreateNote() {
  chromeos::LaunchNoteTakingAppForNewNote(GetProfile(), base::FilePath());
}

bool PaletteDelegateChromeOS::HasNoteApp() {
  return chromeos::IsNoteTakingAppAvailable(GetProfile());
}

void PaletteDelegateChromeOS::SetPartialMagnifierState(bool enabled) {
  ash::PartialMagnificationController* controller =
      ash::Shell::GetInstance()->partial_magnification_controller();
  controller->SetEnabled(enabled);
}

void PaletteDelegateChromeOS::SetStylusStateChangedCallback(
    const OnStylusStateChangedCallback& on_stylus_state_changed) {
  on_stylus_state_changed_ = on_stylus_state_changed;
}

bool PaletteDelegateChromeOS::ShouldAutoOpenPalette() {
  return GetProfile()->GetPrefs()->GetBoolean(
      prefs::kLaunchPaletteOnEjectEvent);
}

void PaletteDelegateChromeOS::TakeScreenshot() {
  auto* screenshot_delegate = ash::Shell::GetInstance()
                                  ->accelerator_controller_delegate()
                                  ->screenshot_delegate();
  screenshot_delegate->HandleTakeScreenshotForAllRootWindows();
}

void PaletteDelegateChromeOS::TakePartialScreenshot() {
  auto* screenshot_controller =
      ash::Shell::GetInstance()->screenshot_controller();
  auto* screenshot_delegate = ash::Shell::GetInstance()
                                  ->accelerator_controller_delegate()
                                  ->screenshot_delegate();

  screenshot_controller->set_pen_events_only(true);
  screenshot_controller->StartPartialScreenshotSession(
      screenshot_delegate, false /* draw_overlay_immediately */);
}

void PaletteDelegateChromeOS::OnStylusStateChanged(ui::StylusState state) {
  on_stylus_state_changed_.Run(state);
}

}  // namespace chromeos
