// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_LAUNCHER_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {

class Window;

namespace client {
class ActivationClient;
}
}

class AppWindowLauncherItemController;
class ChromeLauncherController;
class Profile;

class AppWindowLauncherController
    : public aura::client::ActivationChangeObserver {
 public:
  ~AppWindowLauncherController() override;

  // Called by ChromeLauncherController when the active user changed and the
  // items need to be updated.
  virtual void ActiveUserChanged(const std::string& user_email) {}

  // An additional user identified by |Profile|, got added to the existing
  // session.
  virtual void AdditionalUserAddedToSession(Profile* profile) {}

  // Overriden from client::ActivationChangeObserver:
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

 protected:
  explicit AppWindowLauncherController(ChromeLauncherController* owner);

  ChromeLauncherController* owner() { return owner_; }

  virtual AppWindowLauncherItemController* ControllerForWindow(
      aura::Window* window) = 0;

 private:
  // Unowned pointers.
  ChromeLauncherController* owner_;
  aura::client::ActivationClient* activation_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_LAUNCHER_CONTROLLER_H_
