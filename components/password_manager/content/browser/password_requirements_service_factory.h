// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace password_manager {

class PasswordRequirementsService;

class PasswordRequirementsServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static PasswordRequirementsServiceFactory* GetInstance();

  // Returns the PasswordRequirementsService associated with |context|.
  // This may be nullptr for an incognito |context|.
  static PasswordRequirementsService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<
      PasswordRequirementsServiceFactory>;

  PasswordRequirementsServiceFactory();
  ~PasswordRequirementsServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PasswordRequirementsServiceFactory);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_PASSWORD_REQUIREMENTS_SERVICE_FACTORY_H_
