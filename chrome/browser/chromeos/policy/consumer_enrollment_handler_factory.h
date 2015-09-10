// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_ENROLLMENT_HANDLER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_ENROLLMENT_HANDLER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace policy {

class ConsumerEnrollmentHandler;

class ConsumerEnrollmentHandlerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ConsumerEnrollmentHandler* GetForBrowserContext(
      content::BrowserContext* context);

  static ConsumerEnrollmentHandlerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ConsumerEnrollmentHandlerFactory>;

  ConsumerEnrollmentHandlerFactory();
  ~ConsumerEnrollmentHandlerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(ConsumerEnrollmentHandlerFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_ENROLLMENT_HANDLER_FACTORY_H_
