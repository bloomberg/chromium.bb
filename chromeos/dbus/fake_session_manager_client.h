// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SESSION_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SESSION_MANAGER_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/login_manager/arc.pb.h"
#include "chromeos/dbus/session_manager_client.h"

namespace chromeos {

// A fake implementation of session_manager. Accepts policy blobs to be set and
// returns them unmodified.
class FakeSessionManagerClient : public SessionManagerClient {
 public:
  enum FakeSessionManagerOptions : uint32_t {
    NONE = 0,
    USE_HOST_POLICY = 1 << 0,
  };

  FakeSessionManagerClient();
  explicit FakeSessionManagerClient(
      uint32_t options /* bitwise or of multiple FakeSessionManagerOptions */);
  ~FakeSessionManagerClient() override;

  // SessionManagerClient overrides
  void Init(dbus::Bus* bus) override;
  void SetStubDelegate(StubDelegate* delegate) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  bool IsScreenLocked() const override;
  void EmitLoginPromptVisible() override;
  void EmitAshInitialized() override;
  void RestartJob(int socket_fd,
                  const std::vector<std::string>& argv,
                  VoidDBusMethodCallback callback) override;
  void SaveLoginPassword(const std::string& password) override;
  void StartSession(const cryptohome::Identification& cryptohome_id) override;
  void StopSession() override;
  void NotifySupervisedUserCreationStarted() override;
  void NotifySupervisedUserCreationFinished() override;
  void StartDeviceWipe() override;
  void StartTPMFirmwareUpdate(const std::string& update_mode) override;
  void RequestLockScreen() override;
  void NotifyLockScreenShown() override;
  void NotifyLockScreenDismissed() override;
  void RetrieveActiveSessions(ActiveSessionsCallback callback) override;
  void RetrieveDevicePolicy(RetrievePolicyCallback callback) override;
  RetrievePolicyResponseType BlockingRetrieveDevicePolicy(
      std::string* policy_out) override;
  void RetrievePolicyForUser(const cryptohome::Identification& cryptohome_id,
                             RetrievePolicyCallback callback) override;
  RetrievePolicyResponseType BlockingRetrievePolicyForUser(
      const cryptohome::Identification& cryptohome_id,
      std::string* policy_out) override;
  void RetrievePolicyForUserWithoutSession(
      const cryptohome::Identification& cryptohome_id,
      RetrievePolicyCallback callback) override;
  void RetrieveDeviceLocalAccountPolicy(
      const std::string& account_id,
      RetrievePolicyCallback callback) override;
  RetrievePolicyResponseType BlockingRetrieveDeviceLocalAccountPolicy(
      const std::string& account_id,
      std::string* policy_out) override;
  void StoreDevicePolicy(const std::string& policy_blob,
                         VoidDBusMethodCallback callback) override;
  void StorePolicyForUser(const cryptohome::Identification& cryptohome_id,
                          const std::string& policy_blob,
                          VoidDBusMethodCallback callback) override;
  void StoreDeviceLocalAccountPolicy(const std::string& account_id,
                                     const std::string& policy_blob,
                                     VoidDBusMethodCallback callback) override;
  bool SupportsRestartToApplyUserFlags() const override;
  void SetFlagsForUser(const cryptohome::Identification& cryptohome_id,
                       const std::vector<std::string>& flags) override;
  void GetServerBackedStateKeys(StateKeysCallback callback) override;

  void StartArcMiniContainer(
      const login_manager::StartArcMiniContainerRequest& request,
      StartArcMiniContainerCallback callback) override;
  void UpgradeArcContainer(
      const login_manager::UpgradeArcContainerRequest& request,
      UpgradeArcContainerCallback success_callback,
      UpgradeErrorCallback error_callback) override;
  void StopArcInstance(VoidDBusMethodCallback callback) override;
  void SetArcCpuRestriction(
      login_manager::ContainerCpuRestrictionState restriction_state,
      VoidDBusMethodCallback callback) override;
  void EmitArcBooted(const cryptohome::Identification& cryptohome_id,
                     VoidDBusMethodCallback callback) override;
  void GetArcStartTime(DBusMethodCallback<base::TimeTicks> callback) override;
  void RemoveArcData(const cryptohome::Identification& cryptohome_id,
                     VoidDBusMethodCallback callback) override;

  // Notifies observers as if ArcInstanceStopped signal is received.
  void NotifyArcInstanceStopped(bool clean,
                                const std::string& conainer_instance_id);

