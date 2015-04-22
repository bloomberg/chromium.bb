// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HOST_PAIRING_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HOST_PAIRING_SCREEN_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/host_pairing_screen_actor.h"
#include "components/login/screens/screen_context.h"
#include "components/pairing/host_pairing_controller.h"

namespace chromeos {

class HostPairingScreen
    : public BaseScreen,
      public pairing_chromeos::HostPairingController::Observer,
      public HostPairingScreenActor::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when a configuration has been received, and should be applied to
    // this device.
    virtual void ConfigureHostRequested(bool accepted_eula,
                                        const std::string& lang,
                                        const std::string& timezone,
                                        bool send_reports,
                                        const std::string& keyboard_layout) = 0;

    // Called when a network configuration has been received, and should be
    // used on this device.
    virtual void AddNetworkRequested(const std::string& onc_spec) = 0;
  };

  HostPairingScreen(BaseScreenDelegate* base_screen_delegate,
                    Delegate* delegate,
                    HostPairingScreenActor* actor,
                    pairing_chromeos::HostPairingController* remora_controller);
  ~HostPairingScreen() override;

 private:
  typedef pairing_chromeos::HostPairingController::Stage Stage;

  void CommitContextChanges();

  // Overridden from BaseScreen:
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  std::string GetName() const override;

  // pairing_chromeos::HostPairingController::Observer:
  void PairingStageChanged(Stage new_stage) override;
  void ConfigureHostRequested(bool accepted_eula,
                              const std::string& lang,
                              const std::string& timezone,
                              bool send_reports,
                              const std::string& keyboard_layout) override;
  void AddNetworkRequested(const std::string& onc_spec) override;
  void EnrollHostRequested(const std::string& auth_token) override;

  // Overridden from ControllerPairingView::Delegate:
  void OnActorDestroyed(HostPairingScreenActor* actor) override;

  Delegate* delegate_;

  HostPairingScreenActor* actor_;

  // Controller performing pairing. Owned by the wizard controller.
  pairing_chromeos::HostPairingController* remora_controller_;

  // Current stage of pairing process.
  Stage current_stage_;

  DISALLOW_COPY_AND_ASSIGN(HostPairingScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HOST_PAIRING_SCREEN_H_
