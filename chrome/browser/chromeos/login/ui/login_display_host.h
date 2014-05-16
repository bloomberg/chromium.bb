// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_H_

#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "ui/gfx/native_widget_types.h"

namespace views {
class Widget;
}  // namespace views

namespace chromeos {

class AppLaunchController;
class AutoEnrollmentController;
class LoginScreenContext;
class WebUILoginView;
class WizardController;

// An interface that defines OOBE/login screen host.
// Host encapsulates WebUI window OOBE/login controllers,
// UI implementation (such as LoginDisplay).
class LoginDisplayHost {
 public:
  virtual ~LoginDisplayHost() {}

  // Creates UI implementation specific login display instance (views/WebUI).
  // The caller takes ownership of the returned value.
  virtual LoginDisplay* CreateLoginDisplay(
      LoginDisplay::Delegate* delegate) = 0;

  // Returns corresponding native window.
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Returns the current login view.
  virtual WebUILoginView* GetWebUILoginView() const = 0;

  // Called when browsing session starts before creating initial browser.
  virtual void BeforeSessionStart() = 0;

  // Called when user enters or returns to browsing session so
  // LoginDisplayHost instance may delete itself.
  virtual void Finalize() = 0;

  // Called when a login has completed successfully.
  virtual void OnCompleteLogin() = 0;

  // Open proxy settings dialog.
  virtual void OpenProxySettings() = 0;

  // Toggles status area visibility.
  virtual void SetStatusAreaVisible(bool visible) = 0;

  // Gets the auto-enrollment client.
  virtual AutoEnrollmentController* GetAutoEnrollmentController() = 0;

  // Starts out-of-box-experience flow or shows other screen handled by
  // Wizard controller i.e. camera, recovery.
  // One could specify start screen with |first_screen_name|.
  // Takes ownership of |screen_parameters|, which can also be NULL.
  virtual void StartWizard(
      const std::string& first_screen_name,
      scoped_ptr<base::DictionaryValue> screen_parameters) = 0;

  // Returns current WizardController, if it exists.
  // Result should not be stored.
  virtual WizardController* GetWizardController() = 0;

  // Returns current AppLaunchController, if it exists.
  // Result should not be stored.
  virtual AppLaunchController* GetAppLaunchController() = 0;

  // Starts screen for adding user into session.
  // |completion_callback| called before display host shutdown.
  // |completion_callback| can be null.
  virtual void StartUserAdding(const base::Closure& completion_callback) = 0;

  // Starts sign in screen.
  virtual void StartSignInScreen(const LoginScreenContext& context) = 0;

  // Resumes a previously started sign in screen.
  virtual void ResumeSignInScreen() = 0;

  // Invoked when system preferences that affect the signin screen have changed.
  virtual void OnPreferencesChanged() = 0;

  // Initiates authentication network prewarming.
  virtual void PrewarmAuthentication() = 0;

  // Starts app launch splash screen.
  virtual void StartAppLaunch(const std::string& app_id,
                              bool diagnostic_mode) = 0;

  // Starts the demo app launch.
  virtual void StartDemoAppLaunch() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_H_
