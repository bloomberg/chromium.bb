// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#if defined(OS_ANDROID)
#include "chrome/browser/media/android/router/media_router_android.h"
#else
#include "chrome/browser/media/router/media_router_mojo_impl.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#endif

using content::BrowserContext;

namespace media_router {

namespace {

base::LazyInstance<MediaRouterFactory> service_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
MediaRouter* MediaRouterFactory::GetApiForBrowserContext(
    BrowserContext* context) {
  DCHECK(context);
  // GetServiceForBrowserContext returns a KeyedService hence the static_cast<>
  // to return a pointer to MediaRouter.
  return static_cast<MediaRouter*>(
      service_factory.Get().GetServiceForBrowserContext(context, true));
}

MediaRouterFactory::MediaRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "MediaRouter",
          BrowserContextDependencyManager::GetInstance()) {
#if !defined(OS_ANDROID)
  // On desktop platforms, MediaRouter depends on ProcessManager.
  DependsOn(extensions::ProcessManagerFactory::GetInstance());
#endif
}

MediaRouterFactory::~MediaRouterFactory() {
}

KeyedService* MediaRouterFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
#if defined(OS_ANDROID)
  return new MediaRouterAndroid(context);
#else
  return new MediaRouterMojoImpl(extensions::ProcessManager::Get(context));
#endif
}

}  // namespace media_router
