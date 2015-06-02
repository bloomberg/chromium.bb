// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_mojo_impl_factory.h"

#include "chrome/browser/media/router/media_router_mojo_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"

using content::BrowserContext;

namespace media_router {

namespace {

base::LazyInstance<MediaRouterMojoImplFactory> service_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
MediaRouterMojoImpl* MediaRouterMojoImplFactory::GetApiForBrowserContext(
    BrowserContext* context) {
  DCHECK(context);

  return static_cast<MediaRouterMojoImpl*>(
      service_factory.Get().GetServiceForBrowserContext(context, true));
}

MediaRouterMojoImplFactory::MediaRouterMojoImplFactory()
    : BrowserContextKeyedServiceFactory(
          "MediaRouterMojoImpl",
          BrowserContextDependencyManager::GetInstance()) {
  // MediaRouterMojoImpl depends on ProcessManager.
  DependsOn(extensions::ProcessManagerFactory::GetInstance());
}

MediaRouterMojoImplFactory::~MediaRouterMojoImplFactory() {
}

KeyedService* MediaRouterMojoImplFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new MediaRouterMojoImpl(extensions::ProcessManager::Get(context));
}

BrowserContext* MediaRouterMojoImplFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Always use the input context. This means that an incognito context will
  // have its own MediaRouterMojoImpl service, rather than sharing it with its
  // original non-incognito context.
  return context;
}

}  // namespace media_router
