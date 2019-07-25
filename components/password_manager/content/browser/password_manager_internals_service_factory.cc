// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"

#include "components/autofill/core/browser/logging/log_router.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace password_manager {

using autofill::LogRouter;

// static
LogRouter* PasswordManagerInternalsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LogRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, /* create = */ true));
}

// static
PasswordManagerInternalsServiceFactory*
PasswordManagerInternalsServiceFactory::GetInstance() {
  return base::Singleton<PasswordManagerInternalsServiceFactory>::get();
}

PasswordManagerInternalsServiceFactory::PasswordManagerInternalsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PasswordManagerInternalsService",
          BrowserContextDependencyManager::GetInstance()) {}

PasswordManagerInternalsServiceFactory::
    ~PasswordManagerInternalsServiceFactory() {}

KeyedService* PasswordManagerInternalsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* /* context */) const {
  return new LogRouter();
}

}  // namespace password_manager
