// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_UPDATE_ENGINE_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_UPDATE_ENGINE_CLIENT_H_

#include "base/callback.h"
#include "base/observer_list.h"

#include <string>

namespace dbus {
class Bus;
}  // namespace

namespace chromeos {

// UpdateEngineClient is used to communicate with the update engine.
class UpdateEngineClient {
 public:
  // Edges for state machine
  //    IDLE->CHECKING_FOR_UPDATE
  //    CHECKING_FOR_UPDATE->IDLE
  //    CHECKING_FOR_UPDATE->UPDATE_AVAILABLE
  //    ...
  //    FINALIZING->UPDATE_NEED_REBOOT
  // Any state can transition to REPORTING_ERROR_EVENT and then on to IDLE.
  enum UpdateStatusOperation {
    UPDATE_STATUS_ERROR = -1,
    UPDATE_STATUS_IDLE = 0,
    UPDATE_STATUS_CHECKING_FOR_UPDATE,
    UPDATE_STATUS_UPDATE_AVAILABLE,
    UPDATE_STATUS_DOWNLOADING,
    UPDATE_STATUS_VERIFYING,
    UPDATE_STATUS_FINALIZING,
    UPDATE_STATUS_UPDATED_NEED_REBOOT,
    UPDATE_STATUS_REPORTING_ERROR_EVENT
  };

  // The status of the ongoing update attempt.
  struct Status {
    Status() : status(UPDATE_STATUS_IDLE),
               download_progress(0.0),
               last_checked_time(0),
               new_size(0) {
    }

    UpdateStatusOperation status;
    double download_progress;   // 0.0 - 1.0
    int64_t last_checked_time;  // As reported by std::time().
    std::string new_version;
    int64_t new_size;           // Valid during DOWNLOADING, in bytes.
  };

  // The result code used for RequestUpdateCheck().
  enum UpdateCheckResult {
    UPDATE_RESULT_SUCCESS,
    UPDATE_RESULT_FAILED,
  };

  // Interface for observing changes from the update engine.
  class Observer {
   public:
    // Called when the status is updated.
    virtual void UpdateStatusChanged(const Status& status) {}
  };

  virtual ~UpdateEngineClient();

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  // Returns true if this object has the given observer.
  virtual bool HasObserver(Observer* observer) = 0;

  // Called once RequestUpdateCheck() is complete. Takes one parameter:
  // - UpdateCheckResult: the result of the update check.
  typedef base::Callback<void(UpdateCheckResult)> UpdateCheckCallback;

  // Requests an update check and calls |callback| when completed.
  virtual void RequestUpdateCheck(UpdateCheckCallback callback) = 0;

  // Reboots if update has been performed.
  virtual void RebootAfterUpdate() = 0;

  // Requests to set the release track (channel). |track| should look like
  // "beta-channel" or "dev-channel".
  virtual void SetReleaseTrack(const std::string& track) = 0;

  // Called once GetReleaseTrack() is complete. Takes one parameter;
  // - string: the release track name like "beta-channel".
  typedef base::Callback<void(const std::string&)> GetReleaseTrackCallback;

  // Requests to get the release track and calls |callback| with the
  // release track (channel). On error, calls |callback| with an empty
  // string.
  virtual void GetReleaseTrack(GetReleaseTrackCallback callback) = 0;

  // Returns the last status the object received from the update engine.
  //
  // Ideally, the D-Bus client should be state-less, but there are clients
  // that need this information.
  virtual Status GetLastStatus() = 0;

  // Returns an empty UpdateCheckCallback that does nothing.
  static UpdateCheckCallback EmptyUpdateCheckCallback();

  // Creates the instance.
  static UpdateEngineClient* Create(dbus::Bus* bus);

 protected:
  // Create() should be used instead.
  UpdateEngineClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateEngineClient);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_UPDATE_ENGINE_CLIENT_H_
