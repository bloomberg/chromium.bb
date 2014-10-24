// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/input/public/accelerator_manager.h"
#include "athena/input/public/input_manager.h"
#include "athena/screen_lock/screen_lock_manager_base.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"

namespace athena {
namespace {

enum Command {
  CMD_LOCK_SCREEN
};

const AcceleratorData accelerator_data[] = {
    { TRIGGER_ON_PRESS, ui::VKEY_L,  ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
      CMD_LOCK_SCREEN,  AF_NONE }};

class ChromeScreenLockManager : public ScreenLockManagerBase,
                                public AcceleratorHandler,
                                public PowerButtonObserver {
 public:
  ChromeScreenLockManager() {}

  void Init();

 protected:
  ~ChromeScreenLockManager() override;

 private:
  // ScreenLockManager:
  void LockScreen() override;

  // AcceleratorHandler:
  bool IsCommandEnabled(int command_id) const override;
  bool OnAcceleratorFired(int command_id,
                          const ui::Accelerator& accelerator) override;

  // PowerButtonObserver:
  void OnPowerButtonStateChanged(State state) override;

  DISALLOW_COPY_AND_ASSIGN(ChromeScreenLockManager);
};

ChromeScreenLockManager::~ChromeScreenLockManager() {
  InputManager::Get()->RemovePowerButtonObserver(this);
}

void ChromeScreenLockManager::Init() {
  AcceleratorManager::Get()->RegisterAccelerators(
      accelerator_data, arraysize(accelerator_data), this);
  InputManager::Get()->AddPowerButtonObserver(this);
}

void ChromeScreenLockManager::LockScreen() {
  chromeos::ScreenLocker::HandleLockScreenRequest();
}

bool ChromeScreenLockManager::IsCommandEnabled(int command_id) const {
  return true;
}

bool ChromeScreenLockManager::OnAcceleratorFired(
    int command_id,
    const ui::Accelerator& accelerator) {
  switch (command_id) {
    case CMD_LOCK_SCREEN:
      LockScreen();
      return true;

    default:
      NOTREACHED();
  }
  return false;
}

void ChromeScreenLockManager::OnPowerButtonStateChanged(State state) {
  if (state == PRESSED)
    LockScreen();
}

}  // namespace

// static
ScreenLockManager* ScreenLockManager::Create() {
  ChromeScreenLockManager* instance = new ChromeScreenLockManager();
  instance->Init();
  return instance;
}

}  // namespace athena
