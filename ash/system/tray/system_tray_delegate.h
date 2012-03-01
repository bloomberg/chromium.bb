// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_
#pragma once

namespace ash {

class SystemTrayDelegate {
 public:
  virtual ~SystemTrayDelegate() {}

  // Shows settings.
  virtual void ShowSettings() = 0;

  // Shows help.
  virtual void ShowHelp() = 0;

  // Is the system audio muted?
  virtual bool IsAudioMuted() const = 0;

  // Mutes/Unmutes the audio system.
  virtual void SetAudioMuted(bool muted) = 0;

  // Gets the volume level.
  virtual float GetVolumeLevel() const = 0;

  // Sets the volume level.
  virtual void SetVolumeLevel(float level) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_
