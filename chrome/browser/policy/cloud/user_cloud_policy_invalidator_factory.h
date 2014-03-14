// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_INVALIDATOR_FACTORY_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_INVALIDATOR_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace policy {

// Creates an instance of UserCloudPolicyInvalidator for each profile.
class UserCloudPolicyInvalidatorFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static UserCloudPolicyInvalidatorFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<UserCloudPolicyInvalidatorFactory>;

  UserCloudPolicyInvalidatorFactory();
  virtual ~UserCloudPolicyInvalidatorFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyInvalidatorFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_INVALIDATOR_FACTORY_H_
