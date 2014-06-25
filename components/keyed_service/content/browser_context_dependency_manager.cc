// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/content/browser_context_dependency_manager.h"

#include <algorithm>
#include <deque>
#include <iterator>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "components/keyed_service/content/browser_context_keyed_base_factory.h"
#include "content/public/browser/browser_context.h"

#ifndef NDEBUG
#include "base/command_line.h"
#include "base/file_util.h"

// Dumps dependency information about our browser context keyed services
// into a dot file in the browser context directory.
const char kDumpBrowserContextDependencyGraphFlag[] =
    "dump-browser-context-graph";
#endif  // NDEBUG

void BrowserContextDependencyManager::AddComponent(
    BrowserContextKeyedBaseFactory* component) {
  dependency_graph_.AddNode(component);
}

void BrowserContextDependencyManager::RemoveComponent(
    BrowserContextKeyedBaseFactory* component) {
  dependency_graph_.RemoveNode(component);
}

void BrowserContextDependencyManager::AddEdge(
    BrowserContextKeyedBaseFactory* depended,
    BrowserContextKeyedBaseFactory* dependee) {
  dependency_graph_.AddEdge(depended, dependee);
}

void BrowserContextDependencyManager::RegisterProfilePrefsForServices(
    const content::BrowserContext* context,
    user_prefs::PrefRegistrySyncable* pref_registry) {
  std::vector<DependencyNode*> construction_order;
  if (!dependency_graph_.GetConstructionOrder(&construction_order)) {
    NOTREACHED();
  }

  for (std::vector<DependencyNode*>::const_iterator it =
           construction_order.begin();
       it != construction_order.end();
       ++it) {
    BrowserContextKeyedBaseFactory* factory =
        static_cast<BrowserContextKeyedBaseFactory*>(*it);
    factory->RegisterProfilePrefsIfNecessaryForContext(context, pref_registry);
  }
}

void BrowserContextDependencyManager::CreateBrowserContextServices(
    content::BrowserContext* context) {
  DoCreateBrowserContextServices(context, false);
}

void BrowserContextDependencyManager::CreateBrowserContextServicesForTest(
    content::BrowserContext* context) {
  DoCreateBrowserContextServices(context, true);
}

void BrowserContextDependencyManager::DoCreateBrowserContextServices(
    content::BrowserContext* context,
    bool is_testing_context) {
  TRACE_EVENT0(
      "browser",
      "BrowserContextDependencyManager::DoCreateBrowserContextServices")
#ifndef NDEBUG
  MarkBrowserContextLiveForTesting(context);
#endif

  will_create_browser_context_services_callbacks_.Notify(context);

  std::vector<DependencyNode*> construction_order;
  if (!dependency_graph_.GetConstructionOrder(&construction_order)) {
    NOTREACHED();
  }

#ifndef NDEBUG
  DumpBrowserContextDependencies(context);
#endif

  for (size_t i = 0; i < construction_order.size(); i++) {
    BrowserContextKeyedBaseFactory* factory =
        static_cast<BrowserContextKeyedBaseFactory*>(construction_order[i]);
    if (is_testing_context && factory->ServiceIsNULLWhileTesting() &&
        !factory->HasTestingFactory(context)) {
      factory->SetEmptyTestingFactory(context);
    } else if (factory->ServiceIsCreatedWithBrowserContext()) {
      // Create the service.
      factory->CreateServiceNow(context);
    }
  }
}

void BrowserContextDependencyManager::DestroyBrowserContextServices(
    content::BrowserContext* context) {
  std::vector<DependencyNode*> destruction_order;
  if (!dependency_graph_.GetDestructionOrder(&destruction_order)) {
    NOTREACHED();
  }

#ifndef NDEBUG
  DumpBrowserContextDependencies(context);
#endif

  for (size_t i = 0; i < destruction_order.size(); i++) {
    BrowserContextKeyedBaseFactory* factory =
        static_cast<BrowserContextKeyedBaseFactory*>(destruction_order[i]);
    factory->BrowserContextShutdown(context);
  }

#ifndef NDEBUG
  // The context is now dead to the rest of the program.
  dead_context_pointers_.insert(context);
#endif

  for (size_t i = 0; i < destruction_order.size(); i++) {
    BrowserContextKeyedBaseFactory* factory =
        static_cast<BrowserContextKeyedBaseFactory*>(destruction_order[i]);
    factory->BrowserContextDestroyed(context);
  }
}

scoped_ptr<base::CallbackList<void(content::BrowserContext*)>::Subscription>
BrowserContextDependencyManager::
RegisterWillCreateBrowserContextServicesCallbackForTesting(
    const base::Callback<void(content::BrowserContext*)>& callback) {
  return will_create_browser_context_services_callbacks_.Add(callback);
}

#ifndef NDEBUG
void BrowserContextDependencyManager::AssertBrowserContextWasntDestroyed(
    content::BrowserContext* context) {
  if (dead_context_pointers_.find(context) != dead_context_pointers_.end()) {
    NOTREACHED() << "Attempted to access a BrowserContext that was ShutDown(). "
                 << "This is most likely a heap smasher in progress. After "
                 << "KeyedService::Shutdown() completes, your "
                 << "service MUST NOT refer to depended BrowserContext "
                 << "services again.";
  }
}

void BrowserContextDependencyManager::MarkBrowserContextLiveForTesting(
    content::BrowserContext* context) {
  dead_context_pointers_.erase(context);
}
#endif

// static
BrowserContextDependencyManager*
BrowserContextDependencyManager::GetInstance() {
  return Singleton<BrowserContextDependencyManager>::get();
}

BrowserContextDependencyManager::BrowserContextDependencyManager() {}

BrowserContextDependencyManager::~BrowserContextDependencyManager() {}

#ifndef NDEBUG
namespace {

std::string BrowserContextKeyedBaseFactoryGetNodeName(DependencyNode* node) {
  return static_cast<BrowserContextKeyedBaseFactory*>(node)->name();
}

}  // namespace

void BrowserContextDependencyManager::DumpBrowserContextDependencies(
    content::BrowserContext* context) {
  // Whenever we try to build a destruction ordering, we should also dump a
  // dependency graph to "/path/to/context/context-dependencies.dot".
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          kDumpBrowserContextDependencyGraphFlag)) {
    base::FilePath dot_file =
        context->GetPath().AppendASCII("browser-context-dependencies.dot");
    std::string contents = dependency_graph_.DumpAsGraphviz(
        "BrowserContext",
        base::Bind(&BrowserContextKeyedBaseFactoryGetNodeName));
    base::WriteFile(dot_file, contents.c_str(), contents.size());
  }
}
#endif  // NDEBUG
