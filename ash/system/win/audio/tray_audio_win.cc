// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/win/audio/tray_audio_win.h"

#include "ash/system/win/audio/tray_audio_delegate_win.h"

namespace ash {

using system::TrayAudioDelegate;
using system::TrayAudioDelegateWin;

TrayAudioWin::TrayAudioWin(SystemTray* system_tray)
    : TrayAudio(system_tray,
                scoped_ptr<TrayAudioDelegate>(new TrayAudioDelegateWin())) {
}

TrayAudioWin::~TrayAudioWin() {
}

}  // namespace ash
