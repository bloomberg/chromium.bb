// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_CHROMEOS_SOUNDS_H_
#define CHROMEOS_AUDIO_CHROMEOS_SOUNDS_H_

// This file declares sound resources keys for ChromeOS.
namespace chromeos {

enum {
  SOUND_START = 0,
  SOUND_STARTUP = SOUND_START,
  SOUND_LOCK,
  SOUND_UNLOCK,
  SOUND_SHUTDOWN,
  SOUND_SPOKEN_FEEDBACK_ENABLED,
  SOUND_SPOKEN_FEEDBACK_DISABLED
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_CHROMEOS_SOUNDS_H_
