// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_NOTIFIER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_NOTIFIER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace policy {

class ConsumerManagementNotifier;

class ConsumerManagementNotifierFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ConsumerManagementNotifier* GetForBrowserContext(
      content::BrowserContext* context);

  static ConsumerManagementNotifierFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ConsumerManagementNotifierFactory>;

  ConsumerManagementNotifierFactory();
  ~ConsumerManagementNotifierFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(ConsumerManagementNotifierFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_NOTIFIER_FACTORY_H_
