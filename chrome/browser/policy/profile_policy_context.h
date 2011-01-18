// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_PROFILE_POLICY_CONTEXT_H_
#define CHROME_BROWSER_POLICY_PROFILE_POLICY_CONTEXT_H_
#pragma once

#include "base/scoped_ptr.h"

class Profile;

namespace policy {

class DeviceManagementPolicyProvider;
class DeviceManagementService;

// This class is a container for the profile-specific policy bits located in the
// profile. Since the context owns the policy provider, it's vital that it gets
// initialized before the profile's prefs and destroyed after the prefs are
// gone.
class ProfilePolicyContext {
 public:
  explicit ProfilePolicyContext(Profile* profile);
  ~ProfilePolicyContext();

  // Initializes the context. Should be called only after the profile's request
  // context is up.
  void Initialize();

  // Shuts the context down. This must be called before the networking
  // infrastructure in the profile goes away.
  void Shutdown();

  // Get the policy provider.
  DeviceManagementPolicyProvider* GetDeviceManagementPolicyProvider();

 private:
  // The profile this context is associated with.
  Profile* profile_;

  // The device management service.
  scoped_ptr<DeviceManagementService> device_management_service_;

  // Our provider.
  scoped_ptr<DeviceManagementPolicyProvider> device_management_policy_provider_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_PROFILE_POLICY_CONTEXT_H_
