// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/audio/sounds.h"

#include "ash/common/accessibility_delegate.h"
#include "ash/common/ash_switches.h"
#include "ash/common/wm_shell.h"
#include "base/command_line.h"

using media::SoundsManager;

namespace ash {

bool PlaySystemSoundAlways(SoundsManager::SoundKey key) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshDisableSystemSounds))
    return false;
  return SoundsManager::Get()->Play(key);
}

bool PlaySystemSoundIfSpokenFeedback(SoundsManager::SoundKey key) {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kAshDisableSystemSounds))
    return false;

  if (cl->HasSwitch(switches::kAshEnableSystemSounds))
    return SoundsManager::Get()->Play(key);

  if (!WmShell::Get()->GetAccessibilityDelegate()->IsSpokenFeedbackEnabled())
    return false;

  return SoundsManager::Get()->Play(key);
}

}  // namespace ash
