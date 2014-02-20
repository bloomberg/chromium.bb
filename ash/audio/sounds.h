// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUDIO_SOUNDS_H_
#define ASH_AUDIO_SOUNDS_H_

#include "ash/ash_export.h"
#include "media/audio/sounds/sounds_manager.h"

namespace ash {

// A wrapper around media::SoundsManager::Play() method, which plays sound
// identified by |key|. Returns true when sound is successfully played.
ASH_EXPORT bool PlaySystemSoundAlways(media::SoundsManager::SoundKey key);

// A wrapper around media::SoundsManager::Play() method, which plays
// sound identified by |key| iff at least one of the following
// conditions is true:
// * ash::switches::kAshEnableSystemSounds flag is set
// * spoken feedback is enabled
// Returns true when sound is succesfully played.
ASH_EXPORT bool PlaySystemSoundIfSpokenFeedback(
    media::SoundsManager::SoundKey key);

}  // namespace ash

#endif  // ASH_AUDIO_SOUNDS_H_
