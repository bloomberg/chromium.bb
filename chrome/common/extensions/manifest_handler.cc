// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handler.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/stl_util.h"
#include "chrome/common/extensions/manifest.h"

namespace extensions {

namespace {

class ManifestHandlerRegistry {
 public:
  ManifestHandlerRegistry() : is_sorted_(false) {
  }

  void RegisterManifestHandler(const std::string& key,
                               linked_ptr<ManifestHandler> handler);
  bool ParseExtension(Extension* extension, string16* error);
  void ClearForTesting();

 private:
  friend struct base::DefaultLazyInstanceTraits<ManifestHandlerRegistry>;
  typedef std::map<std::string, linked_ptr<ManifestHandler> >
      ManifestHandlerMap;
  typedef std::map<ManifestHandler*, int> ManifestHandlerPriorityMap;

  // Puts the manifest handlers in order such that each handler comes after
  // any handlers for their PrerequisiteKeys. If there is no handler for
  // a prerequisite key, that dependency is simply ignored.
  // CHECKs that there are no manifest handlers with circular dependencies.
  void SortManifestHandlers();

  // All registered manifest handlers.
  ManifestHandlerMap handlers_;

  // The priority for each manifest handler. Handlers with lower priority
  // values are evaluated first.
  ManifestHandlerPriorityMap priority_map_;
  bool is_sorted_;
};

void ManifestHandlerRegistry::RegisterManifestHandler(
    const std::string& key, linked_ptr<ManifestHandler> handler) {
  handlers_[key] = handler;
  is_sorted_ = false;
}

bool ManifestHandlerRegistry::ParseExtension(Extension* extension,
                                             string16* error) {
  SortManifestHandlers();
  std::map<int, ManifestHandler*> handlers_by_priority;
  for (ManifestHandlerMap::iterator iter = handlers_.begin();
       iter != handlers_.end(); ++iter) {
    ManifestHandler* handler = iter->second.get();
    if (extension->manifest()->HasPath(iter->first) ||
        handler->AlwaysParseForType(extension->GetType())) {
      handlers_by_priority[priority_map_[handler]] = handler;
    }
  }
  for (std::map<int, ManifestHandler*>::iterator iter =
           handlers_by_priority.begin();
       iter != handlers_by_priority.end(); ++iter) {
    if (!(iter->second)->Parse(extension, error))
      return false;
  }
  return true;
}

void ManifestHandlerRegistry::ClearForTesting() {
  priority_map_.clear();
  handlers_.clear();
  is_sorted_ = false;
}

void ManifestHandlerRegistry::SortManifestHandlers() {
  if (is_sorted_)
    return;

  // Forget our existing sort order.
  priority_map_.clear();
  std::set<ManifestHandler*> unsorted_handlers;
  for (ManifestHandlerMap::const_iterator iter = handlers_.begin();
       iter != handlers_.end(); ++iter) {
    unsorted_handlers.insert(iter->second.get());
  }

  int priority = 0;
  while (true) {
    std::set<ManifestHandler*> next_unsorted_handlers;
    for (std::set<ManifestHandler*>::const_iterator iter =
             unsorted_handlers.begin();
         iter != unsorted_handlers.end(); ++iter) {
      ManifestHandler* handler = *iter;
      const std::vector<std::string>& prerequisites =
          handler->PrerequisiteKeys();
      int unsatisfied = prerequisites.size();
      for (size_t i = 0; i < prerequisites.size(); ++i) {
        ManifestHandlerMap::const_iterator prereq_iter =
            handlers_.find(prerequisites[i]);
        // If the prerequisite does not exist, crash.
        CHECK(prereq_iter != handlers_.end())
            << "Extension manifest handler depends on unrecognized key "
            << prerequisites[i];
        // Prerequisite is in our map.
        if (ContainsKey(priority_map_, prereq_iter->second.get()))
          unsatisfied--;
      }
      if (unsatisfied == 0) {
        priority_map_[handler] = priority;
        priority++;
      } else {
        // Put in the list for next time.
        next_unsorted_handlers.insert(handler);
      }
    }
    if (next_unsorted_handlers.size() == unsorted_handlers.size())
      break;
    unsorted_handlers.swap(next_unsorted_handlers);
  }

  // If there are any leftover unsorted handlers, they must have had
  // circular dependencies.
  CHECK(unsorted_handlers.size() == 0) << "Extension manifest handlers have "
                                       << "circular dependencies!";

  is_sorted_ = true;
}

static base::LazyInstance<ManifestHandlerRegistry> g_registry =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ManifestHandler::ManifestHandler() {
}

ManifestHandler::~ManifestHandler() {
}

bool ManifestHandler::AlwaysParseForType(Manifest::Type type) const {
  return false;
}

const std::vector<std::string> ManifestHandler::PrerequisiteKeys() const {
  return std::vector<std::string>();
}

void ManifestHandler::Register() {
  linked_ptr<ManifestHandler> this_linked(this);
  const std::vector<std::string> keys = Keys();
  for (size_t i = 0; i < keys.size(); ++i)
    g_registry.Get().RegisterManifestHandler(keys[i], this_linked);
}

// static
void ManifestHandler::ClearRegistryForTesting() {
  g_registry.Get().ClearForTesting();
}

// static
bool ManifestHandler::ParseExtension(Extension* extension, string16* error) {
  return g_registry.Get().ParseExtension(extension, error);
}

// static
const std::vector<std::string> ManifestHandler::SingleKey(
    const std::string& key) {
  return std::vector<std::string>(1, key);
}

}  // namespace extensions
