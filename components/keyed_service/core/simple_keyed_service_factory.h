// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_SIMPLE_KEYED_SERVICE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CORE_SIMPLE_KEYED_SERVICE_FACTORY_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service_export.h"
#include "components/keyed_service/core/keyed_service_factory.h"

class KeyedService;
class PrefService;
class SimpleDependencyManager;
class SimpleFactoryKey;

// Base class for Factories that take a SimpleFactoryKey object and return some
// service on a one-to-one mapping. Each factory that derives from this class
// *must* be a Singleton (only unit tests don't do that).
//
// We do this because services depend on each other and we need to control
// shutdown/destruction order. In each derived classes' constructors, the
// implementors must explicitly state on which services they depend.
class KEYED_SERVICE_EXPORT SimpleKeyedServiceFactory
    : public KeyedServiceFactory {
 public:
  // A callback that supplies the instance of a KeyedService for a given
  // SimpleFactoryKey. This is used primarily for testing, where we want to feed
  // a specific test double into the SKSF system.
  using TestingFactory = base::RepeatingCallback<std::unique_ptr<KeyedService>(
      SimpleFactoryKey* key)>;

  // Associates |testing_factory| with |key| so that |testing_factory| is
  // used to create the KeyedService when requested.  |testing_factory| can be
  // empty to signal that KeyedService should be null. Multiple calls to
  // SetTestingFactory() are allowed; previous services will be shut down.
  void SetTestingFactory(SimpleFactoryKey* key, TestingFactory testing_factory);

  // Associates |testing_factory| with |key| and immediately returns the
  // created KeyedService. Since the factory will be used immediately, it may
  // not be empty.
  KeyedService* SetTestingFactoryAndUse(SimpleFactoryKey* key,
                                        PrefService* prefs,
                                        TestingFactory testing_factory);

 protected:
  SimpleKeyedServiceFactory(const char* name, SimpleDependencyManager* manager);
  ~SimpleKeyedServiceFactory() override;

  // Common implementation that maps |key| to some service object. Deals
  // with incognito contexts per subclass instructions with
  // GetBrowserContextRedirectedInIncognito() and
  // GetBrowserContextOwnInstanceInIncognito() through the
  // GetBrowserContextToUse() method on the base.  If |create| is true, the
  // service will be created using BuildServiceInstanceFor() if it doesn't
  // already exist.
  KeyedService* GetServiceForKey(SimpleFactoryKey* key,
                                 PrefService* prefs,
                                 bool create);

  // Interface for people building a concrete FooServiceFactory: --------------

  // Finds which SimpleFactoryKey (if any) to use.
  virtual SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const;

  // Interface for people building a type of SimpleKeyedFactory: -------

  // All subclasses of SimpleKeyedServiceFactory must return a
  // KeyedService.
  virtual std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key,
      PrefService* prefs) const = 0;

  // A helper object actually listens for notifications about BrowserContext
  // destruction, calculates the order in which things are destroyed and then
  // does a two pass shutdown.
  //
  // First, SimpleContextShutdown() is called on every ServiceFactory and will
  // usually call KeyedService::Shutdown(), which gives each
  // KeyedService a chance to remove dependencies on other
  // services that it may be holding.
  //
  // Secondly, SimpleContextDestroyed() is called on every ServiceFactory
  // and the default implementation removes it from |mapping_| and deletes
  // the pointer.
  virtual void SimpleContextShutdown(SimpleFactoryKey* key);
  virtual void SimpleContextDestroyed(SimpleFactoryKey* key);

 private:
  // Registers any user preferences on this service. This is called by
  // RegisterPrefsIfNecessaryForContext() and should be overriden by any service
  // that wants to register profile-specific preferences.
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) {}

  // KeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      void* context,
      void* side_parameter) const final;
  bool IsOffTheRecord(void* context) const final;

  // KeyedServiceBaseFactory:
  void* GetContextToUse(void* context) const final;
  bool ServiceIsCreatedWithContext() const final;
  void ContextShutdown(void* context) final;
  void ContextDestroyed(void* context) final;
  void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) final;
  void SetEmptyTestingFactory(void* context) final;
  bool HasTestingFactory(void* context) final;
  void CreateServiceNow(void* context) final;

  DISALLOW_COPY_AND_ASSIGN(SimpleKeyedServiceFactory);
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_SIMPLE_KEYED_SERVICE_FACTORY_H_
