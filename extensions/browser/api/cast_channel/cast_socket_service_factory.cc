// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_socket_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/api/cast_channel/cast_socket_service.h"

namespace extensions {
namespace api {
namespace cast_channel {

using content::BrowserContext;

namespace {

base::LazyInstance<CastSocketServiceFactory>::DestructorAtExit service_factory =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
CastSocketService* CastSocketServiceFactory::GetForBrowserContext(
    BrowserContext* context) {
  DCHECK(context);
  // GetServiceForBrowserContext returns a KeyedService hence the static_cast<>
  // to return a pointer to CastSocketService.
  return static_cast<CastSocketService*>(
      service_factory.Get().GetServiceForBrowserContext(context, true));
}

// static
CastSocketServiceFactory* CastSocketServiceFactory::GetInstance() {
  return &service_factory.Get();
}

CastSocketServiceFactory::CastSocketServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "CastSocketService",
          BrowserContextDependencyManager::GetInstance()) {}

CastSocketServiceFactory::~CastSocketServiceFactory() {}

content::BrowserContext* CastSocketServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

KeyedService* CastSocketServiceFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new CastSocketService();
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
