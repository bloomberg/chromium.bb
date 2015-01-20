// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_UNENROLLMENT_HANDLER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_UNENROLLMENT_HANDLER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace policy {

class ConsumerUnenrollmentHandler;

class ConsumerUnenrollmentHandlerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ConsumerUnenrollmentHandler* GetForBrowserContext(
      content::BrowserContext* context);

  static ConsumerUnenrollmentHandlerFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ConsumerUnenrollmentHandlerFactory>;

  ConsumerUnenrollmentHandlerFactory();
  ~ConsumerUnenrollmentHandlerFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ConsumerUnenrollmentHandlerFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_UNENROLLMENT_HANDLER_FACTORY_H_
