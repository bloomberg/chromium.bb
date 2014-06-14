// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/domain_reliability/service_factory.h"

#include "components/domain_reliability/service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace domain_reliability {

namespace {

// Identifies Chrome as the source of Domain Reliability uploads it sends.
const char* kDomainReliabilityUploadReporterString = "chrome";

}  // namespace

// static
DomainReliabilityService*
DomainReliabilityServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DomainReliabilityService*>(
      GetInstance()->GetServiceForBrowserContext(context,
                                                 /* create = */ true));
}

// static
DomainReliabilityServiceFactory*
DomainReliabilityServiceFactory::GetInstance() {
  return Singleton<DomainReliabilityServiceFactory>::get();
}

DomainReliabilityServiceFactory::DomainReliabilityServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DomainReliabilityService",
          BrowserContextDependencyManager::GetInstance()) {}

DomainReliabilityServiceFactory::~DomainReliabilityServiceFactory() {}

KeyedService* DomainReliabilityServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // TODO(ttuttle): Remove when we start using the factory.
  NOTREACHED();

  return DomainReliabilityService::Create(
      kDomainReliabilityUploadReporterString);
}

}  // namespace domain_reliability
