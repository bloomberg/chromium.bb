// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_
#pragma once

#include "ash/system/user/login_status.h"
#include "base/string16.h"
#include "third_party/skia/include/core/SkBitmap.h"

class SkBitmap;

namespace ash {

struct NetworkIconInfo {
  SkBitmap image;
  string16 name;
  string16 description;
};

class SystemTrayDelegate {
 public:
  virtual ~SystemTrayDelegate() {}

  // Gets information about the logged in user.
  virtual const std::string GetUserDisplayName() const = 0;
  virtual const std::string GetUserEmail() const = 0;
  virtual const SkBitmap& GetUserImage() const = 0;
  virtual user::LoginStatus GetUserLoginStatus() const = 0;

  // Shows settings.
  virtual void ShowSettings() = 0;

  // Shows the settings related to date, timezone etc.
  virtual void ShowDateSettings() = 0;

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

  // Attempts to shut down the system.
  virtual void ShutDown() = 0;

  // Attempts to sign out the user.
  virtual void SignOut() = 0;

  // Attempts to lock the screen.
  virtual void RequestLockScreen() = 0;

  // Returns information about the most relevant network. Relevance is
  // determined by the implementor (e.g. a connecting network may be more
  // relevant over a connected network etc.)
  virtual NetworkIconInfo GetMostRelevantNetworkIcon() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_
