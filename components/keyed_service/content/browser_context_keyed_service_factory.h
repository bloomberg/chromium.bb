// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/keyed_service/content/browser_context_keyed_base_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/keyed_service_export.h"

class BrowserContextDependencyManager;
class KeyedService;

// Base class for Factories that take a BrowserContext object and return some
// service on a one-to-one mapping. Each factory that derives from this class
// *must* be a Singleton (only unit tests don't do that).
//
// We do this because services depend on each other and we need to control
// shutdown/destruction order. In each derived classes' constructors, the
// implementors must explicitly state which services are depended on.
class KEYED_SERVICE_EXPORT BrowserContextKeyedServiceFactory
    : public BrowserContextKeyedBaseFactory {
 public:
  // A function that supplies the instance of a KeyedService for a given
  // BrowserContext. This is used primarily for testing, where we want to feed
  // a specific mock into the BCKSF system.
  typedef KeyedService* (*TestingFactoryFunction)(
      content::BrowserContext* context);

  // Associates |factory| with |context| so that |factory| is used to create
  // the KeyedService when requested.  |factory| can be NULL to signal that
  // KeyedService should be NULL. Multiple calls to SetTestingFactory() are
  // allowed; previous services will be shut down.
  void SetTestingFactory(content::BrowserContext* context,
                         TestingFactoryFunction factory);

  // Associates |factory| with |context| and immediately returns the created
  // KeyedService. Since the factory will be used immediately, it may not be
  // NULL.
  KeyedService* SetTestingFactoryAndUse(content::BrowserContext* context,
                                        TestingFactoryFunction factory);

 protected:
  // BrowserContextKeyedServiceFactories must communicate with a
  // BrowserContextDependencyManager. For all non-test code, write your subclass
  // constructors like this:
  //
  //   MyServiceFactory::MyServiceFactory()
  //     : BrowserContextKeyedServiceFactory(
  //         "MyService",
  //         BrowserContextDependencyManager::GetInstance())
  //   {}
  BrowserContextKeyedServiceFactory(const char* name,
                                    BrowserContextDependencyManager* manager);
  virtual ~BrowserContextKeyedServiceFactory();

  // Common implementation that maps |context| to some service object. Deals
  // with incognito contexts per subclass instructions with
  // GetBrowserContextRedirectedInIncognito() and
  // GetBrowserContextOwnInstanceInIncognito() through the
  // GetBrowserContextToUse() method on the base.  If |create| is true, the
  // service will be created using BuildServiceInstanceFor() if it doesn't
  // already exist.
  KeyedService* GetServiceForBrowserContext(content::BrowserContext* context,
                                            bool create);

  // Maps |context| to |service| with debug checks to prevent duplication.
  void Associate(content::BrowserContext* context, KeyedService* service);

  // Removes the mapping from |context| to a service.
  void Disassociate(content::BrowserContext* context);

  // All subclasses of BrowserContextKeyedServiceFactory must return a
  // KeyedService instead of just a BrowserContextKeyedBase.
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const = 0;

  // A helper object actually listens for notifications about BrowserContext
  // destruction, calculates the order in which things are destroyed and then
  // does a two pass shutdown.
  //
  // First, BrowserContextShutdown() is called on every ServiceFactory and will
  // usually call KeyedService::Shutdown(), which gives each
  // KeyedService a chance to remove dependencies on other
  // services that it may be holding.
  //
  // Secondly, BrowserContextDestroyed() is called on every ServiceFactory
  // and the default implementation removes it from |mapping_| and deletes
  // the pointer.
  virtual void BrowserContextShutdown(content::BrowserContext* context)
      OVERRIDE;
  virtual void BrowserContextDestroyed(content::BrowserContext* context)
      OVERRIDE;

  virtual void SetEmptyTestingFactory(content::BrowserContext* context)
      OVERRIDE;
  virtual bool HasTestingFactory(content::BrowserContext* context) OVERRIDE;
  virtual void CreateServiceNow(content::BrowserContext* context) OVERRIDE;

 private:
  friend class BrowserContextDependencyManager;
  friend class BrowserContextDependencyManagerUnittests;

  typedef std::map<content::BrowserContext*, KeyedService*>
      BrowserContextKeyedServices;
  typedef std::map<content::BrowserContext*, TestingFactoryFunction>
      BrowserContextOverriddenTestingFunctions;

  // The mapping between a BrowserContext and its service.
  BrowserContextKeyedServices mapping_;

  // The mapping between a BrowserContext and its overridden
  // TestingFactoryFunction.
  BrowserContextOverriddenTestingFunctions testing_factories_;

  DISALLOW_COPY_AND_ASSIGN(BrowserContextKeyedServiceFactory);
};

#endif  // COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_
