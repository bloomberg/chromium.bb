// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_BASE_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_BASE_LOGIN_DISPLAY_HOST_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/ownership_status_checker.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/rect.h"

#if defined(USE_AURA)
#include "ui/gfx/compositor/layer_animation_observer.h"
#endif

class PrefService;

namespace policy {
class AutoEnrollmentClient;
}  // namespace policy

namespace chromeos {

class ExistingUserController;
class WizardController;

// An abstract base class that defines OOBE/login screen host.
// It encapsulates controllers, background integration and flow.
class BaseLoginDisplayHost : public LoginDisplayHost,
#if defined(USE_AURA)
                             public ui::LayerAnimationObserver,
#endif
                             public content::NotificationObserver {
 public:
  explicit BaseLoginDisplayHost(const gfx::Rect& background_bounds);
  virtual ~BaseLoginDisplayHost();

  // Returns the default LoginDispalyHost instance if it has been created.
  static LoginDisplayHost* default_host() {
    return default_host_;
  }

  // Registers preferences in local state.
  static void RegisterPrefs(PrefService* local_state);

  // LoginDisplayHost implementation:
  virtual void OnSessionStart() OVERRIDE;
  virtual void StartWizard(
      const std::string& first_screen_name,
      DictionaryValue* screen_parameters) OVERRIDE;
  virtual void StartSignInScreen() OVERRIDE;
  virtual void ResumeSignInScreen() OVERRIDE;
  virtual void CheckForAutoEnrollment() OVERRIDE;

  // Creates specific WizardController.
  virtual WizardController* CreateWizardController() = 0;

  const gfx::Rect& background_bounds() const { return background_bounds_; }

 private:
#if defined(USE_AURA)
  // ui::LayerAnimationObserver overrides:
  virtual void OnLayerAnimationEnded(
      const ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      const ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      const ui::LayerAnimationSequence* sequence) OVERRIDE;
#endif

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Marks display host for deletion.
  // If |post_quit_task| is true also posts Quit task to the MessageLoop.
  void ShutdownDisplayHost(bool post_quit_task);

  // Start sign in transition animation.
  void StartAnimation();

  // Callback for completion of the |ownership_status_checker_|.
  void OnOwnershipStatusCheckDone(OwnershipService::Status status,
                                  bool current_user_is_owner);

  // Callback for completion of the |auto_enrollment_client_|.
  void OnAutoEnrollmentClientDone();

  // Forces auto-enrollment on the appropriate controller.
  void ForceAutoEnrollment();

  // Used to calculate position of the screens and background.
  gfx::Rect background_bounds_;

  content::NotificationRegistrar registrar_;

  // Default LoginDisplayHost.
  static LoginDisplayHost* default_host_;

  // Sign in screen controller.
  scoped_ptr<ExistingUserController> sign_in_controller_;

  // OOBE and some screens (camera, recovery) controller.
  scoped_ptr<WizardController> wizard_controller_;

  // Client for enterprise auto-enrollment check.
  scoped_ptr<policy::AutoEnrollmentClient> auto_enrollment_client_;

  // Used to verify if the device has already been owned.
  scoped_ptr<OwnershipStatusChecker> ownership_status_checker_;

  DISALLOW_COPY_AND_ASSIGN(BaseLoginDisplayHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_BASE_LOGIN_DISPLAY_HOST_H_
