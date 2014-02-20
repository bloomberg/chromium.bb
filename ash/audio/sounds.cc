// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/audio/sounds.h"

#include "ash/accessibility_delegate.h"
#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "base/command_line.h"

using media::SoundsManager;

namespace ash {

bool PlaySystemSoundAlways(media::SoundsManager::SoundKey key) {
  return SoundsManager::Get()->Play(key);
}

bool PlaySystemSoundIfSpokenFeedback(SoundsManager::SoundKey key) {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kAshEnableSystemSounds))
    return SoundsManager::Get()->Play(key);

  Shell* shell = Shell::GetInstance();
  if (!shell->accessibility_delegate()->IsSpokenFeedbackEnabled())
    return false;
  return SoundsManager::Get()->Play(key);
}

}  // namespace ash
