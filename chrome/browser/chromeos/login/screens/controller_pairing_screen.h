// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_CONTROLLER_PAIRING_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_CONTROLLER_PAIRING_SCREEN_H_

#include "base/macros.h"

#include "chrome/browser/chromeos/login/screens/controller_pairing_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/screen_context.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"
#include "components/pairing/controller_pairing_controller.h"

namespace chromeos {

class ControllerPairingScreen :
  public WizardScreen,
  public pairing_chromeos::ControllerPairingController::Observer,
  public ControllerPairingScreenActor::Delegate {
 public:
  ControllerPairingScreen(ScreenObserver* observer,
                          ControllerPairingScreenActor* actor);
  virtual ~ControllerPairingScreen();

 private:
  typedef pairing_chromeos::ControllerPairingController::Stage Stage;

  void CommitContextChanges();
  bool ExpectStageIs(Stage stage) const;

  // Overridden from WizardScreen:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  // Overridden from pairing_chromeos::ControllerPairingController::Observer:
  virtual void PairingStageChanged(Stage new_stage) OVERRIDE;
  virtual void DiscoveredDevicesListChanged() OVERRIDE;

  // Overridden from ControllerPairingView::Delegate:
  virtual void OnActorDestroyed(ControllerPairingScreenActor* actor) OVERRIDE;
  virtual void OnScreenContextChanged(
      const base::DictionaryValue& diff) OVERRIDE;
  virtual void OnUserActed(const std::string& action) OVERRIDE;

  // Context for sharing data between C++ and JS.
  // TODO(dzhioev): move to BaseScreen when possible.
  ScreenContext context_;

  ControllerPairingScreenActor* actor_;

  // Controller performing pairing. Owned by the screen for now.
  // TODO(dzhioev): move to proper place later.
  scoped_ptr<pairing_chromeos::ControllerPairingController> controller_;

  // Current stage of pairing process.
  Stage current_stage_;

  // If this one is |false| first device in device list will be preselected on
  // next device list update.
  bool device_preselected_;

  DISALLOW_COPY_AND_ASSIGN(ControllerPairingScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_CONTROLLER_PAIRING_SCREEN_H_
