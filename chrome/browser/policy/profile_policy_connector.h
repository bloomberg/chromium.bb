// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_method_call_status.h"
#endif

class Profile;

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class DeviceLocalAccountPolicyProvider;
class ManagedModePolicyProvider;
class PolicyService;

// A ProfileKeyedService that creates and manages the per-Profile policy
// components.
class ProfilePolicyConnector : public ProfileKeyedService {
 public:
  explicit ProfilePolicyConnector(Profile* profile);
  virtual ~ProfilePolicyConnector();

  // If |force_immediate_load| then disk caches will be loaded synchronously.
  void Init(bool force_immediate_load,
            base::SequencedTaskRunner* sequenced_task_runner);

  void InitForTesting(scoped_ptr<PolicyService> service);

  // ProfileKeyedService:
  virtual void Shutdown() OVERRIDE;

  // This is never NULL.
  PolicyService* policy_service() const { return policy_service_.get(); }

#if defined(ENABLE_MANAGED_USERS) && defined(ENABLE_CONFIGURATION_POLICY)
  ManagedModePolicyProvider* managed_mode_policy_provider() const {
    return managed_mode_policy_provider_.get();
  }
#endif

  // Returns true if |profile()| has used certificates installed via policy
  // to establish a secure connection before. This means that it may have
  // cached content from an untrusted source.
  bool UsedPolicyCertificates();

 private:
#if defined(ENABLE_CONFIGURATION_POLICY)

#if defined(OS_CHROMEOS)
  void InitializeDeviceLocalAccountPolicyProvider(const std::string& username);

  // Callback for CryptohomeClient::GetSanitizedUsername() that initializes the
  // NetworkConfigurationUpdater after receiving the hashed username.
  void InitializeNetworkConfigurationUpdater(
      bool is_managed,
      chromeos::DBusMethodCallStatus status,
      const std::string& hashed_username);
#endif

  Profile* profile_;

#if defined(OS_CHROMEOS)
  // Some of the user policy configuration affects browser global state, and
  // can only come from one Profile. |is_primary_user_| is true if this
  // connector belongs to the first signed-in Profile, and in that case that
  // Profile's policy is the one that affects global policy settings in
  // local state.
  bool is_primary_user_;

  scoped_ptr<DeviceLocalAccountPolicyProvider>
      device_local_account_policy_provider_;
#endif

#if defined(ENABLE_MANAGED_USERS) && defined(ENABLE_CONFIGURATION_POLICY)
  scoped_ptr<ManagedModePolicyProvider> managed_mode_policy_provider_;
#endif

  base::WeakPtrFactory<ProfilePolicyConnector> weak_ptr_factory_;
#endif  // ENABLE_CONFIGURATION_POLICY

  scoped_ptr<PolicyService> policy_service_;

  DISALLOW_COPY_AND_ASSIGN(ProfilePolicyConnector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_
