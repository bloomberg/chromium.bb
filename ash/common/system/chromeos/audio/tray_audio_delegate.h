// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_AUDIO_TRAY_AUDIO_DELEGATE_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_AUDIO_TRAY_AUDIO_DELEGATE_H_

namespace gfx {
struct VectorIcon;
}

namespace ash {
namespace system {

class TrayAudioDelegate {
 public:
  enum { kNoAudioDeviceIcon = -1 };
  enum AudioChannelMode {
    NORMAL,
    LEFT_RIGHT_SWAPPED,
  };

  virtual ~TrayAudioDelegate() {}

  // Sets the volume level of the output device to the minimum level which is
  // deemed to be audible.
  virtual void AdjustOutputVolumeToAudibleLevel() = 0;

  // Gets the default level in the range 0%-100% at which the output device
  // should be muted.
  virtual int GetOutputDefaultVolumeMuteLevel() = 0;

  // Gets the MD icon to use for the active output device.
  virtual const gfx::VectorIcon& GetActiveOutputDeviceVectorIcon() = 0;

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

  // Sets the internal speaker's channel mode.
  virtual void SetInternalSpeakerChannelMode(AudioChannelMode mode) = 0;

  // If necessary, sets the starting point for re-discovering the active HDMI
  // output device caused by device entering/exiting docking mode, HDMI display
  // changing resolution, or chromeos device suspend/resume. If
  // |force_rediscovering| is true, it will force to set the starting point for
  // re-discovering the active HDMI output device again if it has been in the
  // middle of rediscovering the HDMI active output device.
  virtual void SetActiveHDMIOutoutRediscoveringIfNecessary(
      bool force_rediscovering) = 0;
};

}  // namespace system
}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_AUDIO_TRAY_AUDIO_DELEGATE_H_
