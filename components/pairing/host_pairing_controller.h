// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAIRING_HOST_PAIRING_CONTROLLER_H_
#define COMPONENTS_PAIRING_HOST_PAIRING_CONTROLLER_H_

#include <string>

#include "base/macros.h"

namespace pairing_chromeos {

class HostPairingController {
 public:
  enum Stage {
    STAGE_NONE,
    STAGE_INITIALIZATION_ERROR,
    STAGE_WAITING_FOR_CONTROLLER,
    STAGE_WAITING_FOR_CODE_CONFIRMATION,
    STAGE_UPDATING,
    STAGE_WAITING_FOR_CONTROLLER_AFTER_UPDATE,
    STAGE_WAITING_FOR_CREDENTIALS,
    STAGE_ENROLLING,
    STAGE_ENROLLMENT_ERROR,
    STAGE_PAIRING_DONE,
    STAGE_FINISHED
  };

  enum UpdateStatus {
    UPDATE_STATUS_UNKNOWN,
    UPDATE_STATUS_UPDATING,
    UPDATE_STATUS_REBOOTING,
    UPDATE_STATUS_UPDATED,
  };

  class Observer {
   public:
    Observer();
    virtual ~Observer();

    // Called when pairing has moved on from one stage to another.
    virtual void PairingStageChanged(Stage new_stage) = 0;

    // Called when the controller has sent a configuration to apply.
    virtual void ConfigureHost(bool accepted_eula,
                               const std::string& lang,
                               const std::string& timezone,
                               bool send_reports,
                               const std::string& keyboard_layout) = 0;

    // Called when the controller has provided an |auth_token| for enrollment.
    virtual void EnrollHost(const std::string& auth_token) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  HostPairingController();
  virtual ~HostPairingController();

  // Returns current stage of pairing process.
  virtual Stage GetCurrentStage() = 0;

  // Starts pairing process. Can be called only on |STAGE_NONE| stage.
  virtual void StartPairing() = 0;

  // Returns device name.
  virtual std::string GetDeviceName() = 0;

  // Returns 6-digit confirmation code. Can be called only on
  // |STAGE_WAITING_FOR_CODE_CONFIRMATION| stage.
  virtual std::string GetConfirmationCode() = 0;

  // Returns an enrollment domain name. Can be called on stage
  // |STAGE_ENROLLMENT| and later.
  virtual std::string GetEnrollmentDomain() = 0;

  // Notify that the update status has changed.
  // Can be called on stage |STAGE_UPDATING|.
  virtual void OnUpdateStatusChanged(UpdateStatus update_status) = 0;

  // Called when enrollment has completed.
  virtual void SetEnrollmentComplete(bool success) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostPairingController);
};

}  // namespace pairing_chromeos

#endif  // COMPONENTS_PAIRING_HOST_PAIRING_CONTROLLER_H_
