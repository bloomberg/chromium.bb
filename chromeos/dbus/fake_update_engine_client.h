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

  // UpdateEngineClient overrides
  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool HasObserver(Observer* observer) OVERRIDE;
  virtual void RequestUpdateCheck(const UpdateCheckCallback& callback) OVERRIDE;
  virtual void RebootAfterUpdate() OVERRIDE;
  virtual void Rollback() OVERRIDE;
  virtual void CanRollbackCheck(
      const RollbackCheckCallback& callback) OVERRIDE;
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

  // Sends status change notification.
  void NotifyObserversThatStatusChanged(
      const UpdateEngineClient::Status& status);

  // Sets the default UpdateEngineClient::Status. GetLastStatus() returns the
  // value set here if |status_queue_| is empty.
  void set_default_status(const UpdateEngineClient::Status& status);

  // Sets a value returned by RequestUpdateCheck().
  void set_update_check_result(
      const UpdateEngineClient::UpdateCheckResult& result);

  void set_can_rollback_check_result(bool result) {
      can_rollback_stub_result_ = result;
  }

  // Returns how many times RebootAfterUpdate() is called.
  int reboot_after_update_call_count() const {
      return reboot_after_update_call_count_;
  }

  // Returns how many times Rollback() is called.
  int rollback_call_count() const { return rollback_call_count_; }

  // Returns how many times Rollback() is called.
  int can_rollback_call_count() const { return can_rollback_call_count_; }

 private:
  ObserverList<Observer> observers_;
  std::queue<UpdateEngineClient::Status> status_queue_;
  UpdateEngineClient::Status default_status_;
  UpdateEngineClient::UpdateCheckResult update_check_result_;
  bool can_rollback_stub_result_;
  int reboot_after_update_call_count_;
  int rollback_call_count_;
  int can_rollback_call_count_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_UPDATE_ENGINE_CLIENT_H_
