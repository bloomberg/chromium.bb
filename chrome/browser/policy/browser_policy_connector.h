// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class PrefService;
class TestingBrowserProcess;
class TokenService;

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class CloudPolicySubsystem;
class ConfigurationPolicyProvider;
class DevicePolicyIdentityStrategy;

// Manages the lifecycle of browser-global policy infrastructure, such as the
// platform policy providers.
class BrowserPolicyConnector : public NotificationObserver {
 public:
  static const int kDefaultPolicyRefreshRateInMilliseconds =
      3 * 60 * 60 * 1000;  // 3 hours.

  BrowserPolicyConnector();
  ~BrowserPolicyConnector();

  ConfigurationPolicyProvider* GetManagedPlatformProvider() const;
  ConfigurationPolicyProvider* GetManagedCloudProvider() const;
  ConfigurationPolicyProvider* GetRecommendedPlatformProvider() const;
  ConfigurationPolicyProvider* GetRecommendedCloudProvider() const;

  // Returns a weak pointer to the CloudPolicySubsystem managed by this
  // policy connector, or NULL if no such subsystem exists (i.e. when running
  // outside ChromeOS).
  CloudPolicySubsystem* cloud_policy_subsystem() {
    return cloud_policy_subsystem_.get();
  }

  static void RegisterPrefs(PrefService* user_prefs);

  // Used to set the credentials stored in the identity strategy associated
  // with this policy connector.
  void SetCredentials(const std::string& owner_email,
                      const std::string& gaia_token);

  // Returns true if this device is managed by an enterprise (as opposed to
  // a local owner).
  bool IsEnterpriseManaged();

  // Returns the enterprise domain if device is managed.
  std::string GetEnterpriseDomain();

  // Exposes the StopAutoRetry() method of the CloudPolicySubsystem managed
  // by this connector, which can be used to disable automatic
  // retrying behavior.
  void StopAutoRetry();

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend class ::TestingBrowserProcess;

  static ConfigurationPolicyProvider* CreateManagedPlatformProvider();
  static ConfigurationPolicyProvider* CreateRecommendedPlatformProvider();

  // Constructor for tests that allows tests to use fake platform policy
  // providers instead of using the actual ones.
  BrowserPolicyConnector(
      ConfigurationPolicyProvider* managed_platform_provider,
      ConfigurationPolicyProvider* recommended_platform_provider);

  // Activates the cloud policy subsystem. Called when the default request
  // context is available.
  void Initialize(PrefService* local_state,
                  net::URLRequestContextGetter* request_context);

  scoped_ptr<ConfigurationPolicyProvider> managed_platform_provider_;
  scoped_ptr<ConfigurationPolicyProvider> recommended_platform_provider_;

#if defined(OS_CHROMEOS)
  scoped_ptr<DevicePolicyIdentityStrategy> identity_strategy_;
#endif
  scoped_ptr<CloudPolicySubsystem> cloud_policy_subsystem_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPolicyConnector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_
