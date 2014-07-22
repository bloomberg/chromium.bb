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

  struct UpdateProgress {
    // Number in [0, 1].
    double progress;
  };

  class Observer {
   public:
    Observer();
    virtual ~Observer();

    // Called when pairing has moved on from one stage to another.
    virtual void PairingStageChanged(Stage new_stage) = 0;

    // Called periodically on |STAGE_UPDATING| stage. Current update progress
    // is stored in |progress|.
    virtual void UpdateAdvanced(const UpdateProgress& progress) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  HostPairingController();
  virtual ~HostPairingController();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(HostPairingController);
};

}  // namespace pairing_chromeos

#endif  // COMPONENTS_PAIRING_HOST_PAIRING_CONTROLLER_H_
