// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/network_time/network_time_service_factory.h"

#include "chrome/browser/network_time/network_time_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

NetworkTimeServiceFactory::NetworkTimeServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "NetworkTimeService",
          BrowserContextDependencyManager::GetInstance()) {
}

NetworkTimeServiceFactory::~NetworkTimeServiceFactory() {
}

// static
NetworkTimeServiceFactory* NetworkTimeServiceFactory::GetInstance() {
  return Singleton<NetworkTimeServiceFactory>::get();
}

// static
NetworkTimeService* NetworkTimeServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<NetworkTimeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

content::BrowserContext* NetworkTimeServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

BrowserContextKeyedService*
NetworkTimeServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new NetworkTimeService(static_cast<Profile*>(context));
}
