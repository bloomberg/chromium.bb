// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_CONTROLLER_PAIRING_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_CONTROLLER_PAIRING_SCREEN_H_

#include "base/macros.h"

#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/controller_pairing_screen_actor.h"
#include "components/login/screens/screen_context.h"
#include "components/pairing/controller_pairing_controller.h"

namespace chromeos {

class ControllerPairingScreen
    : public BaseScreen,
      public pairing_chromeos::ControllerPairingController::Observer,
      public ControllerPairingScreenActor::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Set remora configuration from shark.
    virtual void SetHostConfiguration() = 0;
  };

  ControllerPairingScreen(
      BaseScreenDelegate* base_screen_delegate,
      Delegate* delegate,
      ControllerPairingScreenActor* actor,
      pairing_chromeos::ControllerPairingController* shark_controller);
  virtual ~ControllerPairingScreen();

 private:
  typedef pairing_chromeos::ControllerPairingController::Stage Stage;

  void CommitContextChanges();
  bool ExpectStageIs(Stage stage) const;

  // Overridden from BaseScreen:
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual std::string GetName() const override;

  // Overridden from pairing_chromeos::ControllerPairingController::Observer:
  virtual void PairingStageChanged(Stage new_stage) override;
  virtual void DiscoveredDevicesListChanged() override;

  // Overridden from ControllerPairingView::Delegate:
  virtual void OnActorDestroyed(ControllerPairingScreenActor* actor) override;
  virtual void OnScreenContextChanged(
      const base::DictionaryValue& diff) override;
  virtual void OnUserActed(const std::string& action) override;

  Delegate* delegate_;

  ControllerPairingScreenActor* actor_;

  // Controller performing pairing. Owned by the wizard controller.
  pairing_chromeos::ControllerPairingController* shark_controller_;

  // Current stage of pairing process.
  Stage current_stage_;

  // If this one is |false| first device in device list will be preselected on
  // next device list update.
  bool device_preselected_;

  DISALLOW_COPY_AND_ASSIGN(ControllerPairingScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_CONTROLLER_PAIRING_SCREEN_H_
