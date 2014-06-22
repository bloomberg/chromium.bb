// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/domain_reliability/service_factory.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/common/chrome_switches.h"
#include "components/domain_reliability/service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace domain_reliability {

namespace {

// Identifies Chrome as the source of Domain Reliability uploads it sends.
const char* kDomainReliabilityUploadReporterString = "chrome";

bool IsDomainReliabilityMonitoringEnabled() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableDomainReliability))
    return false;
  if (command_line->HasSwitch(switches::kEnableDomainReliability))
    return true;
  return base::FieldTrialList::FindFullName("DomRel-Enable") == "enable";
}

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
  if (!IsDomainReliabilityMonitoringEnabled())
    return NULL;

  return DomainReliabilityService::Create(
      kDomainReliabilityUploadReporterString);
}

}  // namespace domain_reliability
