// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_DISPLAY_HOST_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

// An interface that defines OOBE/login screen host.
// Host encapsulates implementation specific background window (views/WebUI),
// OOBE/login controllers, views/WebUI UI implementation (such as LoginDisplay).
class LoginDisplayHost {
 public:
  virtual ~LoginDisplayHost() {}

  // Creates UI implementation specific login display instance (views/WebUI).
  virtual LoginDisplay* CreateLoginDisplay(
      LoginDisplay::Delegate* delegate) const = 0;

  // Returns corresponding native window.
  // TODO(nkostylev): Might be refactored, move to views-specific code.
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Called when browsing session starts so
  // LoginDisplayHost instance may delete itself.
  virtual void OnSessionStart() = 0;

  // TODO(nkostylev): Refactor enum.
  // Sets current step on OOBE progress bar.
  virtual void SetOobeProgress(BackgroundView::LoginStep step) = 0;

  // Toggles OOBE progress bar visibility, the bar is hidden by default.
  virtual void SetOobeProgressBarVisible(bool visible) = 0;

  // Enable/disable shutdown button.
  virtual void SetShutdownButtonEnabled(bool enable) = 0;

  // Toggles whether status area is enabled.
  virtual void SetStatusAreaEnabled(bool enable) = 0;

  // Toggles status area visibility.
  virtual void SetStatusAreaVisible(bool visible) = 0;

  // Creates and shows a background window.
  virtual void ShowBackground() = 0;

  // Starts out-of-box-experience flow or shows other screen handled by
  // Wizard controller i.e. camera, recovery.
  // One could specify start screen with |first_screen_name|.
  virtual void StartWizard(
      const std::string& first_screen_name,
      const GURL& start_url) = 0;

  // Starts sign in screen.
  virtual void StartSignInScreen() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_DISPLAY_HOST_H_
