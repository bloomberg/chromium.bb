// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/offscreen_presentation_manager_factory.h"

#include "base/lazy_instance.h"

#include "chrome/browser/media/router/offscreen_presentation_manager.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/web_contents.h"

namespace media_router {

namespace {

base::LazyInstance<OffscreenPresentationManagerFactory> service_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
OffscreenPresentationManager*
OffscreenPresentationManagerFactory::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  return OffscreenPresentationManagerFactory::GetOrCreateForBrowserContext(
      web_contents->GetBrowserContext());
}

// static
OffscreenPresentationManager*
OffscreenPresentationManagerFactory::GetOrCreateForBrowserContext(
    content::BrowserContext* context) {
  DCHECK(context);
  return static_cast<OffscreenPresentationManager*>(
      service_factory.Get().GetServiceForBrowserContext(context, true));
}

// static
OffscreenPresentationManagerFactory*
OffscreenPresentationManagerFactory::GetInstanceForTest() {
  return &service_factory.Get();
}

OffscreenPresentationManagerFactory::OffscreenPresentationManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "OffscreenPresentationManager",
          BrowserContextDependencyManager::GetInstance()) {}

OffscreenPresentationManagerFactory::~OffscreenPresentationManagerFactory() {}

content::BrowserContext*
OffscreenPresentationManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
KeyedService* OffscreenPresentationManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OffscreenPresentationManager;
}

}  // namespace media_router
