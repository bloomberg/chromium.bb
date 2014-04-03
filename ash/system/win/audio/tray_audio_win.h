// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_WIN_AUDIO_TRAY_AUDIO_WIN_H_
#define ASH_SYSTEM_WIN_AUDIO_TRAY_AUDIO_WIN_H_

#include "ash/ash_export.h"
#include "ash/system/audio/tray_audio.h"
#include "base/memory/scoped_ptr.h"

namespace ash {

class TrayAudioWin : public TrayAudio {
 public:
  explicit TrayAudioWin(SystemTray* system_tray);
  virtual ~TrayAudioWin();

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayAudioWin);
};

}  // namespace ash

#endif  // ASH_SYSTEM_WIN_AUDIO_TRAY_AUDIO_WIN_H_
