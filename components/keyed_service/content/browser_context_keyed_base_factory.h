// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_KEYED_BASE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_KEYED_BASE_FACTORY_H_

#include <set>

#include "base/threading/non_thread_safe.h"
#include "components/keyed_service/core/dependency_node.h"
#include "components/keyed_service/core/keyed_service_export.h"

class BrowserContextDependencyManager;
class PrefService;

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Base class for Factories that take a BrowserContext object and return some
// service.
//
// Unless you're trying to make a new type of Factory, you probably don't want
// this class, but its subclasses: BrowserContextKeyedServiceFactory and
// RefcountedBrowserContextKeyedServiceFactory. This object describes general
// dependency management between Factories; subclasses react to lifecycle
// events and implement memory management.
class KEYED_SERVICE_EXPORT BrowserContextKeyedBaseFactory
    : public base::NonThreadSafe,
      NON_EXPORTED_BASE(public DependencyNode) {
 public:
  // Registers preferences used in this service on the pref service of
  // |context|. This is the public interface and is safe to be called multiple
  // times because testing code can have multiple services of the same type
  // attached to a single |context|. Only test code is allowed to call this
  // method.
  // TODO(gab): This method can be removed entirely when
  // PrefService::DeprecatedGetPrefRegistry() is phased out.
  void RegisterUserPrefsOnBrowserContextForTest(
      content::BrowserContext* context);

#ifndef NDEBUG
  // Returns our name. We don't keep track of this in release mode.
  const char* name() const { return service_name_; }
#endif

 protected:
  BrowserContextKeyedBaseFactory(const char* name,
                                 BrowserContextDependencyManager* manager);
  virtual ~BrowserContextKeyedBaseFactory();

  // The main public interface for declaring dependencies between services
  // created by factories.
  void DependsOn(BrowserContextKeyedBaseFactory* rhs);

  // Calls RegisterProfilePrefs() after doing house keeping required to work
  // alongside RegisterUserPrefsOnBrowserContextForTest().
  // TODO(gab): This method can be replaced by RegisterProfilePrefs() directly
  // once RegisterUserPrefsOnBrowserContextForTest() is phased out.
  void RegisterProfilePrefsIfNecessaryForContext(
      const content::BrowserContext* context,
      user_prefs::PrefRegistrySyncable* registry);

  // Interface for people building a concrete FooServiceFactory: --------------

  // Finds which browser context (if any) to use.
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const;

  // By default, we create instances of a service lazily and wait until
  // GetForBrowserContext() is called on our subclass. Some services need to be
  // created as soon as the BrowserContext has been brought up.
  virtual bool ServiceIsCreatedWithBrowserContext() const;

  // By default, TestingBrowserContexts will be treated like normal contexts.
  // You can override this so that by default, the service associated with the
  // TestingBrowserContext is NULL. (This is just a shortcut around
  // SetTestingFactory() to make sure our contexts don't directly refer to the
  // services they use.)
  virtual bool ServiceIsNULLWhileTesting() const;

  // Interface for people building a type of BrowserContextKeyedFactory: -------

  // A helper object actually listens for notifications about BrowserContext
  // destruction, calculates the order in which things are destroyed and then
  // does a two pass shutdown.
  //
  // It is up to the individual factory types to determine what this two pass
  // shutdown means. The general framework guarantees the following:
  //
  // - Each BrowserContextShutdown() is called in dependency order (and you may
  //   reach out to other services during this phase).
  //
  // - Each BrowserContextDestroyed() is called in dependency order. We will
  //   NOTREACHED() if you attempt to GetForBrowserContext() any other service.
  //   You should delete/deref/do other final memory management things during
  //   this phase. You must also call the base class method as the last thing
  //   you do.
  virtual void BrowserContextShutdown(content::BrowserContext* context) = 0;
  virtual void BrowserContextDestroyed(content::BrowserContext* context);

  // Returns whether we've registered the preferences on this context.
  bool ArePreferencesSetOn(content::BrowserContext* context) const;

  // Mark context as Preferences set.
  void MarkPreferencesSetOn(content::BrowserContext* context);

  // Which BrowserContextDependencyManager we should communicate with.
  // In real code, this will always be
  // BrowserContextDependencyManager::GetInstance(), but unit tests will want
  // to use their own copy.
  BrowserContextDependencyManager* dependency_manager_;

 private:
  friend class BrowserContextDependencyManager;
  friend class BrowserContextDependencyManagerUnittests;

  // Registers any user preferences on this service. This is called by
  // RegisterProfilePrefsIfNecessary() and should be overriden by any service
  // that wants to register profile-specific preferences.
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) {}

  // These two methods are for tight integration with the
  // BrowserContextDependencyManager.

  // Because of ServiceIsNULLWhileTesting(), we need a way to tell different
  // subclasses that they should disable testing.
  virtual void SetEmptyTestingFactory(content::BrowserContext* context) = 0;

  // Returns true if a testing factory function has been set for |context|.
  virtual bool HasTestingFactory(content::BrowserContext* context) = 0;

  // We also need a generalized, non-returning method that generates the object
  // now for when we're creating the context.
  virtual void CreateServiceNow(content::BrowserContext* context) = 0;

  // BrowserContexts that have this service's preferences registered on them.
  std::set<const content::BrowserContext*> registered_preferences_;

#if !defined(NDEBUG)
  // A static string passed in to our constructor. Should be unique across all
  // services. This is used only for debugging in debug mode. (We can print
  // pretty graphs with GraphViz with this information.)
  const char* service_name_;
#endif
};

#endif  // COMPONENTS_KEYED_SERVICE_CONTENT_BROWSER_CONTEXT_KEYED_BASE_FACTORY_H_
