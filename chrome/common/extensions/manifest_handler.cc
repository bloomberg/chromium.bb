// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handler.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"

namespace extensions {

namespace {

class ManifestHandlerRegistry {
 public:
  void RegisterManifestHandler(const std::string& key,
                               ManifestHandler* handler);
  ManifestHandler* GetManifestHandler(const std::string& key);
  std::vector<std::string> GetManifestHandlerKeys();

 private:
  friend struct base::DefaultLazyInstanceTraits<ManifestHandlerRegistry>;
  typedef std::map<std::string, linked_ptr<ManifestHandler> >
      ManifestHandlerMap;

  ManifestHandlerMap handlers_;
};

void ManifestHandlerRegistry::RegisterManifestHandler(
    const std::string& key, ManifestHandler* handler) {
  handlers_[key] = make_linked_ptr(handler);
}

ManifestHandler* ManifestHandlerRegistry::GetManifestHandler(
    const std::string& key) {
  ManifestHandlerMap::iterator iter = handlers_.find(key);
  if (iter != handlers_.end())
    return iter->second.get();
  // TODO(yoz): The NOTREACHED only makes sense as long as
  // GetManifestHandlerKeys is how we're getting the available
  // manifest handlers.
  NOTREACHED();
  return NULL;
}

std::vector<std::string> ManifestHandlerRegistry::GetManifestHandlerKeys() {
  std::vector<std::string> keys;
  for (ManifestHandlerMap::iterator iter = handlers_.begin();
       iter != handlers_.end(); ++iter) {
    keys.push_back(iter->first);
  }
  return keys;
}

static base::LazyInstance<ManifestHandlerRegistry> g_registry =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ManifestHandler::ManifestHandler() {
}

ManifestHandler::~ManifestHandler() {
}

bool ManifestHandler::HasNoKey(Extension* extension, string16* error) {
  return true;
}

// static
void ManifestHandler::Register(const std::string& key,
                               ManifestHandler* handler) {
  g_registry.Get().RegisterManifestHandler(key, handler);
}

// static
ManifestHandler* ManifestHandler::Get(const std::string& key) {
  return g_registry.Get().GetManifestHandler(key);
}

// static
std::vector<std::string> ManifestHandler::GetKeys() {
  return g_registry.Get().GetManifestHandlerKeys();
}

}  // namespace extensions
