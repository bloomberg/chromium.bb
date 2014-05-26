// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_FACTORY_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {
namespace file_system_provider {

class Service;

// Creates services per profile.
class ServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns a service instance singleton, after creating it (if necessary).
  static Service* Get(content::BrowserContext* context);

  // Returns a service instance for the context if exists. Otherwise, returns
  // NULL.
  static Service* FindExisting(content::BrowserContext* context);

  // Gets a singleton instance of the factory.
  static ServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ServiceFactory>;

  ServiceFactory();
  virtual ~ServiceFactory();

  // BrowserContextKeyedServiceFactory overrides:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ServiceFactory);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_SERVICE_FACTORY_H_
