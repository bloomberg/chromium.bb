// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/ash_system_tray_delegate.h"

#include "ash/system/tray/system_tray_delegate.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/audio/audio_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

namespace chromeos {

namespace {

class SystemTrayDelegate : public ash::SystemTrayDelegate {
 public:
  SystemTrayDelegate() {}
  virtual ~SystemTrayDelegate() {}

  virtual void ShowSettings() OVERRIDE {
    BrowserList::GetLastActive()->OpenOptionsDialog();
  }

  virtual void ShowHelp() OVERRIDE {
    BrowserList::GetLastActive()->ShowHelpTab();
  }

  virtual bool AudioMuted() OVERRIDE {
    return AudioHandler::GetInstance()->IsMuted();
  }

  virtual void SetAudioMuted(bool muted) OVERRIDE {
    return AudioHandler::GetInstance()->SetMuted(muted);
  }

  virtual float VolumeLevel() OVERRIDE {
    return AudioHandler::GetInstance()->GetVolumePercent() / 100.;
  }

  virtual void SetVolumeLevel(float level) OVERRIDE {
    AudioHandler::GetInstance()->SetVolumePercent(level * 100.);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegate);
};

}  // namespace

ash::SystemTrayDelegate* CreateSystemTrayDelegate() {
  return new chromeos::SystemTrayDelegate();
}

}  // namespace chromeos
