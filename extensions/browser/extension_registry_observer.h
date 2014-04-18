// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_REGISTRY_OBSERVER_H_
#define EXTENSIONS_BROWSER_EXTENSION_REGISTRY_OBSERVER_H_

#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
struct UnloadedExtensionInfo;

// Observer for ExtensionRegistry. Exists in a separate header file to reduce
// the include file burden for typical clients of ExtensionRegistry.
class ExtensionRegistryObserver {
 public:
  virtual ~ExtensionRegistryObserver() {}

  // Called after an extension is loaded. The extension will exclusively exist
  // in the enabled_extensions set of ExtensionRegistry.
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const Extension* extension) {}

  // Called after an extension is unloaded. The extension no longer exists in
  // any of the ExtensionRegistry sets (enabled, disabled, etc.).
  virtual void OnExtensionUnloaded(content::BrowserContext* browser_context,
                                   const Extension* extension,
                                   UnloadedExtensionInfo::Reason reason) {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_REGISTRY_OBSERVER_H_
