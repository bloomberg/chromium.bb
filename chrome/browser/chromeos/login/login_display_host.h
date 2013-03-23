// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_DISPLAY_HOST_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "ui/gfx/native_widget_types.h"

namespace views {
class Widget;
}  // namespace views

namespace chromeos {

class WizardController;

// An interface that defines OOBE/login screen host.
// Host encapsulates implementation specific background window (views/WebUI),
// OOBE/login controllers, views/WebUI UI implementation (such as LoginDisplay).
class LoginDisplayHost {
 public:
  virtual ~LoginDisplayHost() {}

  // Creates UI implementation specific login display instance (views/WebUI).
  // The caller takes ownership of the returned value.
  virtual LoginDisplay* CreateLoginDisplay(
      LoginDisplay::Delegate* delegate) = 0;

  // Returns corresponding native window.
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Returns corresponding widget.
  virtual views::Widget* GetWidget() const = 0;

  // Called when browsing session starts before creating initial browser.
  virtual void BeforeSessionStart() = 0;

  // Called when browsing session starts so
  // LoginDisplayHost instance may delete itself.
  virtual void OnSessionStart() = 0;

  // Called when a login has completed successfully.
  virtual void OnCompleteLogin() = 0;

  // Open proxy settings dialog.
  virtual void OpenProxySettings() = 0;

  // Toggles OOBE progress bar visibility, the bar is hidden by default.
  virtual void SetOobeProgressBarVisible(bool visible) = 0;

  // Enable/disable shutdown button.
  virtual void SetShutdownButtonEnabled(bool enable) = 0;

  // Toggles status area visibility.
  virtual void SetStatusAreaVisible(bool visible) = 0;

  // Signals the LoginDisplayHost that it can proceed with the Enterprise
  // Auto-Enrollment checks now.
  virtual void CheckForAutoEnrollment() = 0;

  // Starts out-of-box-experience flow or shows other screen handled by
  // Wizard controller i.e. camera, recovery.
  // One could specify start screen with |first_screen_name|.
  // Takes ownership of |screen_parameters|, which can also be NULL.
  virtual void StartWizard(
      const std::string& first_screen_name,
      DictionaryValue* screen_parameters) = 0;

  // Returns current WizardController, if it exists.
  // Result should not be stored.
  virtual WizardController* GetWizardController() = 0;

  // Starts sign in screen.
  virtual void StartSignInScreen() = 0;

  // Resumes a previously started sign in screen.
  virtual void ResumeSignInScreen() = 0;

  // Invoked when system preferences that affect the signin screen have changed.
  virtual void OnPreferencesChanged() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_DISPLAY_HOST_H_
