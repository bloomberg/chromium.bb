// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAIRING_FAKE_HOST_PAIRING_CONTROLLER_H_
#define COMPONENTS_PAIRING_FAKE_HOST_PAIRING_CONTROLLER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/pairing/host_pairing_controller.h"

namespace pairing_chromeos {

class FakeHostPairingController
    : public HostPairingController,
      public HostPairingController::Observer {
 public:
  typedef HostPairingController::Observer Observer;

  // Config is a comma separated list of key-value pairs separated by colon.
  // Supported options:
  // * async_duration - integer. Default: 3000.
  // * start_after_update - {0,1}. Default: 0.
  // * fail_enrollment - {0,1}. Default: 0.
  // * code - 6 digits or empty string. Default: empty string. If strings is
  // empty, random code is generated.
  // * device_name - string. Default: "Chromebox-01".
  // * domain - string. Default: "example.com".
  explicit FakeHostPairingController(const std::string& config);
  virtual ~FakeHostPairingController();

  // Applies given |config| to flow.
  void ApplyConfig(const std::string& config);

  // Overridden from HostPairingFlow:
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual Stage GetCurrentStage() OVERRIDE;
  virtual void StartPairing() OVERRIDE;
  virtual std::string GetDeviceName() OVERRIDE;
  virtual std::string GetConfirmationCode() OVERRIDE;
  virtual std::string GetEnrollmentDomain() OVERRIDE;

 private:
  void ChangeStage(Stage new_stage);
  void ChangeStageLater(Stage new_stage);
  void SetUpdateProgress(int step);

  // Overridden from HostPairingFlow::Observer:
  virtual void PairingStageChanged(Stage new_stage) OVERRIDE;
  virtual void UpdateAdvanced(const UpdateProgress& progress) OVERRIDE;

  ObserverList<Observer> observers_;
  Stage current_stage_;
  std::string device_name_;
  std::string confirmation_code_;
  base::TimeDelta async_duration_;

  // If this flag is true error happens on |STAGE_ENROLLING| once.
  bool enrollment_should_fail_;

  // Controller starts its work like if update and reboot already happened.
  bool start_after_update_;

  std::string enrollment_domain_;

  DISALLOW_COPY_AND_ASSIGN(FakeHostPairingController);
};

}  // namespace pairing_chromeos

#endif  // COMPONENTS_PAIRING_FAKE_HOST_PAIRING_CONTROLLER_H_
