// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_AUDIO_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_AUDIO_CONTROLLER_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT AudioController {
 public:
  virtual ~AudioController() {}

  virtual void OnVolumeChanged(float percent) = 0;
};

}  // namespace ash

#endif  //ASH_SYSTEM_AUDIO_AUDIO_CONTROLLER_H_
