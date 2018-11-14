// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/domain_reliability/service_factory.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/chrome_switches.h"
#include "components/domain_reliability/service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif  // defined(OS_CHROMEOS)

namespace domain_reliability {

namespace {

// If Domain Reliability is enabled in the absence of a flag or field trial.
const bool kDefaultEnabled = true;

// The name and value of the field trial to turn Domain Reliability on.
const char kFieldTrialName[] = "DomRel-Enable";
const char kFieldTrialValueEnable[] = "enable";

// Identifies Chrome as the source of Domain Reliability uploads it sends.
const char kUploadReporterString[] = "chrome";

class KeyedServiceWrapper : public KeyedService {
 public:
  explicit KeyedServiceWrapper(DomainReliabilityService* service)
      : service_(service) {}
  ~KeyedServiceWrapper() override = default;

  DomainReliabilityService* service() { return service_; }

 private:
  DomainReliabilityService* service_;

  DISALLOW_COPY_AND_ASSIGN(KeyedServiceWrapper);
};

}  // namespace

// static
DomainReliabilityService*
DomainReliabilityServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  auto* wrapper = static_cast<KeyedServiceWrapper*>(
      GetInstance()->GetServiceForBrowserContext(context,
                                                 /* create = */ true));
  if (!wrapper)
    return nullptr;
  return wrapper->service();
}

// static
DomainReliabilityServiceFactory*
DomainReliabilityServiceFactory::GetInstance() {
  return base::Singleton<DomainReliabilityServiceFactory>::get();
}

DomainReliabilityServiceFactory::DomainReliabilityServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DomainReliabilityService",
          BrowserContextDependencyManager::GetInstance()) {}

DomainReliabilityServiceFactory::~DomainReliabilityServiceFactory() {}

KeyedService* DomainReliabilityServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!ShouldCreateService())
    return NULL;

  return new KeyedServiceWrapper(
      DomainReliabilityService::Create(kUploadReporterString));
}

bool DomainReliabilityServiceFactory::ShouldCreateService() const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableDomainReliability))
    return false;
  if (command_line->HasSwitch(switches::kEnableDomainReliability))
    return true;
  if (!ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled())
    return false;
  if (base::FieldTrialList::TrialExists(kFieldTrialName)) {
    std::string value = base::FieldTrialList::FindFullName(kFieldTrialName);
    return (value == kFieldTrialValueEnable);
  }
  return kDefaultEnabled;
}

}  // namespace domain_reliability
