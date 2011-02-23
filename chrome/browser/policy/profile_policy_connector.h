// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

class Profile;

namespace policy {

class CloudPolicySubsystem;
class ConfigurationPolicyProvider;
class UserPolicyIdentityStrategy;

// This class is a container for the profile-specific policy bits located in the
// profile. Since the subsystem owns the policy provider, it's vital that it
// gets initialized before the profile's prefs and destroyed after the prefs
// are gone.
class ProfilePolicyConnector {
 public:
  explicit ProfilePolicyConnector(Profile* profile);
  ~ProfilePolicyConnector();

  // Initializes the context. Should be called only after the profile's request
  // context is up.
  void Initialize();

  // Shuts the context down. This must be called before the networking
  // infrastructure in the profile goes away.
  void Shutdown();

  ConfigurationPolicyProvider* GetManagedCloudProvider();
  ConfigurationPolicyProvider* GetRecommendedCloudProvider();

  static void RegisterPrefs(PrefService* user_prefs);

  static const int kDefaultPolicyRefreshRateInMilliseconds =
      3 * 60 * 60 * 1000;  // 3 hours.

 private:
  Profile* profile_;

  scoped_ptr<UserPolicyIdentityStrategy> identity_strategy_;
  scoped_ptr<CloudPolicySubsystem> cloud_policy_subsystem_;

  DISALLOW_COPY_AND_ASSIGN(ProfilePolicyConnector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_
