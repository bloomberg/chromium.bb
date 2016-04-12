// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_AUDIO_A11Y_CONTROLLER_H_
#define CHROMEOS_AUDIO_AUDIO_A11Y_CONTROLLER_H_

#include <stdint.h>

#include "base/macros.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

class CHROMEOS_EXPORT AudioA11yController {
 public:
  AudioA11yController();
  ~AudioA11yController();

  // Set audio outputs mono or not (that is stereo).
  void SetOutputMono(bool enabled);

 private:
  CrasAudioHandler* cras_audio_handler_;

  DISALLOW_COPY_AND_ASSIGN(AudioA11yController);
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_AUDIO_A11Y_CONTROLLER_H_
