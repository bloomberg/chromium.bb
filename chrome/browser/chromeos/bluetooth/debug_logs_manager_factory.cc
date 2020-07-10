// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/debug_logs_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/bluetooth/debug_logs_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {

namespace bluetooth {

namespace {

// Wraps a DebugLogsManager instance in a KeyedService.
class DebugLogsManagerService : public KeyedService {
 public:
  explicit DebugLogsManagerService(Profile* profile)
      : debug_logs_manager_(chromeos::ProfileHelper::Get()
                                ->GetUserByProfile(profile)
                                ->GetDisplayEmail(),
                            profile->GetPrefs()) {}

  ~DebugLogsManagerService() override = default;

  DebugLogsManager* debug_logs_manager() { return &debug_logs_manager_; }

 private:
  DebugLogsManager debug_logs_manager_;
  DISALLOW_COPY_AND_ASSIGN(DebugLogsManagerService);
};

}  // namespace

// static
DebugLogsManager* DebugLogsManagerFactory::GetForProfile(Profile* profile) {
  if (!profile)
    return nullptr;

  DebugLogsManagerService* service = static_cast<DebugLogsManagerService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));

  return service ? service->debug_logs_manager() : nullptr;
}

// static
DebugLogsManagerFactory* DebugLogsManagerFactory::GetInstance() {
  return base::Singleton<DebugLogsManagerFactory>::get();
}

DebugLogsManagerFactory::DebugLogsManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "DebugLogsManagerFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

DebugLogsManagerFactory::~DebugLogsManagerFactory() = default;

KeyedService* DebugLogsManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // Only primary profiles have an associated logs manager.
  if (!chromeos::ProfileHelper::Get()->IsPrimaryProfile(profile))
    return nullptr;

  return new DebugLogsManagerService(profile);
}

bool DebugLogsManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool DebugLogsManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace bluetooth

}  // namespace chromeos
