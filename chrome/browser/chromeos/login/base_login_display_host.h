// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_BASE_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_BASE_LOGIN_DISPLAY_HOST_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/rect.h"

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
                             public content::NotificationObserver {
 public:
  explicit BaseLoginDisplayHost(const gfx::Rect& background_bounds);
  virtual ~BaseLoginDisplayHost();

  // Returns the default LoginDispalyHost instance if it has been created.
  static LoginDisplayHost* default_host() {
    return default_host_;
  }

  // LoginDisplayHost implementation:
  virtual void BeforeSessionStart() OVERRIDE;
  virtual void OnSessionStart() OVERRIDE;
  virtual void OnCompleteLogin() OVERRIDE;
  virtual void StartWizard(
      const std::string& first_screen_name,
      DictionaryValue* screen_parameters) OVERRIDE;
  virtual void StartSignInScreen() OVERRIDE;
  virtual void ResumeSignInScreen() OVERRIDE;
  virtual void CheckForAutoEnrollment() OVERRIDE;
  virtual WizardController* GetWizardController() OVERRIDE;

  // Creates specific WizardController.
  virtual WizardController* CreateWizardController() = 0;

  // Called when the first browser window is created, but before it's
  // ready (shown).
  virtual void OnBrowserCreated() = 0;

  const gfx::Rect& background_bounds() const { return background_bounds_; }

 protected:
  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Marks display host for deletion.
  // If |post_quit_task| is true also posts Quit task to the MessageLoop.
  void ShutdownDisplayHost(bool post_quit_task);

  // Start sign in transition animation.
  void StartAnimation();

  // Callback for the ownership status check.
  void OnOwnershipStatusCheckDone(DeviceSettingsService::OwnershipStatus status,
                                  bool current_user_is_owner);

  // Callback for completion of the |auto_enrollment_client_|.
  void OnAutoEnrollmentClientDone();

  // Forces auto-enrollment on the appropriate controller.
  void ForceAutoEnrollment();

  // Used to calculate position of the screens and background.
  gfx::Rect background_bounds_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<BaseLoginDisplayHost> pointer_factory_;

  // Default LoginDisplayHost.
  static LoginDisplayHost* default_host_;

  // Sign in screen controller.
  scoped_ptr<ExistingUserController> sign_in_controller_;

  // OOBE and some screens (camera, recovery) controller.
  scoped_ptr<WizardController> wizard_controller_;

  // Client for enterprise auto-enrollment check.
  scoped_ptr<policy::AutoEnrollmentClient> auto_enrollment_client_;

  // Has ShutdownDisplayHost() already been called?  Used to avoid posting our
  // own deletion to the message loop twice if the user logs out while we're
  // still in the process of cleaning up after login (http://crbug.com/134463).
  bool shutting_down_;

  // Whether progress bar is shown on the OOBE page.
  bool oobe_progress_bar_visible_;

  // True if session start is in progress.
  bool session_starting_;

  DISALLOW_COPY_AND_ASSIGN(BaseLoginDisplayHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_BASE_LOGIN_DISPLAY_HOST_H_
