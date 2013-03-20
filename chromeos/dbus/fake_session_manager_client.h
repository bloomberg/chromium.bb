// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SESSION_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SESSION_MANAGER_CLIENT_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "chromeos/dbus/session_manager_client.h"

namespace chromeos {

// A fake implementation of session_manager. Accepts policy blobs to be set and
// returns them unmodified.
class FakeSessionManagerClient : public chromeos::SessionManagerClient {
 public:
  FakeSessionManagerClient();
  virtual ~FakeSessionManagerClient();

  // SessionManagerClient:
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool HasObserver(Observer* observer) OVERRIDE;
  virtual void EmitLoginPromptReady() OVERRIDE;
  virtual void EmitLoginPromptVisible() OVERRIDE;
  virtual void RestartJob(int pid, const std::string& command_line) OVERRIDE;
  virtual void RestartEntd() OVERRIDE;
  virtual void StartSession(const std::string& user_email) OVERRIDE;
  virtual void StopSession() OVERRIDE;
  virtual void StartDeviceWipe() OVERRIDE;
  virtual void RequestLockScreen() OVERRIDE;
  virtual void NotifyLockScreenShown() OVERRIDE;
  virtual void RequestUnlockScreen() OVERRIDE;
  virtual void NotifyLockScreenDismissed() OVERRIDE;
  virtual void RetrieveDevicePolicy(
      const RetrievePolicyCallback& callback) OVERRIDE;
  virtual void RetrieveUserPolicy(
      const RetrievePolicyCallback& callback) OVERRIDE;
  virtual void RetrieveDeviceLocalAccountPolicy(
      const std::string& account_id,
      const RetrievePolicyCallback& callback) OVERRIDE;
  virtual void StoreDevicePolicy(const std::string& policy_blob,
                                 const StorePolicyCallback& callback) OVERRIDE;
  virtual void StoreUserPolicy(const std::string& policy_blob,
                               const StorePolicyCallback& callback) OVERRIDE;
  virtual void StoreDeviceLocalAccountPolicy(
      const std::string& account_id,
      const std::string& policy_blob,
      const StorePolicyCallback& callback) OVERRIDE;

  const std::string& device_policy() const;
  void set_device_policy(const std::string& policy_blob);

  const std::string& user_policy() const;
  void set_user_policy(const std::string& policy_blob);

  const std::string& device_local_account_policy(
      const std::string& account_id) const;
  void set_device_local_account_policy(const std::string& account_id,
                                       const std::string& policy_blob);

  // Notify observers about a property change completion.
  void OnPropertyChangeComplete(bool success);

 private:
  std::string device_policy_;
  std::string user_policy_;
  std::map<std::string, std::string> device_local_account_policy_;
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeSessionManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SESSION_MANAGER_CLIENT_H_
