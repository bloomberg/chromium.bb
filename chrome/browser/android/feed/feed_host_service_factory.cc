// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_host_service_factory.h"

#include "components/feed/core/feed_host_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace feed {

// static
FeedHostService* FeedHostServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FeedHostService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
FeedHostServiceFactory* FeedHostServiceFactory::GetInstance() {
  return base::Singleton<FeedHostServiceFactory>::get();
}

FeedHostServiceFactory::FeedHostServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FeedHostService",
          BrowserContextDependencyManager::GetInstance()) {}

FeedHostServiceFactory::~FeedHostServiceFactory() = default;

KeyedService* FeedHostServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FeedHostService();
}

content::BrowserContext* FeedHostServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context->IsOffTheRecord() ? nullptr : context;
}

}  // namespace feed
