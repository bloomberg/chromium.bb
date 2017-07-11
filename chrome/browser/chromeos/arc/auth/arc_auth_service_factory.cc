// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/auth/arc_auth_service_factory.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace arc {

ArcAuthService* ArcAuthServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ArcAuthService*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

// static
ArcAuthServiceFactory* ArcAuthServiceFactory::GetInstance() {
  return base::Singleton<ArcAuthServiceFactory>::get();
}

ArcAuthServiceFactory::ArcAuthServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ArcAuthServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

ArcAuthServiceFactory::~ArcAuthServiceFactory() = default;

KeyedService* ArcAuthServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* arc_service_manager = arc::ArcServiceManager::Get();

  // Practically, this is in testing case.
  if (!arc_service_manager) {
    VLOG(2) << "ArcServiceManager is not available.";
    return nullptr;
  }

  if (arc_service_manager->browser_context() != context) {
    VLOG(2) << "Non ARC allowed browser context.";
    return nullptr;
  }

  return new ArcAuthService(Profile::FromBrowserContext(context),
                            arc_service_manager->arc_bridge_service());
}

}  // namespace arc
