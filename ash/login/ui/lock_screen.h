// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_SCREEN_H_
#define ASH_LOGIN_UI_LOCK_SCREEN_H_

#include <unordered_map>

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"

namespace ui {
class Layer;
}

namespace ash {

class LockWindow;
class LoginDataDispatcher;
class TrayAction;

class ASH_EXPORT LockScreen : public TrayActionObserver,
                              public SessionObserver {
 public:
  // The UI that this instance is displaying.
  enum class ScreenType { kLogin, kLock };

  // Fetch the global lock screen instance. |Show()| must have been called
  // before this.
  static LockScreen* Get();

  // Creates and displays the lock screen. The lock screen communicates with the
  // backend C++ via a mojo API.
  static void Show(ScreenType type);

  // Check if the lock screen is currently shown.
  static bool IsShown();

  LockWindow* window() { return window_; }

  // Destroys an existing lock screen instance.
  void Destroy();

  // Enables/disables background blur. Used for debugging purpose.
  void ToggleBlurForDebug();

  // Returns the active data dispatcher.
  LoginDataDispatcher* data_dispatcher();

  // TrayActionObserver:
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnLockStateChanged(bool locked) override;

 private:
  explicit LockScreen(ScreenType type);
  ~LockScreen() override;

  // The type of screen shown. Controls how the screen is dismissed.
  const ScreenType type_;

  // Unowned pointer to the window which hosts the lock screen.
  LockWindow* window_ = nullptr;

  // The wallpaper bluriness before entering lock_screen.
  std::unordered_map<ui::Layer*, float> initial_blur_;

  ScopedObserver<TrayAction, TrayActionObserver> tray_action_observer_;
  ScopedSessionObserver session_observer_;

  DISALLOW_COPY_AND_ASSIGN(LockScreen);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_SCREEN_H_
