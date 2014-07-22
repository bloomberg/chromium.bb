// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HOST_PAIRING_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HOST_PAIRING_SCREEN_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/host_pairing_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/screen_context.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"
#include "components/pairing/host_pairing_controller.h"

namespace chromeos {

class HostPairingScreen :
  public WizardScreen,
  public pairing_chromeos::HostPairingController::Observer,
  public HostPairingScreenActor::Delegate {
 public:
  HostPairingScreen(ScreenObserver* observer, HostPairingScreenActor* actor);
  virtual ~HostPairingScreen();

 private:
  typedef pairing_chromeos::HostPairingController::Stage Stage;
  typedef pairing_chromeos::HostPairingController::UpdateProgress
      UpdateProgress;

  void CommitContextChanges();

  // Overridden from WizardScreen:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  // Overridden from pairing_chromeos::HostPairingController::Observer:
  virtual void PairingStageChanged(Stage new_stage) OVERRIDE;
  virtual void UpdateAdvanced(const UpdateProgress& progress) OVERRIDE;

  // Overridden from ControllerPairingView::Delegate:
  virtual void OnActorDestroyed(HostPairingScreenActor* actor) OVERRIDE;

  // Context for sharing data between C++ and JS.
  // TODO(dzhioev): move to BaseScreen when possible.
  ScreenContext context_;

  HostPairingScreenActor* actor_;

  // Controller performing pairing. Owned by the screen for now.
  // TODO(dzhioev): move to proper place later.
  scoped_ptr<pairing_chromeos::HostPairingController> controller_;

  // Current stage of pairing process.
  Stage current_stage_;

  DISALLOW_COPY_AND_ASSIGN(HostPairingScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HOST_PAIRING_SCREEN_H_
