// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handler.h"

#include <map>

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/common/extensions/extension.h"

namespace extensions {

namespace {

static base::LazyInstance<ManifestHandlerRegistry> g_registry =
    LAZY_INSTANCE_INITIALIZER;
static ManifestHandlerRegistry* g_registry_override = NULL;

ManifestHandlerRegistry* GetRegistry() {
  if (!g_registry_override)
    return g_registry.Pointer();
  return g_registry_override;
}

}  // namespace

ManifestHandler::ManifestHandler() {
}

ManifestHandler::~ManifestHandler() {
}

bool ManifestHandler::Validate(const Extension* extension,
                               std::string* error,
                               std::vector<InstallWarning>* warnings) const {
  return true;
}

bool ManifestHandler::AlwaysParseForType(Manifest::Type type) const {
  return false;
}

bool ManifestHandler::AlwaysValidateForType(Manifest::Type type) const {
  return false;
}

const std::vector<std::string> ManifestHandler::PrerequisiteKeys() const {
  return std::vector<std::string>();
}

void ManifestHandler::Register() {
  linked_ptr<ManifestHandler> this_linked(this);
  const std::vector<std::string> keys = Keys();
  for (size_t i = 0; i < keys.size(); ++i)
    GetRegistry()->RegisterManifestHandler(keys[i], this_linked);
}

// static
bool ManifestHandler::ParseExtension(Extension* extension, string16* error) {
  return GetRegistry()->ParseExtension(extension, error);
}

// static
bool ManifestHandler::ValidateExtension(const Extension* extension,
                                        std::string* error,
                                        std::vector<InstallWarning>* warnings) {
  return GetRegistry()->ValidateExtension(extension, error, warnings);
}

// static
const std::vector<std::string> ManifestHandler::SingleKey(
    const std::string& key) {
  return std::vector<std::string>(1, key);
}

ManifestHandlerRegistry::ManifestHandlerRegistry() : is_sorted_(false) {
}

ManifestHandlerRegistry::~ManifestHandlerRegistry() {
}

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

bool ManifestHandlerRegistry::ValidateExtension(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) {
  std::set<ManifestHandler*> handlers;
  for (ManifestHandlerMap::iterator iter = handlers_.begin();
       iter != handlers_.end(); ++iter) {
    ManifestHandler* handler = iter->second.get();
    if (extension->manifest()->HasPath(iter->first) ||
        handler->AlwaysValidateForType(extension->GetType())) {
      handlers.insert(handler);
    }
  }
  for (std::set<ManifestHandler*>::iterator iter = handlers.begin();
       iter != handlers.end(); ++iter) {
    if (!(*iter)->Validate(extension, error, warnings))
      return false;
  }
  return true;
}

// static
ManifestHandlerRegistry* ManifestHandlerRegistry::SetForTesting(
    ManifestHandlerRegistry* new_registry) {
  ManifestHandlerRegistry* old_registry = GetRegistry();
  if (new_registry != g_registry.Pointer())
    g_registry_override = new_registry;
  else
    g_registry_override = NULL;
  return old_registry;
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

}  // namespace extensions
