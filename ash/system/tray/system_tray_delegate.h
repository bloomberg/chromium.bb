// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SYSTEM_DELEGATE_H_
#define ASH_SYSTEM_SYSTEM_DELEGATE_H_
#pragma once

namespace ash {

class SystemTrayDelegate {
 public:
  virtual ~SystemTrayDelegate(){}

  // Shows settings.
  virtual void ShowSettings() = 0;

  // Shows help.
  virtual void ShowHelp() = 0;

  // Is the system muted?
  virtual bool AudioMuted() = 0;

  // Mutes/Unmutes the system.
  virtual void SetAudioMuted(bool muted) = 0;

  // Gets volume level.
  virtual float VolumeLevel() = 0;

  // Sets the volume level.
  virtual void SetVolumeLevel(float level) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_SYSTEM_DELEGATE_H_
