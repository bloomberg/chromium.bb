// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/password_requirements_service_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "components/autofill/core/browser/password_requirements_spec_fetcher.h"
#include "components/autofill/core/browser/password_requirements_spec_fetcher_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace password_manager {

// static
PasswordRequirementsServiceFactory*
PasswordRequirementsServiceFactory::GetInstance() {
  return base::Singleton<PasswordRequirementsServiceFactory>::get();
}

// static
PasswordRequirementsService*
PasswordRequirementsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PasswordRequirementsService*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

PasswordRequirementsServiceFactory::PasswordRequirementsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PasswordRequirementsServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

PasswordRequirementsServiceFactory::~PasswordRequirementsServiceFactory() {}

KeyedService* PasswordRequirementsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord())
    return nullptr;

  // TODO(crbug.com/846694): These should become finch parameters.
  const int kVersion = 1;
  const size_t kPrefixLength = 0;
  const int kTimeout = 5000;
  std::unique_ptr<autofill::PasswordRequirementsSpecFetcher> fetcher =
      std::make_unique<autofill::PasswordRequirementsSpecFetcherImpl>(
          content::BrowserContext::GetDefaultStoragePartition(context)
              ->GetURLLoaderFactoryForBrowserProcess(),
          kVersion, kPrefixLength, kTimeout);
  return new PasswordRequirementsService(std::move(fetcher));
}

}  // namespace password_manager
