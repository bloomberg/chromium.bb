// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_SUBSYSTEM_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_SUBSYSTEM_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/common/notification_observer.h"

class FilePath;
class PrefService;
class URLRequestContextGetter;

namespace policy {

class CloudPolicyCache;
class CloudPolicyController;
class CloudPolicyIdentityStrategy;
class ConfigurationPolicyProvider;
class DeviceManagementService;
class DeviceTokenFetcher;

// This class is a container for the infrastructure required to support cloud
// policy. It glues together the backend, the policy controller and manages the
// life cycle of the policy providers.
class CloudPolicySubsystem : public NotificationObserver {
 public:
  CloudPolicySubsystem(const FilePath& policy_cache_file,
                       CloudPolicyIdentityStrategy* identity_strategy);
  virtual ~CloudPolicySubsystem();

  // Initializes the subsystem.
  void Initialize(PrefService* prefs,
                  const char* refresh_rate_pref_name,
                  URLRequestContextGetter* request_context);

  // Shuts the subsystem down. This must be called before threading and network
  // infrastructure goes away.
  void Shutdown();

  ConfigurationPolicyProvider* GetManagedPolicyProvider();
  ConfigurationPolicyProvider* GetRecommendedPolicyProvider();

 private:
  // Updates the policy controller with a new refresh rate value.
  void UpdatePolicyRefreshRate();

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The pref service that controls the refresh rate.
  PrefService* prefs_;

  // Tracks the pref value for the policy refresh rate.
  IntegerPrefMember policy_refresh_rate_;

  // Cloud policy infrastructure stuff.
  scoped_ptr<DeviceManagementService> device_management_service_;
  scoped_ptr<DeviceTokenFetcher> device_token_fetcher_;
  scoped_ptr<CloudPolicyCache> cloud_policy_cache_;
  scoped_ptr<CloudPolicyController> cloud_policy_controller_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicySubsystem);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_SUBSYSTEM_H_
