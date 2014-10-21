// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CONTENT_REFCOUNTED_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CONTENT_REFCOUNTED_BROWSER_CONTEXT_KEYED_SERVICE_FACTORY_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/content/browser_context_keyed_base_factory.h"
#include "components/keyed_service/core/keyed_service_export.h"

class RefcountedKeyedService;

namespace content {
class BrowserContext;
}

// A specialized BrowserContextKeyedServiceFactory that manages a
// RefcountedThreadSafe<>.
//
// While the factory returns RefcountedThreadSafe<>s, the factory itself is a
// base::NotThreadSafe. Only call methods on this object on the UI thread.
//
// Implementers of RefcountedKeyedService should note that we guarantee that
// ShutdownOnUIThread() is called on the UI thread, but actual object
// destruction can happen anywhere.
class KEYED_SERVICE_EXPORT RefcountedBrowserContextKeyedServiceFactory
    : public BrowserContextKeyedBaseFactory {
 public:
  // A function that supplies the instance of a KeyedService for a given
  // BrowserContext. This is used primarily for testing, where we want to feed
  // a specific mock into the BCKSF system.
  typedef scoped_refptr<RefcountedKeyedService>(*TestingFactoryFunction)(
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
  scoped_refptr<RefcountedKeyedService> SetTestingFactoryAndUse(
      content::BrowserContext* context,
      TestingFactoryFunction factory);

 protected:
  RefcountedBrowserContextKeyedServiceFactory(
      const char* name,
      BrowserContextDependencyManager* manager);
  ~RefcountedBrowserContextKeyedServiceFactory() override;

  scoped_refptr<RefcountedKeyedService> GetServiceForBrowserContext(
      content::BrowserContext* context,
      bool create);

  // Maps |context| to |service| with debug checks to prevent duplication.
  void Associate(content::BrowserContext* context,
                 const scoped_refptr<RefcountedKeyedService>& service);

  // All subclasses of RefcountedBrowserContextKeyedServiceFactory must return
  // a RefcountedKeyedService instead of just
  // a BrowserContextKeyedBase.
  virtual scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const = 0;

  void BrowserContextShutdown(content::BrowserContext* context) override;
  void BrowserContextDestroyed(content::BrowserContext* context) override;
  void SetEmptyTestingFactory(content::BrowserContext* context) override;
  bool HasTestingFactory(content::BrowserContext* context) override;
  void CreateServiceNow(content::BrowserContext* context) override;

 private:
  typedef std::map<content::BrowserContext*,
                   scoped_refptr<RefcountedKeyedService>> RefCountedStorage;
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
