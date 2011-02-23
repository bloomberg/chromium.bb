// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class PrefService;
class TestingBrowserProcess;
class TokenService;
class URLRequestContextGetter;

namespace policy {

class CloudPolicyIdentityStrategy;
class CloudPolicySubsystem;
class ConfigurationPolicyProvider;

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

  static void RegisterPrefs(PrefService* user_prefs);

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
                  URLRequestContextGetter* request_context);

  scoped_ptr<ConfigurationPolicyProvider> managed_platform_provider_;
  scoped_ptr<ConfigurationPolicyProvider> recommended_platform_provider_;

  scoped_ptr<CloudPolicyIdentityStrategy> identity_strategy_;
  scoped_ptr<CloudPolicySubsystem> cloud_policy_subsystem_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPolicyConnector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_H_
