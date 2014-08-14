// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/win/audio/tray_audio_delegate_win.h"

#include <audiopolicy.h>
#include <cmath>

#include "media/audio/win/core_audio_util_win.h"

using base::win::ScopedComPtr;

namespace {

// Volume value which should be considered as muted in range [0, 100].
const int kMuteThresholdPercent = 1;

// Lowest volume which is considered to be audible in the range [0, 100].
const int kDefaultUnmuteVolumePercent = 4;

}  // namespace

namespace ash  {
namespace system {

void TrayAudioDelegateWin::AdjustOutputVolumeToAudibleLevel() {
  if (GetOutputVolumeLevel() <= kMuteThresholdPercent)
    SetOutputVolumeLevel(kDefaultUnmuteVolumePercent);
}

int TrayAudioDelegateWin::GetOutputDefaultVolumeMuteLevel() {
  return kMuteThresholdPercent;
}

int TrayAudioDelegateWin::GetOutputVolumeLevel() {
  ScopedComPtr<ISimpleAudioVolume> volume_control =
      CreateDefaultVolumeControl();
  if (!volume_control)
    return 0;

  float level = 0.0f;
  if (FAILED(volume_control->GetMasterVolume(&level)))
    return 0;

  // MSVC prior to 2013 doesn't have a round function.  The below code is not
  // conformant to C99 round(), but since we know that 0 <= level <= 100, it
  // should be ok.
  return static_cast<int>(level + 0.5);
}

int TrayAudioDelegateWin::GetActiveOutputDeviceIconId() {
  return kNoAudioDeviceIcon;
}

bool TrayAudioDelegateWin::HasAlternativeSources() {
  return false;
}

bool TrayAudioDelegateWin::IsOutputAudioMuted() {
  ScopedComPtr<ISimpleAudioVolume> volume_control =
      CreateDefaultVolumeControl();

  if (!volume_control)
    return false;

  BOOL mute = FALSE;
  if (FAILED(volume_control->GetMute(&mute)))
    return false;

  return !!mute;
}

void TrayAudioDelegateWin::SetOutputAudioIsMuted(bool is_muted) {
  ScopedComPtr<ISimpleAudioVolume> volume_control =
      CreateDefaultVolumeControl();

  if (!volume_control)
    return;

  volume_control->SetMute(is_muted, NULL);
}

void TrayAudioDelegateWin::SetOutputVolumeLevel(int level) {
  ScopedComPtr<ISimpleAudioVolume> volume_control =
      CreateDefaultVolumeControl();

  if (!volume_control)
    return;

  float volume_level = static_cast<float>(level) / 100.0f;
  volume_control->SetMasterVolume(volume_level, NULL);
}

ScopedComPtr<ISimpleAudioVolume>
TrayAudioDelegateWin::CreateDefaultVolumeControl() {
  ScopedComPtr<ISimpleAudioVolume> volume_control;
  ScopedComPtr<IAudioSessionManager> session_manager;

  ScopedComPtr<IMMDevice> device =
      media::CoreAudioUtil::CreateDefaultDevice(eRender, eConsole);
  if (!device ||
      FAILED(device->Activate(__uuidof(IAudioSessionManager), CLSCTX_ALL, NULL,
             session_manager.ReceiveVoid()))) {
    return volume_control;
  }

  session_manager->GetSimpleAudioVolume(NULL, FALSE,
      volume_control.Receive());

  return volume_control;
}

}  // namespace system
}  // namespace ash
