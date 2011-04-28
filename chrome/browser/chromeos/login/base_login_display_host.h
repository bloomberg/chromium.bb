// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_BASE_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_BASE_LOGIN_DISPLAY_HOST_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/gfx/rect.h"

class WizardController;

namespace views {
class Widget;
}

namespace chromeos {

class ExistingUserController;

// An abstract base class that defines OOBE/login screen host.
// It encapsulates controllers, background integration and flow.
class BaseLoginDisplayHost : public LoginDisplayHost,
                             public NotificationObserver {
 public:
  explicit BaseLoginDisplayHost(const gfx::Rect& background_bounds);
  virtual ~BaseLoginDisplayHost();

  // Returns the default LoginDispalyHost instance if it has been created.
  static LoginDisplayHost* default_host() {
    return default_host_;
  }

  // LoginDisplayHost implementation:
  virtual void OnSessionStart();
  virtual void StartWizard(
      const std::string& first_screen_name,
      const GURL& start_url);
  virtual void StartSignInScreen();

  const gfx::Rect& background_bounds() const { return background_bounds_; }

 private:
  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Used to calculate position of the screens and background.
  gfx::Rect background_bounds_;

  NotificationRegistrar registrar_;

  // Default LoginDisplayHost.
  static LoginDisplayHost* default_host_;

  // Sign in screen controller.
  scoped_ptr<ExistingUserController> sign_in_controller_;

  // OOBE and some screens (camera, recovery) controller.
  scoped_ptr<WizardController> wizard_controller_;

  DISALLOW_COPY_AND_ASSIGN(BaseLoginDisplayHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_BASE_LOGIN_DISPLAY_HOST_H_
