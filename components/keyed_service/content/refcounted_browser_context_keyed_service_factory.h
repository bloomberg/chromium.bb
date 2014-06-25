// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CONTENT_REFCOUNTED_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CONTENT_REFCOUNTED_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service.h"
#include "components/keyed_service/core/keyed_service_export.h"

class RefcountedBrowserContextKeyedService;

namespace content {
class BrowserContext;
}

// A specialized BrowserContextKeyedServiceFactory that manages a
// RefcountedThreadSafe<>.
//
// While the factory returns RefcountedThreadSafe<>s, the factory itself is a
// base::NotThreadSafe. Only call methods on this object on the UI thread.
//
// Implementers of RefcountedBrowserContextKeyedService should note that
// we guarantee that ShutdownOnUIThread() is called on the UI thread, but actual
// object destruction can happen anywhere.
class KEYED_SERVICE_EXPORT RefcountedBrowserContextKeyedServiceFactory
    : public BrowserContextKeyedBaseFactory {
 public:
  // A function that supplies the instance of a KeyedService for a given
  // BrowserContext. This is used primarily for testing, where we want to feed
  // a specific mock into the BCKSF system.
  typedef scoped_refptr<RefcountedBrowserContextKeyedService>(
      *TestingFactoryFunction)(content::BrowserContext* context);

  // Associates |factory| with |context| so that |factory| is used to create
  // the KeyedService when requested.  |factory| can be NULL to signal that
  // KeyedService should be NULL. Multiple calls to SetTestingFactory() are
  // allowed; previous services will be shut down.
  void SetTestingFactory(content::BrowserContext* context,
                         TestingFactoryFunction factory);

  // Associates |factory| with |context| and immediately returns the created
  // KeyedService. Since the factory will be used immediately, it may not be
  // NULL.
  scoped_refptr<RefcountedBrowserContextKeyedService> SetTestingFactoryAndUse(
      content::BrowserContext* context,
      TestingFactoryFunction factory);

 protected:
  RefcountedBrowserContextKeyedServiceFactory(
      const char* name,
      BrowserContextDependencyManager* manager);
  virtual ~RefcountedBrowserContextKeyedServiceFactory();

  scoped_refptr<RefcountedBrowserContextKeyedService>
      GetServiceForBrowserContext(content::BrowserContext* context,
                                  bool create);

  // Maps |context| to |service| with debug checks to prevent duplication.
  void Associate(
      content::BrowserContext* context,
      const scoped_refptr<RefcountedBrowserContextKeyedService>& service);

  // All subclasses of RefcountedBrowserContextKeyedServiceFactory must return
  // a RefcountedBrowserContextKeyedService instead of just
  // a BrowserContextKeyedBase.
  virtual scoped_refptr<RefcountedBrowserContextKeyedService>
      BuildServiceInstanceFor(content::BrowserContext* context) const = 0;

  virtual void BrowserContextShutdown(content::BrowserContext* context)
      OVERRIDE;
  virtual void BrowserContextDestroyed(content::BrowserContext* context)
      OVERRIDE;
  virtual void SetEmptyTestingFactory(content::BrowserContext* context)
      OVERRIDE;
  virtual bool HasTestingFactory(content::BrowserContext* context) OVERRIDE;
  virtual void CreateServiceNow(content::BrowserContext* context) OVERRIDE;

 private:
  typedef std::map<content::BrowserContext*,
                   scoped_refptr<RefcountedBrowserContextKeyedService> >
      RefCountedStorage;
  typedef std::map<content::BrowserContext*, TestingFactoryFunction>
      BrowserContextOverriddenTestingFunctions;

  // The mapping between a BrowserContext and its refcounted service.
  RefCountedStorage mapping_;

  // The mapping between a BrowserContext and its overridden
  // TestingFactoryFunction.
  BrowserContextOverriddenTestingFunctions testing_factories_;

  DISALLOW_COPY_AND_ASSIGN(RefcountedBrowserContextKeyedServiceFactory);
};

#endif  // COMPONENTS_KEYED_SERVICE_CONTENT_REFCOUNTED_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_
