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
  virtual ~TrayAudioDelegateWin() {}

  // Overridden from TrayAudioDelegate.
  virtual void AdjustOutputVolumeToAudibleLevel() override;
  virtual int GetOutputDefaultVolumeMuteLevel() override;
  virtual int GetOutputVolumeLevel() override;
  virtual int GetActiveOutputDeviceIconId() override;
  virtual bool HasAlternativeSources() override;
  virtual bool IsOutputAudioMuted() override;
  virtual void SetOutputAudioIsMuted(bool is_muted) override;
  virtual void SetOutputVolumeLevel(int level) override;
  virtual void SetInternalSpeakerChannelMode(AudioChannelMode mode) override;

 private:
  base::win::ScopedComPtr<ISimpleAudioVolume> CreateDefaultVolumeControl();
};

}  // namespace system
}  // namespace ash

#endif  // ASH_SYSTEM_WIN_AUDIO_TRAY_AUDIO_DELEGATE_WIN_H_