  // Returns true if flags for |cryptohome_id| have been set. If the return
  // value is |true|, |*out_flags_for_user| is filled with the flags passed to
  // |SetFlagsForUser|.
  bool GetFlagsForUser(const cryptohome::Identification& cryptohome_id,
                       std::vector<std::string>* out_flags_for_user) const;

  // Sets whether FakeSessionManagerClient should advertise (through
  // |SupportsRestartToApplyUserFlags|) that it supports restarting chrome to
  // apply user-session flags. The default is |false|.
  void set_supports_restart_to_apply_user_flags(
      bool supports_restart_to_apply_user_flags) {
    supports_restart_to_apply_user_flags_ =
        supports_restart_to_apply_user_flags;
  }

  void set_store_device_policy_success(bool success) {
    store_device_policy_success_ = success;
  }
  const std::string& device_policy() const;
  void set_device_policy(const std::string& policy_blob);

  const std::string& user_policy(
      const cryptohome::Identification& cryptohome_id) const;
  void set_user_policy(const cryptohome::Identification& cryptohome_id,
                       const std::string& policy_blob);
  void set_user_policy_without_session(
      const cryptohome::Identification& cryptohome_id,
      const std::string& policy_blob);

  void set_store_user_policy_success(bool success) {
    store_user_policy_success_ = success;
  }

  const std::string& device_local_account_policy(
      const std::string& account_id) const;
  void set_device_local_account_policy(const std::string& account_id,
                                       const std::string& policy_blob);

  const login_manager::UpgradeArcContainerRequest& last_upgrade_arc_request()
      const {
    return last_upgrade_arc_request_;
  }

  // Notify observers about a property change completion.
  void OnPropertyChangeComplete(bool success);

  // Configures the list of state keys used to satisfy
  // GetServerBackedStateKeys() requests.
  void set_server_backed_state_keys(
      const std::vector<std::string>& state_keys) {
    server_backed_state_keys_ = state_keys;
  }

  int start_device_wipe_call_count() const {
    return start_device_wipe_call_count_;
  }
  int request_lock_screen_call_count() const {
    return request_lock_screen_call_count_;
  }

  // Returns how many times LockScreenShown() was called.
  int notify_lock_screen_shown_call_count() const {
    return notify_lock_screen_shown_call_count_;
  }

  // Returns how many times LockScreenDismissed() was called.
  int notify_lock_screen_dismissed_call_count() const {
    return notify_lock_screen_dismissed_call_count_;
  }

  void set_arc_available(bool available) { arc_available_ = available; }
  void set_arc_start_time(base::TimeTicks arc_start_time) {
    arc_start_time_ = arc_start_time;
  }

  void set_low_disk(bool low_disk) { low_disk_ = low_disk; }

  const std::string& container_instance_id() const {
    return container_instance_id_;
  }

 private:
  bool supports_restart_to_apply_user_flags_ = false;

  bool store_device_policy_success_ = true;
  std::string device_policy_;
  std::map<cryptohome::Identification, std::string> user_policies_;
  std::map<cryptohome::Identification, std::string>
      user_policies_without_session_;

  // Controls whether StorePolicyForUser() should report success or not.
  bool store_user_policy_success_ = true;

  std::map<std::string, std::string> device_local_account_policy_;
  base::ObserverList<Observer> observers_;
  SessionManagerClient::ActiveSessionsMap user_sessions_;
  std::vector<std::string> server_backed_state_keys_;

  int start_device_wipe_call_count_;
  int request_lock_screen_call_count_;
  int notify_lock_screen_shown_call_count_;
  int notify_lock_screen_dismissed_call_count_;
  bool screen_is_locked_;

  bool arc_available_;
  base::TimeTicks arc_start_time_;

  bool low_disk_ = false;
  // Pseudo running container id. If not running, empty.
  std::string container_instance_id_;

  // Contains last requst passed to StartArcInstance
  login_manager::UpgradeArcContainerRequest last_upgrade_arc_request_;

  StubDelegate* delegate_;

  // Options for FakeSessionManagerClient with value of bitwise or of
  // multiple FakeSessionManagerOptions.
  uint32_t options_;

  // The last-set flags for user set through |SetFlagsForUser|.
  std::map<cryptohome::Identification, std::vector<std::string>>
      flags_for_user_;

  base::WeakPtrFactory<FakeSessionManagerClient> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FakeSessionManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SESSION_MANAGER_CLIENT_H_
