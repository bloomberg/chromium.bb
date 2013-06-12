// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_CONTEXT_KEYED_SERVICE_BROWSER_CONTEXT_DEPENDENCY_MANAGER_H_
#define COMPONENTS_BROWSER_CONTEXT_KEYED_SERVICE_BROWSER_CONTEXT_DEPENDENCY_MANAGER_H_

#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_export.h"
#include "components/browser_context_keyed_service/dependency_graph.h"

#ifndef NDEBUG
#include <set>
#endif

class BrowserContextKeyedBaseFactory;

namespace content {
class BrowserContext;
}

// A singleton that listens for context destruction notifications and
// rebroadcasts them to each BrowserContextKeyedBaseFactory in a safe order
// based on the stated dependencies by each service.
class BROWSER_CONTEXT_KEYED_SERVICE_EXPORT BrowserContextDependencyManager {
 public:
  // Adds/Removes a component from our list of live components. Removing will
  // also remove live dependency links.
  void AddComponent(BrowserContextKeyedBaseFactory* component);
  void RemoveComponent(BrowserContextKeyedBaseFactory* component);

  // Adds a dependency between two factories.
  void AddEdge(BrowserContextKeyedBaseFactory* depended,
               BrowserContextKeyedBaseFactory* dependee);

  // Called by each BrowserContext to alert us of its creation. Several services
  // want to be started when a context is created. Testing configuration is also
  // done at this time. (If you want your BrowserContextKeyedService to be
  // started with the BrowserContext, override BrowserContextKeyedBaseFactory::
  // ServiceIsCreatedWithBrowserContext() to return true.)
  void CreateBrowserContextServices(content::BrowserContext* context,
                                    bool is_testing_context);

  // Called by each BrowserContext to alert us that we should destroy services
  // associated with it.
  void DestroyBrowserContextServices(content::BrowserContext* context);

#ifndef NDEBUG
  // Debugging assertion called as part of GetServiceForBrowserContext in debug
  // mode. This will NOTREACHED() whenever the user is trying to access a stale
  // BrowserContext*.
  void AssertBrowserContextWasntDestroyed(content::BrowserContext* context);
#endif

  static BrowserContextDependencyManager* GetInstance();

 private:
  friend class BrowserContextDependencyManagerUnittests;
  friend struct DefaultSingletonTraits<BrowserContextDependencyManager>;

  BrowserContextDependencyManager();
  virtual ~BrowserContextDependencyManager();

#ifndef NDEBUG
  void DumpBrowserContextDependencies(content::BrowserContext* context);
#endif

  DependencyGraph dependency_graph_;

#ifndef NDEBUG
  // A list of context objects that have gone through the Shutdown()
  // phase. These pointers are most likely invalid, but we keep track of their
  // locations in memory so we can nicely assert if we're asked to do anything
  // with them.
  std::set<content::BrowserContext*> dead_context_pointers_;
#endif
};

#endif  // COMPONENTS_BROWSER_CONTEXT_KEYED_SERVICE_BROWSER_CONTEXT_DEPENDENCY_MANAGER_H_
