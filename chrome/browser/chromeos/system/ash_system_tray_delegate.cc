// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/ash_system_tray_delegate.h"

#include "ash/shell.h"
#include "ash/system/audio/audio_controller.h"
#include "ash/system/brightness/brightness_controller.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/audio/audio_handler.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

namespace chromeos {

namespace {

class SystemTrayDelegate : public ash::SystemTrayDelegate,
                           public AudioHandler::VolumeObserver,
                           public PowerManagerClient::Observer {
 public:
  explicit SystemTrayDelegate(ash::SystemTray* tray) : tray_(tray) {
    AudioHandler::GetInstance()->AddVolumeObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  }

  virtual ~SystemTrayDelegate() {
    AudioHandler* audiohandler = AudioHandler::GetInstance();
    if (audiohandler)
      audiohandler->RemoveVolumeObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  }

  // Overridden from ash::SystemTrayDelegate.
  virtual const std::string GetUserDisplayName() OVERRIDE {
    return UserManager::Get()->logged_in_user().GetDisplayName();
  }

  virtual const std::string GetUserEmail() OVERRIDE {
    return UserManager::Get()->logged_in_user().email();
  }

  virtual const SkBitmap& GetUserImage() OVERRIDE {
    return UserManager::Get()->logged_in_user().image();
  }

  virtual void ShowSettings() OVERRIDE {
    BrowserList::GetLastActive()->OpenOptionsDialog();
  }

  virtual void ShowHelp() OVERRIDE {
    BrowserList::GetLastActive()->ShowHelpTab();
  }

  virtual bool IsAudioMuted() const OVERRIDE {
    return AudioHandler::GetInstance()->IsMuted();
  }

  virtual void SetAudioMuted(bool muted) OVERRIDE {
    return AudioHandler::GetInstance()->SetMuted(muted);
  }

  virtual float GetVolumeLevel() const OVERRIDE {
    return AudioHandler::GetInstance()->GetVolumePercent() / 100.f;
  }

  virtual void SetVolumeLevel(float level) OVERRIDE {
    AudioHandler::GetInstance()->SetVolumePercent(level * 100.f);
  }

  virtual void ShutDown() OVERRIDE {
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestShutdown();
  }

  virtual void SignOut() OVERRIDE {
    BrowserList::AttemptUserExit();
  }

  virtual void LockScreen() OVERRIDE {
    DBusThreadManager::Get()->GetPowerManagerClient()->
        NotifyScreenLockRequested();
  }

 private:
  ash::SystemTray* tray_;

  // Overridden from AudioHandler::VolumeObserver.
  virtual void OnVolumeChanged() OVERRIDE {
    float level = AudioHandler::GetInstance()->GetVolumePercent() / 100.f;
    ash::Shell::GetInstance()->audio_controller()->
        OnVolumeChanged(level);
  }

  // Overridden from PowerManagerClient::Observer.
  virtual void BrightnessChanged(int level, bool user_initiated) OVERRIDE {
    ash::Shell::GetInstance()->brightness_controller()->
        OnBrightnessChanged(level / 100.f, user_initiated);
  }

  // TODO(sad): Override more from PowerManagerClient::Observer here (e.g.
  // PowerChanged, PowerButtonStateChanged etc.).

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegate);
};

}  // namespace

ash::SystemTrayDelegate* CreateSystemTrayDelegate(ash::SystemTray* tray) {
  return new chromeos::SystemTrayDelegate(tray);
}

}  // namespace chromeos
