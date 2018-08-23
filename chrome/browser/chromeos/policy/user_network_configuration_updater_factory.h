// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_NETWORK_CONFIGURATION_UPDATER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_NETWORK_CONFIGURATION_UPDATER_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}

namespace policy {

class UserNetworkConfigurationUpdater;

// Factory to create UserNetworkConfigurationUpdater.
class UserNetworkConfigurationUpdaterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns an existing or creates a new UserNetworkConfigurationUpdater for
  // |browser_context|. Will return nullptr if this service isn't allowed for
  // |browser_context|, i.e. for all but the BrowserContext which refers to the
  // primary user's profile.
  static UserNetworkConfigurationUpdater* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static UserNetworkConfigurationUpdaterFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      UserNetworkConfigurationUpdaterFactory>;

  UserNetworkConfigurationUpdaterFactory();
  ~UserNetworkConfigurationUpdaterFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(UserNetworkConfigurationUpdaterFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_NETWORK_CONFIGURATION_UPDATER_FACTORY_H_
