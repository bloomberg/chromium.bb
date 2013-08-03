// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_UPDATE_ENGINE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_UPDATE_ENGINE_CLIENT_H_

#include <queue>
#include <string>

#include "chromeos/dbus/update_engine_client.h"

namespace chromeos {

// A fake implementation of UpdateEngineClient. The user of this class can
// use set_update_engine_client_status() to set a fake last Status and
// GetLastStatus() returns the fake with no modification. Other methods do
// nothing.
class FakeUpdateEngineClient : public UpdateEngineClient {
 public:
  FakeUpdateEngineClient();
  virtual ~FakeUpdateEngineClient();

  // Overrides
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool HasObserver(Observer* observer) OVERRIDE;
  virtual void RequestUpdateCheck(const UpdateCheckCallback& callback) OVERRIDE;
  virtual void RebootAfterUpdate() OVERRIDE;
  virtual Status GetLastStatus() OVERRIDE;
  virtual void SetChannel(const std::string& target_channel,
                          bool is_powerwash_allowed) OVERRIDE;
  virtual void GetChannel(bool get_current_channel,
                          const GetChannelCallback& callback) OVERRIDE;

  // Pushes UpdateEngineClient::Status in the queue to test changing status.
  // GetLastStatus() returns the status set by this method in FIFO order.
  // See set_default_status().
  void PushLastStatus(const UpdateEngineClient::Status& status) {
    status_queue_.push(status);
  }

  // Sets the default UpdateEngineClient::Status. GetLastStatus() returns the
  // value set here if |status_queue_| is empty.
  void set_default_status(const UpdateEngineClient::Status& status);

  // Sets a value returned by RequestUpdateCheck().
  void set_update_check_result(
      const UpdateEngineClient::UpdateCheckResult& result);

  // Returns how many times RebootAfterUpdate() is called.
  int reboot_after_update_call_count() {
    return reboot_after_update_call_count_;
  }

 private:
  std::queue<UpdateEngineClient::Status> status_queue_;
  UpdateEngineClient::Status default_status_;
  UpdateEngineClient::UpdateCheckResult update_check_result_;
  int reboot_after_update_call_count_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_UPDATE_ENGINE_CLIENT_H_
