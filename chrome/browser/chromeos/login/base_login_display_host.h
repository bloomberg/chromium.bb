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
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/rect.h"

#if defined(USE_AURA)
#include "ui/gfx/compositor/layer_animation_observer.h"
#endif

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

  // LoginDisplayHost implementation:
  virtual void OnSessionStart() OVERRIDE;
  virtual void StartWizard(
      const std::string& first_screen_name,
      const GURL& start_url) OVERRIDE;
  virtual void StartSignInScreen() OVERRIDE;

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

  // Used to calculate position of the screens and background.
  gfx::Rect background_bounds_;

  content::NotificationRegistrar registrar_;

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
