// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_TRAY_AUDIO_DELEGATE_H_
#define ASH_SYSTEM_AUDIO_TRAY_AUDIO_DELEGATE_H_

namespace ash {
namespace system {

class ASH_EXPORT TrayAudioDelegate {
 public:

  enum { kNoAudioDeviceIcon = -1 };

  virtual ~TrayAudioDelegate() {}

  // Sets the volume level of the output device to the minimum level which is
  // deemed to be audible.
  virtual void AdjustOutputVolumeToAudibleLevel() = 0;

  // Gets the default level in the range 0%-100% at which the output device
  // should be muted.
  virtual int GetOutputDefaultVolumeMuteLevel() = 0;

  // Gets the icon to use for the active output device.
  virtual int GetActiveOutputDeviceIconId() = 0;

  // Returns the volume level of the output device in the range 0%-100%.
  virtual int GetOutputVolumeLevel() = 0;

  // Returns true if the device has alternative inputs or outputs.
  virtual bool HasAlternativeSources() = 0;

  // Returns whether the output volume is muted.
  virtual bool IsOutputAudioMuted() = 0;

  // Sets the mute state of the output device.
  virtual void SetOutputAudioIsMuted(bool is_muted) = 0;

  // Sets the volume level of the output device in the range 0%-100%
  virtual void SetOutputVolumeLevel(int level) = 0;
};

}  // namespace system
}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_TRAY_AUDIO_DELEGATE_H_
