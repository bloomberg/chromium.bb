// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUDIO_SOUNDS_H_
#define ASH_AUDIO_SOUNDS_H_

#include "ash/ash_export.h"
#include "media/audio/sounds/sounds_manager.h"

namespace ash {

// A wrapper around media::SoundsManager::Play() method, which plays
// sound identified by |key| iff at least one of the following
// conditions is true:
// * ash::switches::kAshEnableSystemSounds flag is set
// * |honor_spoken_feedback| is true and spoken feedback is enabled
// * ash::switches::kAshEnableSystemSounds is not set and
//   |honor_spoken_feedback| is false
//
// Currently the latter case is applied for startup sound at OOBE screen
// and ChromeVox enable/disable sounds.
ASH_EXPORT bool PlaySystemSound(media::SoundsManager::SoundKey key,
                                bool honor_spoken_feedback);

}  // namespace ash

#endif  // ASH_AUDIO_SOUNDS_H_
