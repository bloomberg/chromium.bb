// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/palette_delegate_chromeos.h"

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

}  // namespace chromeos
