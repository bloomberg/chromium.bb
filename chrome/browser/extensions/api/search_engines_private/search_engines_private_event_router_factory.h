// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SEARCH_ENGINES_PRIVATE_SEARCH_ENGINES_PRIVATE_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_SEARCH_ENGINES_PRIVATE_SEARCH_ENGINES_PRIVATE_EVENT_ROUTER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class SearchEnginesPrivateEventRouter;

// This is a factory class used by the BrowserContextDependencyManager
// to instantiate the searchEnginesPrivate event router per profile (since the
// extension event router is per profile).
class SearchEnginesPrivateEventRouterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the SearchEnginesPrivateEventRouter for |profile|, creating it if
  // it is not yet created.
  static SearchEnginesPrivateEventRouter* GetForProfile(
      content::BrowserContext* context);

  // Returns the SearchEnginesPrivateEventRouterFactory instance.
  static SearchEnginesPrivateEventRouterFactory* GetInstance();

 protected:
  // BrowserContextKeyedBaseFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend struct base::DefaultSingletonTraits<
      SearchEnginesPrivateEventRouterFactory>;

  SearchEnginesPrivateEventRouterFactory();
  ~SearchEnginesPrivateEventRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(SearchEnginesPrivateEventRouterFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SEARCH_ENGINES_PRIVATE_SEARCH_ENGINES_PRIVATE_EVENT_ROUTER_FACTORY_H_
