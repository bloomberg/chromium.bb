// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_WIN_AUDIO_TRAY_AUDIO_DELEGATE_WIN_H_
#define ASH_SYSTEM_WIN_AUDIO_TRAY_AUDIO_DELEGATE_WIN_H_

#include <audioclient.h>
#include <mmdeviceapi.h>

#include "ash/ash_export.h"
#include "ash/system/audio/tray_audio_delegate.h"
#include "base/compiler_specific.h"
#include "base/win/scoped_comptr.h"

namespace ash {
namespace system {

class ASH_EXPORT TrayAudioDelegateWin : public TrayAudioDelegate {
 public:
  ~TrayAudioDelegateWin() override {}

  // Overridden from TrayAudioDelegate.
  void AdjustOutputVolumeToAudibleLevel() override;
  int GetOutputDefaultVolumeMuteLevel() override;
  int GetOutputVolumeLevel() override;
  int GetActiveOutputDeviceIconId() override;
  bool HasAlternativeSources() override;
  bool IsOutputAudioMuted() override;
  void SetOutputAudioIsMuted(bool is_muted) override;
  void SetOutputVolumeLevel(int level) override;
  void SetInternalSpeakerChannelMode(AudioChannelMode mode) override;
  void SetActiveHDMIOutoutRediscoveringIfNecessary(
      bool force_rediscovering) override;

 private:
  base::win::ScopedComPtr<ISimpleAudioVolume> CreateDefaultVolumeControl();
};

}  // namespace system
}  // namespace ash

#endif  // ASH_SYSTEM_WIN_AUDIO_TRAY_AUDIO_DELEGATE_WIN_H_