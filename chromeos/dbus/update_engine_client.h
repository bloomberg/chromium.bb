// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_UPDATE_ENGINE_CLIENT_H_
#define CHROMEOS_DBUS_UPDATE_ENGINE_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/update_engine/dbus-constants.h"

namespace chromeos {

// UpdateEngineClient is used to communicate with the update engine.
class CHROMEOS_EXPORT UpdateEngineClient : public DBusClient {
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
    UPDATE_STATUS_REPORTING_ERROR_EVENT,
    UPDATE_STATUS_ATTEMPTING_ROLLBACK
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
    UPDATE_RESULT_NOTIMPLEMENTED,
  };

  // Interface for observing changes from the update engine.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the status is updated.
    virtual void UpdateStatusChanged(const Status& status) {}
  };

  ~UpdateEngineClient() override;

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  // Returns true if this object has the given observer.
  virtual bool HasObserver(const Observer* observer) const = 0;

  // Called once RequestUpdateCheck() is complete. Takes one parameter:
  // - UpdateCheckResult: the result of the update check.
  typedef base::Callback<void(UpdateCheckResult)> UpdateCheckCallback;

  // Requests an update check and calls |callback| when completed.
  virtual void RequestUpdateCheck(const UpdateCheckCallback& callback) = 0;

  // Reboots if update has been performed.
  virtual void RebootAfterUpdate() = 0;

  // Starts Rollback.
  virtual void Rollback() = 0;

  // Called once CanRollbackCheck() is complete. Takes one parameter:
  // - bool: the result of the rollback availability check.
  typedef base::Callback<void(bool can_rollback)> RollbackCheckCallback;

  // Checks if Rollback is available and calls |callback| when completed.
  virtual void CanRollbackCheck(
      const RollbackCheckCallback& callback) = 0;

  // Called once GetChannel() is complete. Takes one parameter;
  // - string: the channel name like "beta-channel".
  typedef base::Callback<void(const std::string& channel_name)>
      GetChannelCallback;

  // Returns the last status the object received from the update engine.
  //
  // Ideally, the D-Bus client should be state-less, but there are clients
  // that need this information.
  virtual Status GetLastStatus() = 0;

  // Changes the current channel of the device to the target
  // channel. If the target channel is a less stable channel than the
  // current channel, then the channel change happens immediately (at
  // the next update check).  If the target channel is a more stable
  // channel, then if |is_powerwash_allowed| is set to true, then also
  // the change happens immediately but with a powerwash if
  // required. Otherwise, the change takes effect eventually (when the
  // version on the target channel goes above the version number of
  // what the device currently has). |target_channel| should look like
  // "dev-channel", "beta-channel" or "stable-channel".
  virtual void SetChannel(const std::string& target_channel,
                          bool is_powerwash_allowed) = 0;

  // If |get_current_channel| is set to true, calls |callback| with
  // the name of the channel that the device is currently
  // on. Otherwise, it calls it with the name of the channel the
  // device is supposed to be (in case of a pending channel
  // change). On error, calls |callback| with an empty string.
  virtual void GetChannel(bool get_current_channel,
                          const GetChannelCallback& callback) = 0;

  // Called once GetEolStatus() is complete. Takes one parameter;
  // - EndOfLife Status: the end of life status of the device.
  typedef base::Callback<void(update_engine::EndOfLifeStatus status)>
      GetEolStatusCallback;

  // Get EndOfLife status of the device and calls |callback| when completed.
  virtual void GetEolStatus(const GetEolStatusCallback& callback) = 0;

  // Either allow or disallow receiving updates over cellular connections.
  // Synchronous (blocking) method.
  virtual void SetUpdateOverCellularPermission(
      bool allowed,
      const base::Closure& callback) = 0;

  // Returns an empty UpdateCheckCallback that does nothing.
  static UpdateCheckCallback EmptyUpdateCheckCallback();

  // Creates the instance.
  static UpdateEngineClient* Create(DBusClientImplementationType type);

  // Returns true if |target_channel| is more stable than |current_channel|.
  static bool IsTargetChannelMoreStable(const std::string& current_channel,
                                        const std::string& target_channel);

 protected:
  // Create() should be used instead.
  UpdateEngineClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateEngineClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_UPDATE_ENGINE_CLIENT_H_
