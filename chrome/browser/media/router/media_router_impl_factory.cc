// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_impl_factory.h"

#include "chrome/browser/media/router/media_router_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

using content::BrowserContext;

namespace media_router {

// static
MediaRouterImpl* MediaRouterImplFactory::GetMediaRouterForBrowserContext(
    BrowserContext* context) {
  DCHECK(context);
  return static_cast<MediaRouterImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
MediaRouterImplFactory* MediaRouterImplFactory::GetInstance() {
  return Singleton<MediaRouterImplFactory>::get();
}

MediaRouterImplFactory::MediaRouterImplFactory()
    : BrowserContextKeyedServiceFactory(
          "MediaRouterImpl",
          BrowserContextDependencyManager::GetInstance()) {
}

MediaRouterImplFactory::~MediaRouterImplFactory() {
}

KeyedService* MediaRouterImplFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new MediaRouterImpl;
}

BrowserContext* MediaRouterImplFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // This means MediaRouterImpl is available in incognito contexts as well.
  return context;
}

}  // namespace media_router
