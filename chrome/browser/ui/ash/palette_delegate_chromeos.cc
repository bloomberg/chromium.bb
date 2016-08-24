// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/palette_delegate_chromeos.h"

#include "ash/accelerators/accelerator_controller_delegate_aura.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/utility/screenshot_controller.h"
#include "chrome/browser/chromeos/note_taking_app_utils.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace chromeos {
namespace {

Profile* GetProfile() {
  return ProfileManager::GetActiveUserProfile();
}

}  // namespace

PaletteDelegateChromeOS::PaletteDelegateChromeOS() {}

PaletteDelegateChromeOS::~PaletteDelegateChromeOS() {}

void PaletteDelegateChromeOS::CreateNote() {
  chromeos::LaunchNoteTakingAppForNewNote(GetProfile(), base::FilePath());
}

bool PaletteDelegateChromeOS::HasNoteApp() {
  return chromeos::IsNoteTakingAppAvailable(GetProfile());
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

}  // namespace chromeos
