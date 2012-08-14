// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_VOLUME_CONTROL_DELEGATE_H_
#define ASH_VOLUME_CONTROL_DELEGATE_H_

namespace ui {
class Accelerator;
}  // namespace ui

namespace ash {

// Delegate for controlling the volume.
class VolumeControlDelegate {
 public:
  virtual ~VolumeControlDelegate() {}

  virtual bool HandleVolumeMute(const ui::Accelerator& accelerator) = 0;
  virtual bool HandleVolumeDown(const ui::Accelerator& accelerator) = 0;
  virtual bool HandleVolumeUp(const ui::Accelerator& accelerator) = 0;

  // Is the system audio muted?
  virtual bool IsAudioMuted() const = 0;

  // Mutes/Unmutes the audio system.
  virtual void SetAudioMuted(bool muted) = 0;

  // Gets the volume level. The range is [0, 1.0].
  virtual float GetVolumeLevel() const = 0;

  // Sets the volume level. The range is [0, 1.0].
  virtual void SetVolumeLevel(float level) = 0;

  // Requests that the volume be set to |percent|, in the range
  // [0.0, 100.0].
  virtual void SetVolumePercent(double percent) = 0;
};

}  // namespace ash

#endif  // ASH_VOLUME_CONTROL_DELEGATE_H_
