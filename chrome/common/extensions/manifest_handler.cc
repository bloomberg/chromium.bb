// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handler.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "chrome/common/extensions/manifest.h"

namespace extensions {

namespace {

class ManifestHandlerRegistry {
 public:
  void RegisterManifestHandler(const std::string& key,
                               ManifestHandler* handler);
  bool ParseExtension(Extension* extension, string16* error);

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

bool ManifestHandlerRegistry::ParseExtension(Extension* extension,
                                             string16* error) {
  std::set<ManifestHandler*> handler_set;
  for (ManifestHandlerMap::iterator iter = handlers_.begin();
       iter != handlers_.end(); ++iter) {
    ManifestHandler* handler = iter->second.get();
    if (extension->manifest()->HasPath(iter->first) ||
        handler->AlwaysParseForType(extension->GetType()))
      handler_set.insert(handler);
  }

  // TODO(yoz): Some handlers may depend on other handlers having already
  // parsed their keys. Reorder the handlers so that handlers needed earlier
  // come first in the returned container.
  for (std::set<ManifestHandler*>::iterator iter = handler_set.begin();
       iter != handler_set.end(); ++iter) {
    if (!(*iter)->Parse(extension, error))
      return false;
  }
  return true;
}

static base::LazyInstance<ManifestHandlerRegistry> g_registry =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ManifestHandler::ManifestHandler() {
}

ManifestHandler::~ManifestHandler() {
}

bool ManifestHandler::AlwaysParseForType(Manifest::Type type) {
  return false;
}

// static
void ManifestHandler::Register(const std::string& key,
                               ManifestHandler* handler) {
  g_registry.Get().RegisterManifestHandler(key, handler);
}

// static
bool ManifestHandler::ParseExtension(Extension* extension, string16* error) {
  return g_registry.Get().ParseExtension(extension, error);
}

}  // namespace extensions
