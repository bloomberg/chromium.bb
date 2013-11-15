// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/dom_distiller_service_factory.h"

#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/dom_distiller/content/distiller_page_web_contents.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/dom_distiller_store.h"
#include "content/public/browser/browser_context.h"

namespace dom_distiller {

DomDistillerContextKeyedService::DomDistillerContextKeyedService(
    scoped_ptr<DomDistillerStoreInterface> store,
    scoped_ptr<DistillerFactory> distiller_factory)
    : DomDistillerService(store.Pass(), distiller_factory.Pass()) {}

// static
DomDistillerServiceFactory* DomDistillerServiceFactory::GetInstance() {
  return Singleton<DomDistillerServiceFactory>::get();
}

// static
DomDistillerContextKeyedService*
DomDistillerServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DomDistillerContextKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

DomDistillerServiceFactory::DomDistillerServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DomDistillerService",
          BrowserContextDependencyManager::GetInstance()) {}

DomDistillerServiceFactory::~DomDistillerServiceFactory() {}

BrowserContextKeyedService* DomDistillerServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  scoped_ptr<DomDistillerStoreInterface> dom_distiller_store;
  scoped_ptr<DistillerPageFactory> distiller_page_factory(
      new DistillerPageWebContentsFactory(profile));
  scoped_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory(
      new DistillerURLFetcherFactory(profile->GetRequestContext()));
  scoped_ptr<DistillerFactory> distiller_factory(new DistillerFactoryImpl(
      distiller_page_factory.Pass(), distiller_url_fetcher_factory.Pass()));
  return new DomDistillerContextKeyedService(dom_distiller_store.Pass(),
                                             distiller_factory.Pass());
}

content::BrowserContext* DomDistillerServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // TODO(cjhopman): Do we want this to be
  // GetBrowserContextRedirectedInIncognito? If so, find some way to use that in
  // components/.
  return context;
}

}  // namespace apps
